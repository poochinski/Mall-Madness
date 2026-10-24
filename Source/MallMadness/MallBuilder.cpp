#include "MallBuilder.h"

#include "MallMaterials.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/ConstructorHelpers.h"

AMallBuilder::AMallBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereM(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeM(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneM(TEXT("/Engine/BasicShapes/Plane.Plane"));
	CubeMesh = Cube.Object;
	CylMesh = Cylinder.Object;
	SphereMesh = SphereM.Object;
	ConeMesh = ConeM.Object;
	PlaneMesh = PlaneM.Object;
}

UInstancedStaticMeshComponent* AMallBuilder::GetISM(UStaticMesh* Mesh, FName Mat, bool bCollide, bool bShadow)
{
	const FString MatStr = Mat.ToString();
	if (MatStr.StartsWith(TEXT("glass")) || MatStr.StartsWith(TEXT("light_")) || MatStr.StartsWith(TEXT("screen")) || MatStr.StartsWith(TEXT("sign")))
	{
		bShadow = false;   // glazing and emitters never block light
	}
	const FName Key = *FString::Printf(TEXT("%s_%s_%d%d"), *Mesh->GetName(), *MatStr, bCollide ? 1 : 0, bShadow ? 1 : 0);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = ISMs.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, Key);
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMaterial(0, UMallMaterials::Get()->Mat(Mat));
	ISM->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	ISM->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	ISM->SetCastShadow(bShadow);
	ISM->SetCanEverAffectNavigation(false);
	ISM->SetupAttachment(RootComponent);
	ISM->RegisterComponent();
	ISMs.Add(Key, ISM);
	return ISM;
}

void AMallBuilder::Box(const FVector& Center, const FVector& Size, FName Mat, bool bCollide, const FRotator& Rot, bool bShadow)
{
	GetISM(CubeMesh, Mat, bCollide, bShadow)->AddInstance(FTransform(Rot, Center, Size / 100.0), true);
}

void AMallBuilder::Cyl(const FVector& Center, float Radius, float Height, FName Mat, bool bCollide, const FRotator& Rot)
{
	GetISM(CylMesh, Mat, bCollide, true)->AddInstance(FTransform(Rot, Center, FVector(Radius / 50.0, Radius / 50.0, Height / 100.0)), true);
}

void AMallBuilder::Sphere(const FVector& Center, const FVector& Size, FName Mat, bool bCollide)
{
	GetISM(SphereMesh, Mat, bCollide, true)->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.0), true);
}

void AMallBuilder::Cone(const FVector& Center, float Radius, float Height, FName Mat, const FRotator& Rot)
{
	GetISM(ConeMesh, Mat, false, true)->AddInstance(FTransform(Rot, Center, FVector(Radius / 50.0, Radius / 50.0, Height / 100.0)), true);
}

void AMallBuilder::Collider(const FVector& Center, const FVector& Size, const FRotator& Rot, bool bBlockShots)
{
	const FName Key = bBlockShots ? FName("Collider_Shots") : FName("Collider_Pawn");
	UInstancedStaticMeshComponent* ISM = nullptr;
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = ISMs.Find(Key))
	{
		ISM = *Found;
	}
	else
	{
		ISM = NewObject<UInstancedStaticMeshComponent>(this, Key);
		ISM->SetMobility(EComponentMobility::Static);
		ISM->SetStaticMesh(CubeMesh);
		ISM->SetVisibility(false);
		ISM->SetHiddenInGame(true);
		ISM->SetCastShadow(false);
		ISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ISM->SetCollisionObjectType(ECC_WorldStatic);
		ISM->SetCollisionResponseToAllChannels(ECR_Block);
		if (!bBlockShots)
		{
			// Ramps and rail caps should stop bodies but let bullets and sight pass.
			ISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			ISM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		ISM->SetupAttachment(RootComponent);
		ISM->RegisterComponent();
		ISMs.Add(Key, ISM);
	}
	ISM->AddInstance(FTransform(Rot, Center, Size / 100.0), true);
}

UTextRenderComponent* AMallBuilder::Text(const FString& Str, const FVector& Pos, const FRotator& Rot, float Height, const FColor& Color, bool bCenter)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetMobility(EComponentMobility::Static);
	T->SetText(FText::FromString(Str));
	T->SetWorldSize(Height);
	T->SetTextRenderColor(Color);
	T->SetHorizontalAlignment(bCenter ? EHTA_Center : EHTA_Left);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->SetCastShadow(false);
	T->SetupAttachment(RootComponent);
	T->SetWorldLocationAndRotation(Pos, Rot);
	T->RegisterComponent();
	return T;
}

UPointLightComponent* AMallBuilder::Point(const FVector& Pos, const FLinearColor& Color, float Intensity, float Radius, bool bShadow)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(bShadow);
	L->SetSourceRadius(10.f);
	L->SetupAttachment(RootComponent);
	L->SetWorldLocation(Pos);
	L->RegisterComponent();
	++LightCount;
	return L;
}

URectLightComponent* AMallBuilder::Rect(const FVector& Pos, const FRotator& Rot, const FLinearColor& Color, float Intensity, float Radius, float W, float H, bool bShadow)
{
	URectLightComponent* L = NewObject<URectLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetSourceWidth(W);
	L->SetSourceHeight(H);
	L->SetCastShadows(bShadow);
	L->SetupAttachment(RootComponent);
	L->SetWorldLocationAndRotation(Pos, Rot);
	L->RegisterComponent();
	++LightCount;
	return L;
}

UStaticMeshComponent* AMallBuilder::Panel(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(CubeMesh);
	C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetupAttachment(RootComponent);
	C->SetWorldTransform(FTransform(Rot, Center, Size / 100.0));
	C->RegisterComponent();
	return C;
}

void AMallBuilder::WallX(double X0, double X1, double Y, double Z0, double Z1, double T, FName Mat, const TArray<FVector2D>& Openings, double OpenTop)
{
	// Openings are (start, end) along X in metres; above each opening a lintel fills to Z1.
	TArray<FVector2D> Ops = Openings;
	Ops.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	double Cursor = X0;
	for (const FVector2D& O : Ops)
	{
		if (O.X > Cursor)
		{
			Box(M((Cursor + O.X) * 0.5, Y, (Z0 + Z1) * 0.5), M(O.X - Cursor, T, Z1 - Z0), Mat);
		}
		if (Z1 > Z0 + OpenTop)
		{
			Box(M((O.X + O.Y) * 0.5, Y, (Z0 + OpenTop + Z1) * 0.5), M(O.Y - O.X, T, Z1 - Z0 - OpenTop), Mat);
		}
		Cursor = O.Y;
	}
	if (X1 > Cursor)
	{
		Box(M((Cursor + X1) * 0.5, Y, (Z0 + Z1) * 0.5), M(X1 - Cursor, T, Z1 - Z0), Mat);
	}
}

void AMallBuilder::WallY(double Y0, double Y1, double X, double Z0, double Z1, double T, FName Mat, const TArray<FVector2D>& Openings, double OpenTop)
{
	TArray<FVector2D> Ops = Openings;
	Ops.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	double Cursor = Y0;
	for (const FVector2D& O : Ops)
	{
		if (O.X > Cursor)
		{
			Box(M(X, (Cursor + O.X) * 0.5, (Z0 + Z1) * 0.5), M(T, O.X - Cursor, Z1 - Z0), Mat);
		}
		if (Z1 > Z0 + OpenTop)
		{
			Box(M(X, (O.X + O.Y) * 0.5, (Z0 + OpenTop + Z1) * 0.5), M(T, O.Y - O.X, Z1 - Z0 - OpenTop), Mat);
		}
		Cursor = O.Y;
	}
	if (Y1 > Cursor)
	{
		Box(M(X, (Cursor + Y1) * 0.5, (Z0 + Z1) * 0.5), M(T, Y1 - Cursor, Z1 - Z0), Mat);
	}
}

UTexture* AMallBuilder::MakeTextTexture(int32 W, int32 H, const FColor& Bg, const TArray<TPair<FString, FColor>>& Lines, float FontScale)
{
	UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(this, W, H, RTF_RGBA8, FLinearColor(Bg), false);
	if (!RT)
	{
		return nullptr;
	}
	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RT, Canvas, CanvasSize, Context);
	if (Canvas)
	{
		UFont* Font = GEngine->GetLargeFont();
		float Y = H * 0.18f;
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			const float Scale = FontScale * (I == 0 ? 1.f : 0.55f);
			float TW = 0, TH = 0;
			Canvas->StrLen(Font, Lines[I].Key, TW, TH);
			TW *= Scale;
			TH *= Scale;
			FCanvasTextItem Item(FVector2D((W - TW) * 0.5f, Y), FText::FromString(Lines[I].Key), Font, FLinearColor(Lines[I].Value));
			Item.Scale = FVector2D(Scale, Scale);
			Item.bOutlined = false;
			Canvas->DrawItem(Item);
			Y += TH * 1.15f;
		}
		// Thin frame line for a printed look.
		FCanvasBoxItem Frame(FVector2D(W * 0.04f, H * 0.04f), FVector2D(W * 0.92f, H * 0.92f));
		Frame.SetColor(FLinearColor(Lines.Num() ? Lines[0].Value : FColor::White) * 0.6f);
		Frame.LineThickness = 3.f;
		Canvas->DrawItem(Frame);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	return RT;
}
