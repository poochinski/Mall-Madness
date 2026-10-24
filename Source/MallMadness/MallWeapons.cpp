#include "MallWeapons.h"

#include "MallMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace
{
	TArray<FMallWeaponDef> MakeDefs()
	{
		TArray<FMallWeaponDef> D;
		D.SetNum(4);

		FMallWeaponDef& H = D[0];
		H.Name = TEXT("9mm Handgun"); H.AmmoName = TEXT("9mm");
		H.MagSize = 15; H.Damage = 34; H.FireInterval = 0.13f; H.bAutomatic = false;
		H.HipSpread = 1.4f; H.AdsSpread = 0.25f; H.MoveSpread = 1.6f;
		H.RecoilPitch = 1.5f; H.RecoilYaw = 0.35f; H.ReloadTime = 1.45f;
		H.AdsFov = 72; H.AdsTime = 0.15f; H.Range = 70; H.FalloffStart = 15; H.FalloffEnd = 40; H.MinDamageFrac = 0.55f;
		H.HeadMult = 2.2f; H.NoiseRadius = 26; H.Sound = "shot_pistol";

		FMallWeaponDef& A = D[1];
		A.Name = TEXT("AR15 Carbine"); A.AmmoName = TEXT("5.56");
		A.MagSize = 30; A.Damage = 29; A.FireInterval = 0.085f; A.bAutomatic = true;
		A.HipSpread = 2.4f; A.AdsSpread = 0.35f; A.MoveSpread = 2.2f;
		A.RecoilPitch = 0.75f; A.RecoilYaw = 0.4f; A.ReloadTime = 2.25f;
		A.AdsFov = 58; A.AdsTime = 0.2f; A.Range = 140; A.FalloffStart = 35; A.FalloffEnd = 90; A.MinDamageFrac = 0.6f;
		A.HeadMult = 2.0f; A.NoiseRadius = 38; A.Sound = "shot_ar";

		FMallWeaponDef& S = D[2];
		S.Name = TEXT("Pump Shotgun"); S.AmmoName = TEXT("12ga");
		S.MagSize = 8; S.Damage = 17; S.Pellets = 9; S.FireInterval = 0.9f; S.bAutomatic = false;
		S.HipSpread = 5.0f; S.AdsSpread = 3.6f; S.MoveSpread = 1.0f;
		S.RecoilPitch = 5.0f; S.RecoilYaw = 1.0f; S.bShellReload = true; S.ShellTime = 0.48f; S.ReloadTime = 0.5f;
		S.AdsFov = 76; S.AdsTime = 0.2f; S.Range = 40; S.FalloffStart = 6; S.FalloffEnd = 20; S.MinDamageFrac = 0.12f;
		S.HeadMult = 1.5f; S.NoiseRadius = 45; S.CycleTime = 0.55f; S.Sound = "shot_shotgun"; S.CycleSound = "pump";

		FMallWeaponDef& R = D[3];
		R.Name = TEXT("Bolt-Action Rifle"); R.AmmoName = TEXT(".308");
		R.MagSize = 5; R.Damage = 150; R.FireInterval = 1.35f; R.bAutomatic = false;
		R.HipSpread = 6.0f; R.AdsSpread = 0.0f; R.MoveSpread = 3.0f;
		R.RecoilPitch = 6.5f; R.RecoilYaw = 0.8f; R.ReloadTime = 3.0f;
		R.AdsFov = 16; R.AdsTime = 0.3f; R.Range = 300; R.FalloffStart = 300; R.FalloffEnd = 400; R.MinDamageFrac = 1.f;
		R.HeadMult = 3.0f; R.NoiseRadius = 60; R.CycleTime = 0.95f; R.bScope = true; R.Sound = "shot_sniper"; R.CycleSound = "bolt";
		return D;
	}

	struct FKit
	{
		AActor* Owner;
		UStaticMesh* Cube;
		UStaticMesh* Cyl;
		UStaticMesh* Sph;

		USceneComponent* Group(USceneComponent* Parent, const FVector& Rel)
		{
			USceneComponent* G = NewObject<USceneComponent>(Owner);
			G->SetupAttachment(Parent);
			G->SetRelativeLocation(Rel);
			G->RegisterComponent();
			return G;
		}

		UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Pos, const FVector& Size, FName Mat, const FRotator& Rot = FRotator::ZeroRotator)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
			C->SetStaticMesh(Mesh);
			C->SetMaterial(0, UMallMaterials::Get()->Mat(Mat));
			C->SetupAttachment(Parent);
			C->SetRelativeTransform(FTransform(Rot, Pos, Size / 100.0));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
			C->bReceivesDecals = false;
			C->RegisterComponent();
			return C;
		}

		UStaticMeshComponent* B(USceneComponent* P, const FVector& Pos, const FVector& Size, FName Mat, const FRotator& Rot = FRotator::ZeroRotator) { return Part(P, Cube, Pos, Size, Mat, Rot); }
		/** Cylinder whose axis runs along X. */
		UStaticMeshComponent* CX(USceneComponent* P, const FVector& Pos, float R, float Len, FName Mat) { return Part(P, Cyl, Pos, FVector(R * 2, R * 2, Len), Mat, FRotator(90, 0, 0)); }
		UStaticMeshComponent* S(USceneComponent* P, const FVector& Pos, const FVector& Size, FName Mat) { return Part(P, Sph, Pos, Size, Mat); }

		/** Gloved hand plus sleeved forearm reaching back out of frame. */
		void Arm(USceneComponent* P, const FVector& Hand, const FVector& Elbow, const FRotator& HandRot)
		{
			B(P, Hand, FVector(8.5f, 4.2f, 8.f), "glove", HandRot);
			const FVector Dir = (Elbow - Hand).GetSafeNormal();
			const float Len = FVector::Dist(Hand, Elbow);
			const FVector Mid = (Hand + Elbow) * 0.5f;
			const FRotator Rot = FRotationMatrix::MakeFromZ(Dir).Rotator();
			Part(P, Cyl, Hand + Dir * 4.5f, FVector(7.5f, 7.5f, 8.f), "glove", Rot);
			Part(P, Cyl, Mid + Dir * 4.f, FVector(9.5f, 9.5f, Len), "sleeve", Rot);
		}

		USceneComponent* Flash(USceneComponent* P, const FVector& At, float Scale)
		{
			USceneComponent* F = Group(P, At);
			S(F, FVector(1.5f, 0, 0) * Scale, FVector(5, 3.5f, 3.5f) * Scale, "muzzle");
			B(F, FVector(3.f, 0, 0) * Scale, FVector(7, 0.4f, 5) * Scale, "muzzle", FRotator(0, 0, 45));
			B(F, FVector(3.f, 0, 0) * Scale, FVector(7, 0.4f, 5) * Scale, "muzzle", FRotator(0, 0, -45));
			F->SetVisibility(false, true);
			return F;
		}
	};
}

const FMallWeaponDef& MallWeapons::Def(int32 Slot)
{
	static const TArray<FMallWeaponDef> Defs = MakeDefs();
	return Defs[FMath::Clamp(Slot, 0, 3)];
}

FMallWeaponModel MallWeapons::Build(int32 Slot, AActor* Owner, USceneComponent* Parent)
{
	FKit K{Owner,
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))};

	FMallWeaponModel W;
	W.Root = K.Group(Parent, FVector::ZeroVector);

	// Model space: X forward along the bore, Y right, Z up; the sight line sits on the X axis.
	if (Slot == 0)
	{
		W.Moving = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Moving, FVector(5.5f, 0, -2.3f), FVector(19.5f, 2.7f, 3.2f), "gun_metal");
		K.B(W.Moving, FVector(-2.f, 0, -1.2f), FVector(3.5f, 2.75f, 0.9f), "gun_steel");           // rear serrations
		K.B(W.Moving, FVector(14.3f, 0, -0.45f), FVector(0.6f, 0.5f, 0.8f), "gun_steel");          // front sight
		K.B(W.Moving, FVector(-3.6f, 0.7f, -0.45f), FVector(0.8f, 0.7f, 0.8f), "gun_steel");       // rear sight L/R
		K.B(W.Moving, FVector(-3.6f, -0.7f, -0.45f), FVector(0.8f, 0.7f, 0.8f), "gun_steel");
		K.B(W.Moving, FVector(3.5f, 1.38f, -2.0f), FVector(4.5f, 0.15f, 1.4f), "gun_steel");       // ejection port
		K.CX(W.Root, FVector(15.0f, 0, -2.5f), 0.55f, 1.5f, "gun_steel");
		K.B(W.Root, FVector(6.f, 0, -4.7f), FVector(16.f, 2.4f, 1.8f), "gun_polymer");
		K.B(W.Root, FVector(10.5f, 0, -5.8f), FVector(5.f, 2.f, 0.9f), "gun_polymer");              // accessory rail
		K.B(W.Root, FVector(-1.8f, 0, -9.8f), FVector(5.0f, 2.9f, 10.5f), "gun_polymer", FRotator(-16, 0, 0));
		K.B(W.Root, FVector(3.8f, 0, -7.6f), FVector(5.5f, 0.7f, 0.6f), "gun_polymer");             // trigger guard
		K.B(W.Root, FVector(6.5f, 0, -6.6f), FVector(0.6f, 0.7f, 2.3f), "gun_polymer");
		K.B(W.Root, FVector(2.6f, 0, -6.4f), FVector(0.5f, 0.4f, 1.8f), "gun_steel", FRotator(-10, 0, 0));
		W.Mag = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Mag, FVector(-3.3f, 0, -15.4f), FVector(4.6f, 2.8f, 0.9f), "gun_polymer", FRotator(-16, 0, 0));
		K.Arm(W.Root, FVector(-2.5f, 1.2f, -10.f), FVector(-34.f, 9.f, -30.f), FRotator(-16, 0, 0));
		K.Arm(W.Root, FVector(-1.f, -2.3f, -10.5f), FVector(-30.f, -16.f, -32.f), FRotator(-16, 0, -20));
		W.Muzzle = FVector(16.f, 0, -2.5f);
		W.HipOffset = FVector(24.f, 7.0f, -4.6f);
		W.AdsDist = 26.f;
		W.MovingTravel = 2.6f;
		W.Flash = K.Flash(W.Root, W.Muzzle, 0.8f);
	}
	else if (Slot == 1)
	{
		K.B(W.Root, FVector(0.f, 0, -4.3f), FVector(26.f, 2.7f, 4.3f), "gun_metal");               // upper
		K.B(W.Root, FVector(0.f, 0, -2.0f), FVector(26.f, 2.0f, 0.6f), "gun_metal");               // top rail
		for (int32 I = 0; I < 10; ++I)
		{
			K.B(W.Root, FVector(-11.f + I * 2.5f, 0, -1.55f), FVector(1.2f, 2.1f, 0.4f), "gun_metal");
		}
		// Red-dot optic, centred on the sight line.
		K.B(W.Root, FVector(2.f, 0, -1.f), FVector(4.f, 2.f, 1.4f), "gun_metal");
		K.CX(W.Root, FVector(2.f, 0, 0.f), 1.9f, 5.5f, "gun_metal");
		K.Part(W.Root, K.Cyl, FVector(4.8f, 0, 0), FVector(3.2f, 3.2f, 0.2f), "glass_dark", FRotator(90, 0, 0));
		K.S(W.Root, FVector(5.2f, 0, 0), FVector(0.12f, 0.12f, 0.12f), "light_red");
		K.B(W.Root, FVector(2.f, 2.05f, 0.4f), FVector(1.6f, 0.6f, 1.6f), "gun_metal");             // brightness knob
		K.B(W.Root, FVector(24.f, 0, -4.4f), FVector(24.f, 4.3f, 4.7f), "gun_polymer");             // handguard
		for (int32 I = 0; I < 4; ++I)
		{
			K.B(W.Root, FVector(16.f + I * 5.f, 2.17f, -4.4f), FVector(2.8f, 0.1f, 1.2f), "gun_metal");
			K.B(W.Root, FVector(16.f + I * 5.f, -2.17f, -4.4f), FVector(2.8f, 0.1f, 1.2f), "gun_metal");
		}
		K.CX(W.Root, FVector(44.f, 0, -4.4f), 0.8f, 16.f, "gun_metal");
		K.CX(W.Root, FVector(37.5f, 0, -4.4f), 1.2f, 2.f, "gun_metal");                            // gas block
		K.CX(W.Root, FVector(53.5f, 0, -4.4f), 1.15f, 4.f, "gun_metal");                           // flash hider
		K.B(W.Root, FVector(-1.f, 0, -8.2f), FVector(20.f, 2.5f, 3.8f), "gun_metal");              // lower
		K.B(W.Root, FVector(-12.f, 0, -2.1f), FVector(2.5f, 3.6f, 0.9f), "gun_metal");             // charging handle
		K.B(W.Root, FVector(-1.f, 1.38f, -4.f), FVector(6.f, 0.2f, 2.f), "gun_steel");             // dust cover
		K.B(W.Root, FVector(-8.5f, 0, -13.f), FVector(3.6f, 2.9f, 9.5f), "gun_polymer", FRotator(-20, 0, 0));
		K.B(W.Root, FVector(-3.5f, 0, -11.f), FVector(6.f, 0.7f, 0.6f), "gun_metal");               // trigger guard
		K.CX(W.Root, FVector(-21.f, 0, -4.6f), 1.5f, 17.f, "gun_metal");                           // buffer tube
		K.B(W.Root, FVector(-27.f, 0, -6.6f), FVector(15.f, 3.4f, 7.f), "gun_tan");                 // stock
		K.B(W.Root, FVector(-34.f, 0, -7.f), FVector(1.5f, 3.6f, 10.f), "gun_polymer");             // butt pad
		W.Mag = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Mag, FVector(4.f, 0, -14.f), FVector(6.6f, 2.3f, 10.f), "gun_tan", FRotator(8, 0, 0));
		K.B(W.Mag, FVector(5.8f, 0, -20.2f), FVector(6.4f, 2.3f, 4.f), "gun_tan", FRotator(18, 0, 0));
		W.Moving = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Moving, FVector(-1.f, 1.45f, -4.3f), FVector(3.5f, 0.2f, 1.2f), "gun_steel");        // bolt face visible in port
		K.Arm(W.Root, FVector(-8.5f, 1.4f, -12.5f), FVector(-36.f, 12.f, -30.f), FRotator(-20, 0, 0));
		K.Arm(W.Root, FVector(24.f, -2.6f, -7.f), FVector(-6.f, -22.f, -26.f), FRotator(0, 0, -30));
		W.Muzzle = FVector(56.f, 0, -4.4f);
		W.HipOffset = FVector(18.f, 9.0f, -7.4f);
		W.AdsDist = 13.f;
		W.MovingAxis = FVector(-1, 0, 0);
		W.MovingTravel = 3.f;
		W.Flash = K.Flash(W.Root, W.Muzzle, 1.f);
	}
	else if (Slot == 2)
	{
		K.B(W.Root, FVector(0.f, 0, -3.9f), FVector(21.f, 3.3f, 5.6f), "gun_metal");               // receiver
		K.B(W.Root, FVector(0.f, 0, -1.0f), FVector(21.f, 1.0f, 0.3f), "gun_steel");               // sight groove
		K.CX(W.Root, FVector(36.f, 0, -2.6f), 1.1f, 52.f, "gun_metal");                            // barrel
		K.CX(W.Root, FVector(31.f, 0, -5.1f), 1.05f, 42.f, "gun_metal");                           // magazine tube
		K.B(W.Root, FVector(52.f, 0, -3.9f), FVector(2.f, 1.4f, 3.f), "gun_metal");                // barrel clamp
		K.S(W.Root, FVector(61.5f, 0, -1.35f), FVector(0.5f, 0.5f, 0.5f), "brass");               // bead
		K.B(W.Root, FVector(2.f, 1.68f, -3.4f), FVector(6.f, 0.1f, 2.f), "gun_steel");             // ejection port
		K.B(W.Root, FVector(-2.f, 0, -7.6f), FVector(6.5f, 0.7f, 0.6f), "gun_metal");              // trigger guard
		K.B(W.Root, FVector(-14.f, 0, -6.4f), FVector(12.f, 3.3f, 5.f), "gun_wood", FRotator(-12, 0, 0));
		K.B(W.Root, FVector(-29.f, 0, -8.8f), FVector(22.f, 3.7f, 10.f), "gun_wood", FRotator(-6, 0, 0));
		K.B(W.Root, FVector(-40.5f, 0, -10.f), FVector(1.5f, 3.9f, 11.f), "gun_polymer", FRotator(-6, 0, 0));
		W.Moving = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Moving, FVector(26.f, 0, -5.4f), FVector(17.f, 4.6f, 4.2f), "gun_wood");              // pump
		for (int32 I = 0; I < 6; ++I)
		{
			K.B(W.Moving, FVector(20.f + I * 2.4f, 0, -7.55f), FVector(1.f, 4.0f, 0.3f), "gun_wood");
		}
		W.Shell = K.Group(W.Root, FVector(4.f, 0, -9.f));
		K.CX(W.Shell, FVector(-3.f, 0, 0), 1.0f, 6.f, "prop_red");
		K.CX(W.Shell, FVector(0.4f, 0, 0), 1.05f, 1.2f, "brass");
		W.Shell->SetVisibility(false, true);
		K.Arm(W.Root, FVector(-13.f, 1.5f, -8.5f), FVector(-40.f, 12.f, -28.f), FRotator(-12, 0, 0));
		K.Arm(W.Moving, FVector(26.f, -2.9f, -6.5f), FVector(-4.f, -22.f, -26.f), FRotator(0, 0, -30));
		W.Muzzle = FVector(62.5f, 0, -2.6f);
		W.HipOffset = FVector(18.f, 9.0f, -7.0f);
		W.AdsDist = 18.f;
		W.MovingAxis = FVector(-1, 0, 0);
		W.MovingTravel = 9.f;
		W.Flash = K.Flash(W.Root, W.Muzzle, 1.5f);
	}
	else
	{
		K.B(W.Root, FVector(-4.f, 0, -8.4f), FVector(50.f, 3.8f, 6.f), "gun_olive");                // stock body
		K.B(W.Root, FVector(-9.f, 0, -4.6f), FVector(14.f, 3.2f, 2.f), "gun_olive");                // cheek rest
		K.B(W.Root, FVector(-35.f, 0, -10.5f), FVector(14.f, 3.9f, 13.f), "gun_olive");             // butt
		K.B(W.Root, FVector(-42.5f, 0, -10.5f), FVector(1.5f, 4.1f, 13.5f), "gun_polymer");
		K.B(W.Root, FVector(-11.f, 0, -13.f), FVector(5.f, 3.2f, 7.f), "gun_olive", FRotator(-18, 0, 0)); // grip
		K.B(W.Root, FVector(-4.f, 0, -12.2f), FVector(6.f, 0.7f, 0.6f), "gun_metal");
		K.CX(W.Root, FVector(6.f, 0, -5.4f), 1.7f, 25.f, "gun_metal");                             // receiver
		K.CX(W.Root, FVector(46.f, 0, -5.4f), 1.0f, 56.f, "gun_metal");                            // barrel
		K.CX(W.Root, FVector(75.5f, 0, -5.4f), 1.35f, 5.f, "gun_metal");                           // brake
		K.B(W.Root, FVector(75.5f, 0, -5.4f), FVector(1.f, 3.2f, 0.8f), "gun_metal");
		K.CX(W.Root, FVector(10.f, 0, 0.f), 1.35f, 30.f, "gun_metal");                             // scope tube
		K.CX(W.Root, FVector(26.f, 0, 0.f), 2.3f, 7.f, "gun_metal");                               // objective bell
		K.Part(W.Root, K.Cyl, FVector(29.6f, 0, 0), FVector(4.f, 4.f, 0.2f), "glass_dark", FRotator(90, 0, 0));
		K.CX(W.Root, FVector(-6.f, 0, 0.f), 2.0f, 6.f, "gun_metal");                               // eyepiece
		K.Part(W.Root, K.Cyl, FVector(10.f, 0, 2.f), FVector(2.4f, 2.4f, 2.4f), "gun_metal");      // elevation turret
		K.Part(W.Root, K.Cyl, FVector(10.f, 2.f, 0.f), FVector(2.4f, 2.4f, 2.4f), "gun_metal", FRotator(0, 0, 90));
		K.B(W.Root, FVector(1.f, 0, -2.6f), FVector(2.f, 2.6f, 3.4f), "gun_metal");                // rings
		K.B(W.Root, FVector(17.f, 0, -2.6f), FVector(2.f, 2.6f, 3.4f), "gun_metal");
		K.CX(W.Root, FVector(30.f, 1.4f, -8.f), 0.35f, 16.f, "gun_metal");                         // folded bipod
		K.CX(W.Root, FVector(30.f, -1.4f, -8.f), 0.35f, 16.f, "gun_metal");
		W.Mag = K.Group(W.Root, FVector::ZeroVector);
		K.B(W.Mag, FVector(6.f, 0, -12.f), FVector(7.f, 2.8f, 4.5f), "gun_polymer");
		W.Moving = K.Group(W.Root, FVector(-4.f, 0, -5.4f));
		K.Part(W.Moving, K.Cyl, FVector(0, 2.8f, -0.6f), FVector(0.9f, 0.9f, 5.f), "gun_steel", FRotator(0, 0, 80));
		K.S(W.Moving, FVector(0, 5.4f, -1.2f), FVector(1.8f, 1.8f, 1.8f), "gun_steel");
		K.Arm(W.Root, FVector(-11.f, 1.6f, -12.f), FVector(-40.f, 13.f, -30.f), FRotator(-18, 0, 0));
		K.Arm(W.Root, FVector(28.f, -2.6f, -9.f), FVector(-2.f, -22.f, -28.f), FRotator(0, 0, -30));
		W.Muzzle = FVector(78.f, 0, -5.4f);
		W.HipOffset = FVector(18.f, 10.f, -8.0f);
		W.AdsDist = 9.f;
		W.MovingAxis = FVector(-1, 0, 0);
		W.MovingTravel = 7.f;
		W.Flash = K.Flash(W.Root, W.Muzzle, 1.3f);
	}
	return W;
}
