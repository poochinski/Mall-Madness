#include "MallFX.h"

#include "MallAudio.h"
#include "MallMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AMallFX::AMallFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AMallFX* AMallFX::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AMallFX> It(World);
	return It ? *It : nullptr;
}

UStaticMeshComponent* AMallFX::MakeComp(UStaticMesh* Mesh, FName Mat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, UMallMaterials::Get()->Mat(Mat));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(RootComponent);
	C->SetVisibility(false);
	C->bReceivesDecals = false;
	C->RegisterComponent();
	return C;
}

void AMallFX::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* HoleMat = UMallMaterials::Get()->Solid(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f);
	for (int32 I = 0; I < 80; ++I)
	{
		UStaticMeshComponent* H = MakeComp(Cyl, "plastic_black");
		H->SetMaterial(0, HoleMat);
		Holes.Add(H);
	}
	Debris.SetNum(90);
	for (int32 I = 0; I < Debris.Num(); ++I)
	{
		Debris[I].Comp = MakeComp(I % 3 == 0 ? Sph : Cube, "concrete");
	}
	Shells.SetNum(24);
	for (int32 I = 0; I < Shells.Num(); ++I)
	{
		Shells[I].Comp = MakeComp(Cyl, "brass");
		Shells[I].bShell = true;
	}
}

AMallFX::FParticle& AMallFX::NextParticle(TArray<FParticle>& Pool, int32& Cursor)
{
	FParticle& P = Pool[Cursor];
	Cursor = (Cursor + 1) % Pool.Num();
	return P;
}

void AMallFX::Impact(const FVector& Loc, const FVector& Normal, int32 Surface)
{
	UMallMaterials* Mats = UMallMaterials::Get();
	if (Surface == 0 && Holes.Num() > 0)
	{
		UStaticMeshComponent* H = Holes[HoleCursor];
		HoleCursor = (HoleCursor + 1) % Holes.Num();
		const float S = FMath::FRandRange(0.035f, 0.05f);
		H->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Normal).Rotator(), Loc + Normal * 0.15f, FVector(S, S, 0.002f)));
		H->SetVisibility(true);
	}
	const int32 Count = Surface == 1 ? 5 : 6;
	for (int32 I = 0; I < Count; ++I)
	{
		FParticle& P = NextParticle(Debris, DebrisCursor);
		P.Comp->SetMaterial(0, Surface == 1 ? Mats->Solid(FLinearColor(0.12f, 0.01f, 0.01f), 0.4f) : Mats->Mat(I % 2 ? "concrete" : "plaster"));
		P.Vel = (Normal + FMath::VRand() * 0.7f).GetSafeNormal() * FMath::FRandRange(150.f, 420.f);
		if (Surface == 1)
		{
			P.Vel *= 0.45f;
		}
		P.Spin = FMath::VRand() * 720.f;
		P.MaxLife = P.Life = FMath::FRandRange(0.35f, 0.8f);
		P.Size = Surface == 1 ? FMath::FRandRange(0.02f, 0.045f) : FMath::FRandRange(0.008f, 0.02f);
		P.FloorZ = -1e9f;
		P.Comp->SetWorldLocationAndRotation(Loc + Normal * 2.f, FMath::VRand().Rotation());
		P.Comp->SetWorldScale3D(FVector(P.Size));
		P.Comp->SetVisibility(true);
	}
}

void AMallFX::Shell(const FVector& Loc, const FVector& Vel, int32 Kind)
{
	FParticle& P = NextParticle(Shells, ShellCursor);
	P.Comp->SetMaterial(0, UMallMaterials::Get()->Mat(Kind == 1 ? "prop_red" : "brass"));
	P.Vel = Vel;
	P.Spin = FVector(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-1400.f, 1400.f), FMath::FRandRange(-300.f, 300.f));
	P.MaxLife = P.Life = 2.5f;
	P.Size = Kind == 1 ? 1.f : 0.6f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallShellFloor), false);
	P.FloorZ = GetWorld()->LineTraceSingleByObjectType(Hit, Loc, Loc - FVector(0, 0, 400.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Hit.ImpactPoint.Z + 0.6f : -1e9f;
	P.Comp->SetWorldLocationAndRotation(Loc, FRotator(0, 0, 90));
	P.Comp->SetWorldScale3D(Kind == 1 ? FVector(0.022f, 0.022f, 0.065f) : FVector(0.011f, 0.011f, 0.045f));
	P.Comp->SetVisibility(true);
}

void AMallFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	auto Update = [&](TArray<FParticle>& Pool)
	{
		for (FParticle& P : Pool)
		{
			if (P.Life <= 0.f)
			{
				continue;
			}
			P.Life -= Dt;
			if (P.Life <= 0.f)
			{
				P.Comp->SetVisibility(false);
				continue;
			}
			P.Vel.Z -= 980.f * Dt;
			FVector L = P.Comp->GetComponentLocation() + P.Vel * Dt;
			if (P.bShell && L.Z < P.FloorZ)
			{
				L.Z = P.FloorZ;
				if (P.Vel.Z < -120.f)
				{
					UMallAudio::Get()->PlayAt(GetWorld(), "shell_drop", L, 0.25f, FMath::FRandRange(0.9f, 1.3f), 900.f);
				}
				P.Vel.Z = -P.Vel.Z * 0.3f;
				P.Vel.X *= 0.5f;
				P.Vel.Y *= 0.5f;
				P.Spin *= 0.5f;
			}
			P.Comp->SetWorldLocationAndRotation(L, P.Comp->GetComponentRotation() + FRotator(P.Spin.X, P.Spin.Y, P.Spin.Z) * Dt);
			if (!P.bShell)
			{
				P.Comp->SetWorldScale3D(FVector(P.Size * FMath::Clamp(P.Life / P.MaxLife * 1.5f, 0.f, 1.f)));
			}
		}
	};
	Update(Debris);
	Update(Shells);
}
