#include "MallBuilder.h"

#include "MallInteractable.h"
#include "MallMaterials.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

// Layout constants (metres). Level 1 floor at 0, level 2 floor at 6.
namespace
{
	constexpr double F2 = 6.0;     // level 2 floor
	constexpr double C1 = 5.4;     // level 1 ceiling (underside of slab)
	constexpr double C2 = 11.0;    // level 2 ceiling
	constexpr double ROOF = 14.5;  // atrium skylight
	constexpr double VX = 17.0;    // atrium void half-length
	constexpr double VY = 5.0;     // atrium void half-width
	constexpr double CW = 9.0;     // concourse half-width
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

void AMallBuilder::NavRegion(double X0, double X1, double Y0, double Y1, double F, double Spacing)
{
	NavRegions.Add({FBox(M(X0, Y0, F), M(X1, Y1, F + 3.0)), Spacing});
}

void AMallBuilder::Spawn(double X, double Y, double F, bool bStart)
{
	(bStart ? StartZombies : ZombieSpawns).Add(M(X, Y, F));
}

void AMallBuilder::StoreShell(double X0, double X1, double Y0, double Y1, double F, FName Wall, FName Floor, double CeilH)
{
	const double MX = (X0 + X1) * 0.5, MY = (Y0 + Y1) * 0.5;
	Box(M(MX, MY, F + 0.006), M(X1 - X0, Y1 - Y0, 0.012), Floor, false);
	Box(M(X0 + 0.1, MY, F + 2.7), M(0.2, Y1 - Y0, 5.4), Wall);
	Box(M(X1 - 0.1, MY, F + 2.7), M(0.2, Y1 - Y0, 5.4), Wall);
	Box(M(MX, MY, F + CeilH + 0.05), M(X1 - X0, Y1 - Y0, 0.1), "ceiling", false);
}

void AMallBuilder::Railing(const FVector& A, const FVector& B)
{
	// Glass balustrade with a steel cap rail. Its blocker stops bodies but lets bullets and sight through.
	const FVector D = B - A;
	const double Len = D.Size();
	const FRotator Rot = D.Rotation();
	const FVector Mid = (A + B) * 0.5;
	Box(Mid + FVector(0, 0, 55), FVector(Len, 2.5, 100), "glass", false, Rot);
	Box(Mid + FVector(0, 0, 4), FVector(Len, 8, 8), "steel", false, Rot);
	Cyl(Mid + FVector(0, 0, 108), 3.5f, Len, "steel", false, Rot + FRotator(90, 0, 0));
	const int32 Posts = FMath::Max(1, int32(Len / 150.0));
	for (int32 I = 0; I <= Posts; ++I)
	{
		Box(A + D * (double(I) / Posts) + FVector(0, 0, 55), FVector(5, 5, 108), "steel", false, Rot);
	}
	Collider(Mid + FVector(0, 0, 95), FVector(Len, 20, 190), Rot, false);
}

void AMallBuilder::SlopedRail(const FVector& A, const FVector& B, bool bGlass)
{
	const FVector D = B - A;
	const double Len = D.Size();
	const FVector Dir = D / Len;
	const FRotator Rot = FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).Rotator();
	const FVector Mid = (A + B) * 0.5;
	if (bGlass)
	{
		Box(Mid + FVector(0, 0, 55), FVector(Len, 2.5, 95), "glass", false, Rot);
	}
	Cyl(Mid + FVector(0, 0, 105), 3.5f, Len, bGlass ? "steel" : "rubber", false, FRotationMatrix::MakeFromZ(Dir).Rotator());
	Box(Mid + FVector(0, 0, -10), FVector(Len, 10, 40), bGlass ? "bronze" : "steel", false, Rot);
	Collider(Mid + FVector(0, 0, 95), FVector(Len, 16, 200), Rot, false);
}

void AMallBuilder::Ramp(const FVector& Bottom, const FVector& Top, double Width)
{
	// Invisible walking surface laid over visible steps: no snagging, no camera jitter.
	const FVector D = Top - Bottom;
	const FVector Dir = D.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir.GetSafeNormal2D()).GetSafeNormal();
	const FRotator Rot = FRotationMatrix::MakeFromXY(Dir, Side).Rotator();
	const FVector Normal = FVector::CrossProduct(Dir, Side).GetSafeNormal() * (FVector::CrossProduct(Dir, Side).Z < 0 ? -1.0 : 1.0);
	Collider((Bottom + Top) * 0.5 - Normal * 5.0, FVector(D.Size(), Width, 10.0), Rot, false);
}

void AMallBuilder::HangingSign(const FVector& P, float Yaw, const FString& Line1, float W)
{
	const FRotator R(0, Yaw, 0);
	Box(P, FVector(10, W * 100.f, 45), "plaster_navy", false, R);
	Box(P + FVector(0, 0, 24), FVector(12, W * 100.f + 2, 3), "bronze", false, R);
	for (float S : {-0.4f, 0.4f})
	{
		Cyl(P + R.RotateVector(FVector(0, S * W * 100.f, 0)) + FVector(0, 0, 55), 0.6f, 70.f, "steel_dark");
	}
	Text(Line1, P + R.RotateVector(FVector(5.5f, 0, 0)), R, 24.f, FColor(240, 232, 210));
	Text(Line1, P + R.RotateVector(FVector(-5.5f, 0, 0)), R + FRotator(0, 180, 0), 24.f, FColor(240, 232, 210));
}

void AMallBuilder::GunProp(AMallInteractable* I, int32 Slot, const FVector& Rel, const FRotator& Rot)
{
	UMallMaterials* Mt = UMallMaterials::Get();
	auto P = [&](UStaticMesh* Mesh, const FVector& Off, const FVector& Size, FName Mat, const FRotator& R = FRotator::ZeroRotator)
	{
		I->AddPart(Mesh, Rel + Rot.RotateVector(Off), Size, Mt->Mat(Mat), Rot + R);
	};
	const FRotator Along(90, 0, 0);
	if (Slot == 1)
	{
		P(CubeMesh, FVector(0, 0, 0), FVector(26, 3, 9), "gun_metal");
		P(CubeMesh, FVector(25, 0, 0), FVector(24, 4.5, 5), "gun_polymer");
		P(CylMesh, FVector(44, 0, 0), FVector(1.6, 1.6, 16), "gun_metal", Along);
		P(CubeMesh, FVector(-24, 0, -2), FVector(22, 3.4, 9), "gun_tan");
		P(CubeMesh, FVector(4, 0, -11), FVector(7, 2.5, 14), "gun_tan", FRotator(10, 0, 0));
		P(CubeMesh, FVector(-8, 0, -9), FVector(4, 3, 9), "gun_polymer", FRotator(-20, 0, 0));
		P(CylMesh, FVector(2, 0, 7), FVector(3.6, 3.6, 6), "gun_metal", Along);
	}
	else if (Slot == 2)
	{
		P(CubeMesh, FVector(0, 0, 0), FVector(22, 3.4, 8), "gun_metal");
		P(CylMesh, FVector(36, 0, 2), FVector(2.2, 2.2, 52), "gun_metal", Along);
		P(CylMesh, FVector(31, 0, -1), FVector(2.1, 2.1, 42), "gun_metal", Along);
		P(CubeMesh, FVector(26, 0, -1.5), FVector(17, 4.6, 4.6), "gun_wood");
		P(CubeMesh, FVector(-24, 0, -4), FVector(30, 3.6, 11), "gun_wood", FRotator(-6, 0, 0));
	}
	else
	{
		P(CubeMesh, FVector(-6, 0, -3), FVector(62, 3.8, 7), "gun_olive");
		P(CubeMesh, FVector(-34, 0, -6), FVector(14, 3.9, 13), "gun_olive");
		P(CylMesh, FVector(42, 0, 0), FVector(2.0, 2.0, 60), "gun_metal", Along);
		P(CylMesh, FVector(6, 0, 6), FVector(2.8, 2.8, 32), "gun_metal", Along);
		P(CylMesh, FVector(22, 0, 6), FVector(4.6, 4.6, 7), "gun_metal", Along);
		P(CubeMesh, FVector(2, 3, 0), FVector(1.5, 5, 1.5), "gun_steel");
	}
}

AMallInteractable* AMallBuilder::Loot(uint8 ItemByte, int32 Amount, const FVector& At, float Yaw, const FString& Prompt, const FString& Label)
{
	const EMallItem Item = EMallItem(ItemByte);
	FActorSpawnParameters Params;
	AMallInteractable* I = GetWorld()->SpawnActor<AMallInteractable>(AMallInteractable::StaticClass(), FTransform(FRotator(0, Yaw, 0), At), Params);
	I->Item = Item;
	I->Amount = Amount;
	I->Prompt = Prompt;
	I->Label = Label;
	UMallMaterials* Mt = UMallMaterials::Get();
	auto P = [&](UStaticMesh* Mesh, const FVector& Rel, const FVector& Size, FName Mat, const FRotator& R = FRotator::ZeroRotator) { return I->AddPart(Mesh, Rel, Size, Mt->Mat(Mat), R); };
	FVector Glint(0, 0, 10);

	switch (Item)
	{
	case EMallItem::AmmoPistol:
		P(CubeMesh, FVector(0, 0, 3), FVector(14, 9, 6), "prop_dark");
		P(CubeMesh, FVector(0, 0, 6.2), FVector(14.2, 9.2, 0.6), "prop_orange");
		Glint = FVector(0, 0, 8);
		break;
	case EMallItem::AmmoRifle:
		P(CubeMesh, FVector(0, 0, 9), FVector(28, 11, 18), "prop_olive");
		P(CubeMesh, FVector(0, 0, 18.6), FVector(12, 2.5, 1.5), "gun_metal");
		P(CubeMesh, FVector(0, 5.6, 10), FVector(20, 0.3, 6), "prop_dark");
		Glint = FVector(0, 0, 21);
		break;
	case EMallItem::AmmoShells:
		P(CubeMesh, FVector(0, 0, 3.5), FVector(13, 8, 7), "prop_red");
		P(CubeMesh, FVector(0, 0, 7.2), FVector(13.2, 8.2, 0.6), "prop_white");
		P(CylMesh, FVector(10, 0, 1.2), FVector(2.2, 2.2, 6.5), "prop_red", FRotator(90, 30, 0));
		Glint = FVector(0, 0, 9);
		break;
	case EMallItem::AmmoSniper:
		P(CubeMesh, FVector(0, 0, 2.5), FVector(12, 7, 5), "prop_green");
		for (int32 K = 0; K < 3; ++K)
		{
			P(CylMesh, FVector(-4 + K * 4, 9, 0.8), FVector(1.2, 1.2, 7.5), "brass", FRotator(90, 0, 0));
		}
		Glint = FVector(0, 0, 7);
		break;
	case EMallItem::Medkit:
		P(CubeMesh, FVector(0, 0, 9), FVector(26, 11, 18), "prop_white");
		P(CubeMesh, FVector(0, 5.6, 9), FVector(10, 0.4, 3), "prop_red");
		P(CubeMesh, FVector(0, 5.6, 9), FVector(3, 0.4, 10), "prop_red");
		P(CubeMesh, FVector(0, 0, 18.6), FVector(10, 2, 1.5), "prop_dark");
		Glint = FVector(0, 0, 21);
		break;
	case EMallItem::Food:
		if (Label.Contains(TEXT("chips")) || Label.Contains(TEXT("bar")) || Label.Contains(TEXT("popcorn")))
		{
			P(CubeMesh, FVector(0, 0, 9), FVector(14, 6, 18), Label.Contains(TEXT("popcorn")) ? "prop_red" : "prop_orange");
			Glint = FVector(0, 0, 20);
		}
		else
		{
			P(CylMesh, FVector(0, 0, 6), FVector(6.6, 6.6, 12), Label.Contains(TEXT("water")) ? "prop_white" : (Label.Contains(TEXT("energy")) ? "prop_green" : "prop_red"));
			P(CylMesh, FVector(0, 0, 12.2), FVector(5.6, 5.6, 0.5), "gun_steel");
			Glint = FVector(0, 0, 15);
		}
		break;
	case EMallItem::WeaponAR:
	case EMallItem::WeaponShotgun:
	case EMallItem::WeaponSniper:
	{
		const int32 Slot = Item == EMallItem::WeaponAR ? 1 : (Item == EMallItem::WeaponShotgun ? 2 : 3);
		GunProp(I, Slot, FVector(0, 0, 4), FRotator(0, 0, 90));
		Glint = FVector(0, 0, 9);
		break;
	}
	case EMallItem::Fuse:
		P(CubeMesh, FVector(0, 0, 1.5), FVector(16, 10, 3), "prop_dark");
		P(CylMesh, FVector(0, 0, 4.5), FVector(3.2, 3.2, 10), "prop_white", FRotator(90, 0, 0));
		P(CylMesh, FVector(5.5, 0, 4.5), FVector(3.6, 3.6, 1.5), "brass", FRotator(90, 0, 0));
		P(CylMesh, FVector(-5.5, 0, 4.5), FVector(3.6, 3.6, 1.5), "brass", FRotator(90, 0, 0));
		Glint = FVector(0, 0, 9);
		break;
	case EMallItem::Container:
		if (Label.Contains(TEXT("duffel")))
		{
			P(CylMesh, FVector(0, 0, 16), FVector(32, 32, 62), Label.Contains(TEXT("green")) ? "prop_olive" : "prop_dark", FRotator(90, 0, 0));
			P(CubeMesh, FVector(0, 0, 32), FVector(30, 3, 2), "prop_dark");
			Glint = FVector(0, 0, 34);
		}
		else if (Label.Contains(TEXT("cabinet")))
		{
			P(CubeMesh, FVector(0, 0, 0), FVector(12, 40, 50), "prop_white");
			P(CubeMesh, FVector(6.2, 0, 0), FVector(0.5, 14, 4), "prop_red");
			P(CubeMesh, FVector(6.2, 0, 0), FVector(0.5, 4, 14), "prop_red");
			Glint = FVector(8, 0, 28);
		}
		else if (Label.Contains(TEXT("box")))
		{
			P(CubeMesh, FVector(0, 0, 18), FVector(45, 35, 36), "cardboard");
			P(CubeMesh, FVector(0, 0, 36.2), FVector(45, 6, 0.4), "plastic_yellow");
			Glint = FVector(0, 0, 38);
		}
		else if (Label.Contains(TEXT("case")))
		{
			P(CubeMesh, FVector(0, 0, 5), FVector(120, 38, 10), "plastic_black");
			P(CubeMesh, FVector(0, 0, 9.8), FVector(116, 34, 1), "fabric_grey");
			P(CubeMesh, FVector(0, -24, 22), FVector(120, 6, 38), "plastic_black", FRotator(0, 0, 20));
			Glint = FVector(0, 0, 14);
		}
		else
		{
			// Backpack / shopping bag
			P(CubeMesh, FVector(0, 0, 22), FVector(30, 18, 44), Label.Contains(TEXT("bag")) ? "cardboard" : "prop_red");
			P(CubeMesh, FVector(0, 10, 15), FVector(22, 4, 20), "prop_dark");
			Glint = FVector(0, 0, 46);
		}
		break;
	case EMallItem::Radio:
		P(CubeMesh, FVector(0, 0, 16), FVector(40, 70, 32), "painted_green");
		P(CubeMesh, FVector(20.5, 0, 18), FVector(1, 62, 24), "plastic_black");
		P(CubeMesh, FVector(21.2, -18, 24), FVector(0.5, 18, 7), "light_amber");
		for (int32 K = 0; K < 4; ++K)
		{
			P(CylMesh, FVector(22, 4 + K * 7, 12), FVector(4, 4, 2), "gun_steel", FRotator(90, 0, 0));
		}
		P(CubeMesh, FVector(10, 30, 34), FVector(8, 18, 5), "plastic_black");
		P(CylMesh, FVector(-12, -30, 60), FVector(1.2, 1.2, 60), "steel");
		I->StatusLight = NewObject<UPointLightComponent>(I);
		I->StatusLight->SetupAttachment(I->GetRootComponent());
		I->StatusLight->SetRelativeLocation(FVector(25, 0, 30));
		I->StatusLight->SetIntensity(120.f);
		I->StatusLight->SetLightColor(FLinearColor(1.f, 0.15f, 0.05f));
		I->StatusLight->SetAttenuationRadius(250.f);
		I->StatusLight->SetCastShadows(false);
		I->StatusLight->RegisterComponent();
		I->FocusOffset = FVector(0, 0, 20);
		I->bKeepAfterUse = true;
		I->UseTime = 2.0f;
		return Interactables.Add_GetRef(I);
	case EMallItem::Note:
		P(CubeMesh, FVector(0, 0, 0), FVector(2, 120, 80), "cardboard");
		P(CubeMesh, FVector(1.5, -25, 10), FVector(0.5, 21, 29.7), "paper", FRotator(0, 0, 3));
		P(CubeMesh, FVector(1.5, 25, 5), FVector(0.5, 21, 29.7), "paper", FRotator(0, 0, -4));
		P(CubeMesh, FVector(1.5, 0, -20), FVector(0.5, 40, 25), "prop_white");
		I->FocusOffset = FVector(0, 0, 0);
		I->bKeepAfterUse = true;
		return Interactables.Add_GetRef(I);
	case EMallItem::Exit:
		I->FocusOffset = FVector(0, 0, 150);
		I->bKeepAfterUse = true;
		return Interactables.Add_GetRef(I);
	default:
		break;
	}
	if (Item == EMallItem::Container)
	{
		I->bKeepAfterUse = true;
	}
	I->FocusOffset = Glint * 0.6f;
	I->AddGlint(Glint);
	return Interactables.Add_GetRef(I);
}

// ---------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------

void AMallBuilder::Build()
{
	const double T0 = FPlatformTime::Seconds();
	BuildShell();
	BuildAtrium();
	BuildVerticalRoutes();
	BuildConcourseDressing();
	BuildFoodCourt();
	BuildClothing();
	BuildSporting();
	BuildSecurity();
	BuildTheater();
	BuildPharmacy();
	BuildElectronics();
	BuildMaintenance();
	BuildClosedShops();
	BuildServiceAreas();
	BuildExterior();
	BuildLighting();
	PlaceLoot();
	BuildNavGraph();
	UE_LOG(LogTemp, Display, TEXT("MALL: build took %.2fs, %d ISM groups, %d lights"), FPlatformTime::Seconds() - T0, ISMs.Num(), LightCount);
}

void AMallBuilder::BuildShell()
{
	// Ground slab and the parking plaza outside.
	Box(M(1, 0, -0.15), M(92, 62, 0.3), "floor_main");
	Box(M(-60, 0, -0.16), M(30, 80, 0.3), "tarmac");
	Box(M(-47, 0, -0.05), M(6, 24, 0.12), "concrete");

	// Outer walls (two storeys + parapet).
	const double H = C2 + 0.6;
	Box(M(3, 28.3, H * 0.5), M(86.6, 0.6, H), "concrete");
	Box(M(3, -28.3, H * 0.5), M(86.6, 0.6, H), "concrete");
	Box(M(46.3, 0, H * 0.5), M(0.6, 57.2, H), "concrete");
	// West wall: vestibule opening on L1, tall windows on L2.
	WallY(-28.6, 28.6, -40.3, 0, H, 0.6, "concrete", {FVector2D(-5, 5)}, 5.4);
	Box(M(-40.3, 0, 8.3), M(0.25, 18, 4.6), "glass_sky", false);
	for (double Y = -9; Y <= 9.01; Y += 3)
	{
		Box(M(-40.2, Y, 8.3), M(0.3, 0.12, 4.6), "steel_dark");
	}

	// Level 2 slab with tiled top and ceiling underside, leaving the atrium void open.
	auto Slab = [this](double X0, double X1, double Y0, double Y1)
	{
		Box(M((X0 + X1) * 0.5, (Y0 + Y1) * 0.5, F2 - 0.05), M(X1 - X0, Y1 - Y0, 0.1), "floor_upper");
		Box(M((X0 + X1) * 0.5, (Y0 + Y1) * 0.5, (C1 + F2 - 0.1) * 0.5), M(X1 - X0, Y1 - Y0, F2 - 0.1 - C1), "ceiling");
	};
	Slab(-40, -VX, -28, 28);
	Slab(VX, 32, -28, 28);
	Slab(32, 46, -28, -10);
	Slab(32, 46, 10, 28);
	Slab(-VX, VX, VY, 28);
	Slab(-VX, VX, -28, -VY);
	// Slab edge fascia around the void.
	Box(M(0, VY - 0.08, 5.75), M(VX * 2, 0.16, 0.75), "plaster_white");
	Box(M(0, -VY + 0.08, 5.75), M(VX * 2, 0.16, 0.75), "plaster_white");
	Box(M(-VX + 0.08, 0, 5.75), M(0.16, VY * 2, 0.75), "plaster_white");
	Box(M(VX - 0.08, 0, 5.75), M(0.16, VY * 2, 0.75), "plaster_white");

	// Roof over level 2, raised clerestory and skylight over the atrium.
	auto Roof = [this](double X0, double X1, double Y0, double Y1)
	{
		Box(M((X0 + X1) * 0.5, (Y0 + Y1) * 0.5, C2 + 0.25), M(X1 - X0, Y1 - Y0, 0.5), "ceiling");
	};
	Roof(-40.3, -VX, -28.3, 28.3);
	Roof(VX, 46.3, -28.3, 28.3);
	Roof(-VX, VX, VY, 28.3);
	Roof(-VX, VX, -28.3, -VY);
	for (double S : {-1.0, 1.0})
	{
		// Clerestory glazing on the long sides, solid ends.
		Box(M(0, S * VY, (C2 + ROOF) * 0.5), M(VX * 2, 0.08, ROOF - C2), "glass_sky", false);
		for (double X = -VX; X <= VX + 0.01; X += 2.0)
		{
			Box(M(X, S * VY, (C2 + ROOF) * 0.5), M(0.12, 0.2, ROOF - C2), "steel_dark");
		}
		Box(M(S * VX, 0, (C2 + ROOF) * 0.5), M(0.3, VY * 2, ROOF - C2), "plaster_white");
		Box(M(0, S * VY, C2 + 0.3), M(VX * 2, 0.4, 0.6), "plaster_white");
	}
	Box(M(0, 0, ROOF + 0.05), M(VX * 2, VY * 2, 0.06), "glass_sky", false);
	for (double X = -VX + 1.5; X < VX; X += 3.0)
	{
		Box(M(X, 0, ROOF - 0.2), M(0.2, VY * 2, 0.4), "steel_dark");
		Box(M(X, 0, ROOF - 0.55), M(0.08, VY * 2 - 0.4, 0.08), "steel_dark");
	}
	for (double Y = -VY + 1.25; Y < VY; Y += 2.5)
	{
		Box(M(0, Y, ROOF - 0.08), M(VX * 2, 0.1, 0.12), "steel_dark");
	}

	// Level 2 ceiling soffit over walkways (bulkhead around the void).
	Box(M(0, VY + 0.3, C2 - 0.3), M(VX * 2, 0.6, 0.6), "plaster_white");
	Box(M(0, -VY - 0.3, C2 - 0.3), M(VX * 2, 0.6, 0.6), "plaster_white");

	// Main entrance vestibule.
	Box(M(-42, 5.15, 2.25), M(4.4, 0.3, 4.5), "stone_clad");
	Box(M(-42, -5.15, 2.25), M(4.4, 0.3, 4.5), "stone_clad");
	Box(M(-42, 0, 4.65), M(4.4, 10.6, 0.3), "ceiling");
	Box(M(-42, 0, 5.2), M(4.6, 10.6, 0.8), "concrete");
	Box(M(-42, 0, 0.005), M(4, 10, 0.01), "floor_vinyl", false);
	Box(M(-41.5, 0, 0.01), M(1.6, 4, 0.012), "carpet_grey", false);
	// Glass facade with door frames.
	Box(M(-44.1, 4.1, 2.25), M(0.1, 1.8, 4.5), "glass");
	Box(M(-44.1, -4.1, 2.25), M(0.1, 1.8, 4.5), "glass");
	Box(M(-44.1, 0, 3.9), M(0.1, 6.4, 1.2), "glass");
	for (double Y : {-5.0, -3.2, -1.6, 0.0, 1.6, 3.2, 5.0})
	{
		Box(M(-44.1, Y, 2.25), M(0.16, 0.1, 4.5), "bronze");
	}
	Box(M(-44.1, 0, 3.3), M(0.16, 6.4, 0.12), "bronze");
	for (double Y : {-2.4, -0.8, 0.8, 2.4})
	{
		Box(M(-44.12, Y, 1.6), M(0.06, 1.5, 3.2), "glass", true);
		Box(M(-44.05, Y + (Y > 0 ? -0.6 : 0.6), 1.1), M(0.08, 0.04, 0.6), "steel");
	}
	Text(TEXT("LARKSPUR GALLERIA"), M(-44.25, 0, 4.1), FRotator(0, 180, 0), 55.f, FColor(235, 220, 190));
	// Roll-down security shutter (opens when extraction arrives).
	ExitShutter = NewObject<UStaticMeshComponent>(this);
	ExitShutter->SetStaticMesh(CubeMesh);
	ExitShutter->SetMaterial(0, UMallMaterials::Get()->Mat("painted_metal"));
	ExitShutter->SetMobility(EComponentMobility::Movable);
	ExitShutter->SetupAttachment(RootComponent);
	ExitShutter->SetWorldTransform(FTransform(FRotator::ZeroRotator, M(-43.5, 0, 1.65), FVector(0.08, 6.6, 3.3)));
	ExitShutter->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ExitShutter->RegisterComponent();
	Box(M(-43.5, 0, 3.55), M(0.5, 6.8, 0.5), "steel_dark");
	Text(TEXT("EMERGENCY LOCKDOWN"), M(-43.43, 0, 2.4), FRotator(0, 0, 0), 26.f, FColor(220, 60, 40));
	ExitLocation = M(-42.6, 0, 0);
	PlayerStart = M(-40.5, 1.5, 0);
	PlayerStartRot = FRotator(0, -8, 0);
	NavRegion(-43, -40, -4, 4, 0, 2.0);
}

void AMallBuilder::BuildAtrium()
{
	// Dry fountain - the landmark at the centre of the atrium.
	const int32 Segs = 20;
	for (int32 I = 0; I < Segs; ++I)
	{
		const double A = 2 * PI * I / Segs;
		Box(M(FMath::Cos(A) * 3.25, FMath::Sin(A) * 3.25, 0.28), M(1.06, 0.36, 0.56), "stone_clad", true, FRotator(0, FMath::RadiansToDegrees(A) + 90, 0));
		Box(M(FMath::Cos(A) * 3.25, FMath::Sin(A) * 3.25, 0.58), M(1.1, 0.46, 0.05), "stone_clad", false, FRotator(0, FMath::RadiansToDegrees(A) + 90, 0));
	}
	Cyl(M(0, 0, 0.06), 310.f, 12.f, "concrete");
	Cyl(M(0, 0, 0.7), 70.f, 140.f, "stone_clad", true);
	Cyl(M(0, 0, 1.45), 140.f, 14.f, "bronze");
	Cyl(M(0, 0, 2.0), 22.f, 110.f, "bronze");
	Cyl(M(0, 0, 2.6), 60.f, 10.f, "bronze");
	Sphere(M(0, 0, 3.0), M(0.5, 0.5, 0.7), "bronze");
	Box(M(1.2, -0.9, 0.13), M(0.4, 0.3, 0.02), "paper", false, FRotator(0, 30, 0));
	ShoppingBag(M(-1.7, 1.2, 0.12), 40.f, "cardboard");
	Box(M(-0.6, 2.0, 0.12), M(0.25, 0.25, 0.02), "paper", false, FRotator(0, 70, 0));

	// Seating, planters and bins around the fountain.
	Bench(M(-6.4, 2.6, 0), 90.f);
	Bench(M(6.4, -2.6, 0), -90.f);
	Bench(M(-6.4, -2.6, 0), 90.f);
	Planter(M(6.5, 2.5, 0), 1.8f, 1.8f);
	TrashCan(M(-6.6, 0, 0));
	TrashCan(M(6.6, 0.2, 0));

	// Directory boards on both levels.
	const FColor Ink(235, 228, 210);
	UTexture* Dir1 = MakeTextTexture(512, 768, FColor(18, 26, 40), {
		{TEXT("DIRECTORY"), FColor(255, 196, 90)},
		{TEXT("YOU ARE HERE - LEVEL 1"), Ink},
		{TEXT("L1  Thread & Theory - Security"), Ink},
		{TEXT("L1  Summit Outfitters - Food Court"), Ink},
		{TEXT("L2  Voltline - CareWell Pharmacy"), Ink},
		{TEXT("L2  Marquee 6 Cinemas"), Ink},
		{TEXT("L2  Maintenance: east service hall"), Ink},
		{TEXT("STAIRS west  -  ESCALATORS east"), FColor(255, 196, 90)}}, 1.3f);
	for (const FVector& P : {M(-19.5, -3.0, 0), M(19.5, 3.0, 0)})
	{
		Box(P + M(0, 0, 1.15), M(0.25, 1.3, 2.3), "plaster_navy");
		Box(P + M(0, 0, 2.34), M(0.3, 1.36, 0.08), "bronze");
		if (Dir1)
		{
			Panel(P + M(0.13, 0, 1.3), M(0.01, 1.1, 1.6), UMallMaterials::Get()->MakeImageMaterial(Dir1, 1.4f));
			Panel(P + M(-0.13, 0, 1.3), M(0.01, 1.1, 1.6), UMallMaterials::Get()->MakeImageMaterial(Dir1, 1.4f), FRotator(0, 180, 0));
		}
	}

	// Suspended landmark sign hanging into the atrium.
	const FVector SignP = M(0, 0, 9.4);
	Box(SignP, M(9.0, 0.25, 1.3), "plaster_navy");
	Box(SignP + M(0, 0, 0.68), M(9.1, 0.3, 0.07), "bronze");
	Box(SignP - M(0, 0, 0.68), M(9.1, 0.3, 0.07), "bronze");
	for (double X : {-4.0, 4.0})
	{
		Cyl(SignP + M(X, 0, 2.6), 1.0f, 400.f, "steel_dark");
	}
	Text(TEXT("LARKSPUR GALLERIA"), SignP + M(0, 0.14, 0.05), FRotator(0, 90, 0), 75.f, FColor(250, 225, 170));
	Text(TEXT("LARKSPUR GALLERIA"), SignP - M(0, 0.14, -0.05), FRotator(0, -90, 0), 75.f, FColor(250, 225, 170));
	Text(TEXT("EST. 1994"), SignP + M(0, 0.14, -0.45), FRotator(0, 90, 0), 20.f, FColor(200, 180, 140));

	// Information kiosk east of the escalators.
	const FVector K = M(22, 0, 0);
	Cyl(K + M(0, 0, 0.53), 130.f, 106.f, "wood", true);
	Cyl(K + M(0, 0, 1.08), 140.f, 5.f, "laminate_white");
	Cyl(K + M(0, 0, 2.75), 170.f, 12.f, "plaster_navy");
	for (int32 I = 0; I < 4; ++I)
	{
		const double A = PI * 0.25 + I * PI * 0.5;
		Cyl(K + M(FMath::Cos(A) * 1.2, FMath::Sin(A) * 1.2, 1.9), 3.5f, 160.f, "steel");
	}
	Text(TEXT("INFORMATION"), K + M(-1.72, 0, 2.75), FRotator(0, 180, 0), 22.f, FColor(240, 230, 210));
	Text(TEXT("INFORMATION"), K + M(1.72, 0, 2.75), FRotator(0, 0, 0), 22.f, FColor(240, 230, 210));
	Screen(K + M(-0.6, 0.4, 1.25), FRotator(-15, 160, 0), 0.4f, 0.3f, "screen_blue");
	Papers(K + M(-1.6, 1.2, 0), 6, 0.6f, 77);

	NavRegion(-VX + 0.5, VX - 0.5, -CW + 0.8, CW - 0.8, 0, 3.0);
}

void AMallBuilder::BuildVerticalRoutes()
{
	// ---- West: broad stone staircase rising north from the concourse to the level 2 walkway ----
	{
		const double X0 = -15, X1 = -9, CX = -12, Y0 = -VY, Y1 = VY;
		const int32 N = 34;
		const double Rise = F2 / N, Run = (Y1 - Y0) / N;
		for (int32 I = 0; I < N; ++I)
		{
			const double Y = Y0 + (I + 0.5) * Run;
			const double Top = (I + 1) * Rise;
			Box(M(CX, Y, Top * 0.5), M(X1 - X0, Run + 0.005, Top), "stone_clad", true);
			Box(M(CX, Y - Run * 0.5 + 0.03, Top - 0.005), M(X1 - X0 - 0.1, 0.05, 0.02), "rubber", false);
		}
		Ramp(M(CX, Y0 - 0.25, -0.15), M(CX, Y1, F2), X1 - X0);
		for (double X : {X0 - 0.06, X1 + 0.06})
		{
			SlopedRail(M(X, Y0 - 0.3, 0), M(X, Y1, F2), true);
		}
		Text(TEXT("LEVEL 2"), M(CX, Y0 - 0.25, 0.6), FRotator(0, -90, 0), 22.f, FColor(230, 220, 200));
		StairBottom = M(CX, Y0 - 1.6, 0);
		StairTop = M(CX, Y1 + 1.4, F2);
	}

	// ---- East: pair of stopped escalators rising south to the level 2 walkway ----
	{
		const double X0 = 9, X1 = 13, CX = 11, YBase = VY, YTop = -VY;
		const int32 N = 29;
		const double Rise = F2 / N, Run = (YBase - YTop) / N;
		for (int32 I = 0; I < N; ++I)
		{
			const double Y = YBase - (I + 0.5) * Run;
			const double Top = (I + 1) * Rise;
			Box(M(CX, Y, Top * 0.5), M(X1 - X0, Run + 0.005, Top), "steel", true);
			for (double EX : {10.0, 12.0})
			{
				Box(M(EX, Y, Top - 0.01), M(1.0, Run - 0.03, 0.03), "steel_dark", false);
				Box(M(EX, Y + Run * 0.5 - 0.03, Top - 0.02), M(1.0, 0.04, 0.03), "painted_yellow", false);
			}
		}
		Box(M(CX, YBase + 0.6, 0.01), M(X1 - X0, 1.2, 0.02), "floor_plate", false);
		Box(M(CX, YTop - 0.6, F2 + 0.01), M(X1 - X0, 1.2, 0.02), "floor_plate", false);
		Ramp(M(CX, YBase + 0.25, -0.15), M(CX, YTop, F2), X1 - X0);
		SlopedRail(M(X0 - 0.06, YBase + 0.6, 0), M(X0 - 0.06, YTop, F2), true);
		SlopedRail(M(X1 + 0.06, YBase + 0.6, 0), M(X1 + 0.06, YTop, F2), true);
		// Centre balustrade between the two units (visual only so the ramp stays one surface).
		Box(M(CX, 0, 3.6), M(0.25, 11.6, 0.1), "steel", false, FRotator(0, 0, 0) + FRotationMatrix::MakeFromXY(FVector(1, 0, 0), FVector(0, -10, 6).GetSafeNormal()).Rotator() - FRotator(0, 0, 0));
		Box(M(CX, 0, 3.0), M(0.06, 11.6, 0.9), "glass", false, FRotationMatrix::MakeFromXY(FVector(1, 0, 0), FVector(0, -10, 6).GetSafeNormal()).Rotator());
		Text(TEXT("OUT OF SERVICE"), M(CX, YBase + 1.25, 1.1), FRotator(0, 90, 0), 18.f, FColor(230, 70, 50));
		Box(M(CX, YBase + 1.2, 0.55), M(1.6, 0.06, 0.06), "painted_yellow", false);
		EscBottom = M(CX, YBase + 1.6, 0);
		EscTop = M(CX, YTop - 1.5, F2);
	}

	// ---- Level 2 railings around the void (openings at the stair and escalator heads) ----
	const double RZ = F2;
	Railing(M(-VX, VY, RZ), M(-15.1, VY, RZ));
	Railing(M(-8.9, VY, RZ), M(VX, VY, RZ));
	Railing(M(-VX, -VY, RZ), M(8.9, -VY, RZ));
	Railing(M(13.1, -VY, RZ), M(VX, -VY, RZ));
	Railing(M(-VX, -VY, RZ), M(-VX, VY, RZ));
	Railing(M(VX, -VY, RZ), M(VX, VY, RZ));
}

void AMallBuilder::BuildConcourseDressing()
{
	// Level 1 concourse furniture away from storefront entrances.
	for (double X : {-34.0, -26.0, 27.0})
	{
		Bench(M(X, 0.6, 0), 180.f);
		Bench(M(X, -0.6, 0), 0.f);
		Planter(M(X + 2.6, 0, 0), 1.2f, 1.2f);
	}
	Planter(M(-30, 0, 0), 1.4f, 2.4f);
	for (double X : {-37.0, -21.0, 15.0, 24.5})
	{
		TrashCan(M(X, 7.6, 0));
	}
	TrashCan(M(-22, -7.6, 0));
	// Phone-accessory cart.
	Box(M(-22, 3.5, 0.5), M(2.4, 1.0, 1.0), "wood_dark", true);
	Box(M(-22, 3.5, 1.02), M(2.5, 1.1, 0.04), "laminate_white", false);
	for (double X : {-23.1, -20.9})
	{
		Cyl(M(X, 3.5, 1.8), 3.f, 160.f, "steel_dark");
	}
	Box(M(-22, 3.5, 2.62), M(2.8, 1.4, 0.06), "fabric_red", false);
	for (int32 I = 0; I < 6; ++I)
	{
		Box(M(-22.9 + I * 0.35, 3.2, 1.12), M(0.08, 0.16, 0.14), I % 2 ? "plastic_black" : "plastic_white", false);
	}
	Text(TEXT("CASE CLOSED"), M(-22, 2.79, 2.62), FRotator(0, -90, 0), 14.f, FColor(250, 240, 230));

	// Evacuation debris: dropped bags, luggage, papers, a toppled bin and a stroller.
	Suitcase(M(-28, -4.5, 0), 25.f, "plastic_blue", true);
	Suitcase(M(-15.5, -7.3, 0), 80.f, "fabric_red", false);
	ShoppingBag(M(-31, 4.8, 0), 15.f, "cardboard");
	ShoppingBag(M(-30.6, 5.2, 0), 70.f, "prop_white");
	ShoppingBag(M(4.5, 7.4, 0), -20.f, "plastic_red");
	Papers(M(-24, -2, 0), 14, 1.6f, 11);
	Papers(M(2, -6.5, 0), 10, 1.2f, 12);
	Papers(M(18, 6, 0), 8, 1.4f, 13);
	Cyl(M(-36.4, 5.9, 0.27), 26.f, 90.f, "steel", true, FRotator(90, 30, 0));
	Papers(M(-35.8, 5.5, 0), 6, 0.7f, 14);
	{
		const FVector S = M(-8, -7, 0);
		Box(S + M(0, 0, 0.55), M(0.7, 0.45, 0.35), "fabric_blue", false, FRotator(0, 20, 0));
		Box(S + M(-0.1, 0, 0.9), M(0.5, 0.45, 0.4), "fabric_blue", false, FRotator(-30, 20, 0));
		for (double WX : {-0.25, 0.25})
		{
			for (double WY : {-0.18, 0.18})
			{
				Cyl(S + FRotator(0, 20, 0).RotateVector(M(WX, WY, 0.1)), 9.f, 3.f, "plastic_black", false, FRotator(0, 20, 90));
			}
		}
	}

	// Level 2 walkway benches, planters and a few dropped items.
	for (double X : {-5.0, 4.0})
	{
		Bench(M(X, 7.9, F2), 180.f);
		Bench(M(X, -7.9, F2), 0.f);
	}
	Planter(M(0, 8.0, F2), 2.4f, 0.8f);
	Planter(M(0, -8.0, F2), 2.4f, 0.8f);
	for (double X : {-30.0, -22.0})
	{
		Bench(M(X, 0, F2), 90.f);
		Planter(M(X, 2.4, F2), 1.2f, 1.2f);
	}
	Bench(M(22, 0, F2), 90.f);
	TrashCan(M(-19, 7.5, F2));
	TrashCan(M(19, -7.5, F2));
	ShoppingBag(M(7, -6.8, F2), 50.f, "cardboard");
	Papers(M(-26, -3, F2), 10, 1.4f, 21);
	Suitcase(M(14.5, 7.4, F2), 10.f, "fabric_grey", true);

	// Wayfinding.
	HangingSign(M(-36, 0, 4.4), 90.f, TEXT("<  MAIN ENTRANCE          FOOD COURT  >"), 5.0f);
	HangingSign(M(-20, 0, 4.4), 90.f, TEXT("<  EXIT     STAIRS TO LEVEL 2  >"), 4.4f);
	HangingSign(M(24, 0, 4.4), 90.f, TEXT("<  ESCALATORS     FOOD COURT  >"), 4.4f);
	HangingSign(M(-24, 0, F2 + 4.0), 90.f, TEXT("<  VOLTLINE    PHARMACY    CINEMAS  >"), 5.2f);
	HangingSign(M(21, 0, F2 + 4.0), 90.f, TEXT("<  STAIRS        MARQUEE 6 CINEMAS  >"), 5.0f);
	HangingSign(M(12, 7.0, F2 + 4.0), 0.f, TEXT("MAINTENANCE / SERVICE  ^"), 3.0f);

	// Navigation coverage for the concourses.
	NavRegion(-40, -VX, -CW + 0.8, CW - 0.8, 0, 3.0);
	NavRegion(VX, 26, -CW + 0.8, CW - 0.8, 0, 3.0);
	NavRegion(-40, -VX, -CW + 0.8, CW - 0.8, F2, 3.0);
	NavRegion(VX, 26, -CW + 0.8, CW - 0.8, F2, 3.0);
	NavRegion(-VX, VX, VY + 0.6, CW - 0.6, F2, 2.5);
	NavRegion(-VX, VX, -CW + 0.6, -VY - 0.6, F2, 2.5);
	for (double X : {-36.0, -28.0, -20.0, 20.0, 24.0})
	{
		Spawn(X, 6.5, 0, true);
	}
	Spawn(-2, -7, 0, true);
	Spawn(5, 7.5, 0, true);
	Spawn(-30, -6, F2, true);
	Spawn(22, 5, F2, true);
	Spawn(0, 7, F2, true);
}

// ---------------------------------------------------------------------------------------------
// Ground floor locations
// ---------------------------------------------------------------------------------------------

void AMallBuilder::BuildFoodCourt()
{
	// Open dining hall at the east end with five counters and one enterable kitchen.
	const double X0 = 26, X1 = 42, Y0 = -20, Y1 = 20;
	Box(M(34, 0, 0.006), M(X1 - X0, Y1 - Y0, 0.012), "floor_food", false);
	Box(M(36, 0, 4.45), M(20, 40, 0.1), "ceiling", false);
	WallY(Y0, -CW, X0, 0, C1, 0.3, "plaster_warm");
	WallY(CW, Y1, X0, 0, C1, 0.3, "plaster_warm");
	Box(M(34, Y1 + 0.15, 2.7), M(16.3, 0.3, 5.4), "plaster_warm");
	Box(M(34, Y0 - 0.15, 2.7), M(16.3, 0.3, 5.4), "plaster_warm");
	Box(M(X0, 0, 4.9), M(0.4, CW * 2, 1.0), "plaster_navy");
	Text(TEXT("FOOD COURT"), M(X0 - 0.22, 0, 4.9), FRotator(0, 180, 0), 50.f, FColor(255, 214, 140));

	struct FCounter { double Y0, Y1; FString Name; FName Color; FColor Sign; };
	const FCounter Counters[] = {
		{-17.5, -11.0, TEXT("SEOUL BOWL"), "painted_red", FColor(255, 120, 90)},
		{-9.0, -4.0, TEXT("FIORE PIZZA"), "painted_green", FColor(140, 230, 140)},
		{-3.5, 3.5, TEXT("BIG SKY BURGERS"), "painted_yellow", FColor(255, 210, 90)},
		{4.0, 9.0, TEXT("CASA VERDE"), "painted_green", FColor(170, 240, 120)},
		{11.0, 17.5, TEXT("LEMON TREE"), "painted_yellow", FColor(255, 240, 120)}};
	for (const FCounter& C : Counters)
	{
		const double CY = (C.Y0 + C.Y1) * 0.5, W = C.Y1 - C.Y0;
		const bool bKitchen = C.Name.StartsWith(TEXT("BIG SKY"));
		// Counter front with a gap for staff on the kitchen unit.
		if (bKitchen)
		{
			Counter(M(40.2, CY - 1.4, 0), M(1.0, W - 2.8, 1.05), "steel", C.Color);
			Counter(M(40.2, C.Y1 - 0.4, 0), M(1.0, 0.8, 1.05), "steel", C.Color);
		}
		else
		{
			Counter(M(40.2, CY, 0), M(1.0, W, 1.05), "steel", C.Color);
		}
		Box(M(40.2, CY, 1.08), M(0.6, W - 0.6, 0.02), "glass", false);
		// Menu boards with emissive panels.
		UTexture* Menu = MakeTextTexture(512, 256, FColor(20, 18, 16), {{C.Name, C.Sign}, {TEXT("COMBO 1   $8.99"), FColor(230, 225, 210)}, {TEXT("COMBO 2   $10.49"), FColor(230, 225, 210)}}, 1.4f);
		if (Menu)
		{
			Panel(M(41.95, CY, 3.3), M(0.02, W - 0.8, 1.3), UMallMaterials::Get()->MakeImageMaterial(Menu, 1.6f), FRotator(0, 180, 0));
		}
		Box(M(42.05, CY, 4.05), M(0.2, W, 0.6), C.Color);
		Text(C.Name, M(41.93, CY, 4.05), FRotator(0, 180, 0), 34.f, C.Sign);
		if (!bKitchen)
		{
			Box(M(42.1, CY, 2.1), M(0.2, W, 4.2), "plaster_white");
			Box(M(41.5, CY, 1.4), M(0.9, W - 0.4, 0.04), "steel", false);
		}
		NormalLights.Add(Point(M(39.0, CY, 3.6), FLinearColor(1.f, 0.78f, 0.55f), 600.f, 900.f));
	}
	// Burger kitchen behind the central counter.
	{
		const double KY0 = -3.5, KY1 = 3.5;
		WallY(-20, KY0, 42.0, 0, 4.4, 0.2, "plaster_white");
		WallY(KY1, 20, 42.0, 0, 4.4, 0.2, "plaster_white");
		Box(M(44.1, KY0 - 0.1, 2.2), M(4.2, 0.2, 4.4), "plaster_white");
		Box(M(44.1, KY1 + 0.1, 2.2), M(4.2, 0.2, 4.4), "plaster_white");
		Box(M(44.1, 0, 0.007), M(4.2, 7, 0.014), "floor_vinyl", false);
		Box(M(44.1, 0, 3.25), M(4.2, 7, 0.1), "ceiling", false);
		Box(M(42.1, 0, 3.8), M(0.2, 7, 1.2), "plaster_white");   // lintel over the pass
		Counter(M(45.5, -1.5, 0), M(1.2, 3.0, 0.92), "steel", "steel");          // grill line
		Box(M(45.5, -1.5, 0.96), M(1.0, 2.6, 0.06), "steel_dark", false);
		Box(M(45.6, 1.8, 0.95), M(1.0, 1.4, 1.9), "steel", true);               // fridge
		Box(M(45.08, 1.8, 1.1), M(0.04, 0.06, 0.6), "steel_dark", false);
		Counter(M(43.6, 0.5, 0), M(1.0, 2.0, 0.92), "steel", "steel");           // prep table
		Box(M(45.7, -1.5, 2.3), M(1.0, 3.2, 0.6), "steel", false);               // hood
		for (int32 I = 0; I < 3; ++I)
		{
			Cyl(M(43.4 + I * 0.25, 0.2, 0.98), 12.f, 4.f, "steel_dark");
		}
		Tray(M(43.7, 0.9, 0.93), 10.f, 5);
		NormalLights.Add(Point(M(44.2, 0, 3.0), FLinearColor(0.9f, 0.95f, 1.f), 300.f, 600.f));
		NavRegion(42.4, 45.8, KY0 + 0.4, KY1 - 0.4, 0, 1.5);
		Spawn(45.6, 0.2, 0);
	}

	// Dining tables, chairs (some knocked over), trays and spills.
	FRandomStream R(404);
	for (double X = 28.5; X <= 37.0; X += 2.8)
	{
		for (double Y = -16.0; Y <= 16.1; Y += 3.2)
		{
			if (FMath::Abs(Y) < 1.0 && X < 31) continue;
			const bool bRound = (int32(X + Y) % 2) == 0;
			const float Yaw = R.FRandRange(-10.f, 10.f);
			if (R.FRand() < 0.12f)
			{
				// Overturned table.
				Box(M(X, Y, 0.4), M(0.8, 0.035, 0.8), "laminate_white", true, FRotator(0, Yaw, 0));
				Box(M(X + 0.35, Y, 0.38), M(0.7, 0.08, 0.08), "steel_dark", false, FRotator(0, Yaw, 0));
			}
			else
			{
				Table(M(X, Y, 0), bRound, Yaw);
				if (R.FRand() < 0.4f) Tray(M(X + R.FRandRange(-0.15f, 0.15f), Y, 0.76), R.FRandRange(0.f, 90.f), R.RandRange(0, 99));
			}
			for (int32 C = 0; C < 4; ++C)
			{
				if (R.FRand() < 0.2f) continue;
				const double A = C * PI * 0.5 + FMath::DegreesToRadians(Yaw);
				const FVector CP = M(X + FMath::Cos(A) * 0.75, Y + FMath::Sin(A) * 0.75, 0);
				Chair(CP, FMath::RadiansToDegrees(A) + 90.f + R.FRandRange(-20.f, 20.f), R.FRand() < 0.15f);
			}
		}
	}
	Tray(M(33.2, -6.1, 0.0), 30.f, 1);
	Tray(M(30.1, 9.4, 0.0), 120.f, 2);
	Box(M(31.5, 3.5, 0.003), M(1.4, 0.9, 0.004), "blood", false, FRotator(0, 20, 0), false);
	for (double X : {28.0, 31.0})
	{
		Vending(M(X, 19.3, 0), 180.f, X < 30 ? FName("plastic_red") : FName("plastic_blue"), "light_cool");
	}
	Vending(M(28.0, -19.3, 0), 0.f, "plastic_green", "light_cool");
	Planter(M(27.2, 12, 0), 1.2f, 2.0f);
	Planter(M(27.2, -12, 0), 1.2f, 2.0f);
	TrashCan(M(37.5, 18.6, 0));
	TrashCan(M(37.5, -18.6, 0));

	CeilingLights(FBox(M(27, -19, 0), M(38, 19, 0)), 4.4, 3.6, "light_warm", FLinearColor(1.f, 0.82f, 0.62f), 260.f, true);
	NavRegion(X0 + 0.6, 39.4, Y0 + 0.6, Y1 - 0.6, 0, 3.0);
	Spawn(38.5, 18.5, 0);
	Spawn(38.5, -18.5, 0);
	Spawn(33, 12, 0, true);
	Spawn(35, -10, 0, true);
	Spawn(30, -4, 0, true);
}

void AMallBuilder::BuildClothing()
{
	// THREAD & THEORY - L1 north-west. Sales floor, fitting rooms, mirrors and a stockroom.
	const double X0 = -40, X1 = -24, Y0 = CW, Y1 = 28, F = 0;
	StoreShell(X0, X1, Y0, Y1, F, "plaster_white", "floor_wood", 4.2);
	Storefront(X0, X1, Y0, F, true, TEXT("THREAD & THEORY"), "plaster_dark", FColor(250, 245, 235), 4.0, true);
	WallX(X0, X1, 23.0, F, 4.2, 0.2, "plaster_white", {FVector2D(-31.0, -29.4)}, 2.4);
	Text(TEXT("STOCKROOM"), M(-30.2, 22.88, 2.6), FRotator(0, -90, 0), 14.f, FColor(40, 40, 40));

	// Racks and display tables - medium height, leaving a clear loop.
	FRandomStream R(55);
	int32 Seed = 0;
	for (double X : {-37.5, -34.0, -30.5})
	{
		for (double Y : {12.5, 16.0, 19.5})
		{
			if (X == -34.0 && Y == 16.0)
			{
				// Folded-clothes table at the centre.
				Box(M(X, Y, 0.42), M(1.6, 1.0, 0.84), "wood_light");
				for (int32 I = 0; I < 6; ++I)
				{
					Box(M(X - 0.6 + (I % 3) * 0.6, Y - 0.25 + (I / 3) * 0.5, 0.88 + R.FRandRange(0.f, 0.06f)), M(0.4, 0.32, 0.08 + R.FRandRange(0.f, 0.08f)), I % 2 ? "fabric_beige" : "fabric_blue", false);
				}
				continue;
			}
			ClothesRack(M(X, Y, 0), (Seed % 2) ? 0.f : 90.f, 100 + Seed);
			++Seed;
		}
	}
	Mannequin(M(-38.5, 10.2, 0), -90.f, "fabric_red", "fabric_black", false);
	Mannequin(M(-25.6, 10.2, 0), -90.f, "fabric_beige", "fabric_blue", false);
	Mannequin(M(-27.2, 13.0, 0), 20.f, "fabric_green", "fabric_grey", true);
	// Wall shelving with folded stock along the west wall.
	for (double Y = 11.0; Y < 22.5; Y += 1.8)
	{
		Box(M(-39.7, Y, 1.2), M(0.45, 1.6, 0.03), "wood_light", false);
		Box(M(-39.7, Y, 1.7), M(0.45, 1.6, 0.03), "wood_light", false);
		Box(M(-39.65, Y, 1.27), M(0.35, 1.4, 0.12), (R.FRand() < 0.5f) ? FName("fabric_grey") : FName("fabric_red"), false);
		Box(M(-39.65, Y, 1.77), M(0.35, 1.4, 0.12), (R.FRand() < 0.5f) ? FName("fabric_beige") : FName("fabric_black"), false);
	}
	Collider(M(-39.7, 16.5, 1.0), M(0.5, 12, 2.0));
	// Checkout counter.
	Counter(M(-27.0, 21.2, 0), M(3.0, 0.9, 1.05), "wood", "plaster_dark");
	Box(M(-27.6, 21.2, 1.2), M(0.35, 0.3, 0.25), "plastic_black", false);
	ShoppingBag(M(-26.4, 21.2, 1.07), 0.f, "prop_white");
	// Fitting rooms along the east wall with curtains and a mirror wall.
	for (int32 I = 0; I < 3; ++I)
	{
		const double Y = 14.0 + I * 2.2;
		Box(M(-25.25, Y - 1.1, 1.2), M(1.3, 0.06, 2.4), "plaster_white");
		Box(M(-26.0, Y, 1.25), M(0.04, 1.8, 2.1), I == 1 ? "fabric_red" : "fabric_grey", false, FRotator(0, 0, 0));
		Box(M(-24.25, Y, 1.3), M(0.03, 1.4, 1.9), "chrome", false);
	}
	Box(M(-25.25, 20.6, 1.2), M(1.3, 0.06, 2.4), "plaster_white");
	Text(TEXT("FITTING ROOMS"), M(-26.1, 16.2, 2.7), FRotator(0, 180, 0), 14.f, FColor(40, 40, 40));
	Box(M(-39.88, 21.8, 1.3), M(0.03, 1.4, 2.0), "chrome", false);   // full-length mirror
	// Stockroom: boxes, a garment rail, dim light.
	Crate(M(-38.5, 25.0, 0), M(0.6, 0.5, 0.5), 10.f);
	Crate(M(-38.5, 25.0, 0.5), M(0.5, 0.45, 0.4), 30.f);
	Crate(M(-37.4, 26.8, 0), M(0.6, 0.6, 0.6), -5.f);
	Crate(M(-26.0, 26.5, 0), M(0.7, 0.5, 0.5), 15.f);
	ClothesRack(M(-33.0, 26.5, 0), 0.f, 77);
	Shelf(M(-30.0, 27.4, 0), 3.0f, 2.2f, 0.f, "steel_dark", 91, 0);

	CeilingLights(FBox(M(X0 + 1, Y0 + 1, 0), M(X1 - 1, 22.5, 0)), 4.2, 3.2, "light_cool", FLinearColor(1.f, 0.93f, 0.85f), 300.f, true);
	NormalLights.Add(Point(M(-32, 25.5, 3.4), FLinearColor(1.f, 0.9f, 0.75f), 150.f, 700.f));
	NavRegion(X0 + 0.6, X1 - 0.6, Y0 + 0.8, 22.4, F, 2.0);
	NavRegion(X0 + 0.6, X1 - 0.6, 23.5, Y1 - 0.6, F, 2.0);
	Spawn(-35, 26.4, 0);
	Spawn(-32, 18, 0, true);
}

void AMallBuilder::BuildSporting()
{
	// SUMMIT OUTFITTERS - L1 south-west. Long gondola aisles, camping, and the hunting counter.
	const double X0 = -40, X1 = -16, Y0 = -28, Y1 = -CW, F = 0;
	StoreShell(X0, X1, Y0, Y1, F, "plaster_olive", "floor_concrete", 4.4);
	Storefront(X0, X1, Y1, F, false, TEXT("SUMMIT OUTFITTERS"), "wood_dark", FColor(235, 200, 140), 5.0, true);
	// Back stock area.
	WallY(Y0, -23.0, -22.0, F, 4.4, 0.2, "plaster_olive", {FVector2D(-24.0, -23.0)}, 2.4);
	WallX(-22.0, X1, -23.0, F, 4.4, 0.2, "plaster_olive");
	// Two long gondolas forming an 18 m aisle that shows off the shotgun's falloff.
	Shelf(M(-28.5, -14.2, 0), 18.f, 1.9f, 0.f, "painted_metal", 201, 2);
	Shelf(M(-28.5, -18.6, 0), 18.f, 1.9f, 0.f, "painted_metal", 202, 2);
	Text(TEXT("CAMPING"), M(-35, -14.2, 2.3), FRotator(0, 90, 0), 20.f, FColor(240, 230, 210));
	Text(TEXT("FITNESS"), M(-24, -14.2, 2.3), FRotator(0, 90, 0), 20.f, FColor(240, 230, 210));
	Text(TEXT("OUTDOOR"), M(-30, -18.6, 2.3), FRotator(0, -90, 0), 20.f, FColor(240, 230, 210));
	// Camping display: tent, sleeping bags, lantern.
	{
		const FVector T = M(-36.8, -11.2, 0);
		Box(T + M(0, 0, 0.6), M(2.2, 1.6, 0.04), "fabric_green", false, FRotator(0, 0, 38));
		Box(T + M(0, 0, 0.6), M(2.2, 1.6, 0.04), "fabric_green", false, FRotator(0, 0, -38));
		Collider(T + M(0, 0, 0.6), M(2.2, 2.4, 1.2));
		Cyl(T + M(1.6, 0.4, 0.15), 15.f, 70.f, "fabric_red", false, FRotator(90, 0, 0));
		Cyl(T + M(1.7, -0.3, 0.15), 15.f, 70.f, "fabric_blue", false, FRotator(90, 0, 0));
	}
	// Bikes/kayak hint along the east wall and a weights rack.
	Box(M(-16.6, -13.5, 1.4), M(0.6, 3.2, 0.5), "plastic_orange", false, FRotator(0, 0, 0));
	Box(M(-17.2, -16.0, 0.4), M(0.9, 2.0, 0.8), "steel_dark", true);
	for (int32 I = 0; I < 4; ++I)
	{
		Cyl(M(-17.2, -16.8 + I * 0.5, 0.95), 14.f, 6.f, "plastic_black", false, FRotator(0, 0, 90));
	}
	// Hunting section: counter and locked rifle rack.
	Counter(M(-33.0, -23.8, 0), M(6.0, 0.9, 1.05), "wood", "wood_dark");
	Box(M(-33.0, -23.8, 1.08), M(5.6, 0.7, 0.02), "glass", false);
	Box(M(-33.0, -27.6, 1.7), M(7.0, 0.3, 2.6), "wood_dark");
	for (int32 I = 0; I < 6; ++I)
	{
		Box(M(-35.8 + I * 1.1, -27.35, 1.6), M(0.08, 0.06, 1.3), "gun_metal", false, FRotator(0, 0, 12));
	}
	Box(M(-33.0, -27.4, 2.95), M(7.0, 0.2, 0.5), "painted_red");
	Text(TEXT("HUNTING & SPORT"), M(-33, -27.28, 2.95), FRotator(0, 90, 0), 26.f, FColor(250, 240, 220));
	Shelf(M(-39.4, -21.5, 0), 4.f, 2.0f, 90.f, "wood_dark", 203, 2);
	// Stock area clutter.
	Crate(M(-20.5, -25.5, 0), M(0.8, 0.6, 0.6), 5.f);
	Crate(M(-18.0, -26.8, 0), M(0.6, 0.6, 0.5), 20.f);
	Crate(M(-18.0, -26.8, 0.5), M(0.5, 0.5, 0.4), 0.f);

	CeilingLights(FBox(M(X0 + 1, Y0 + 1, 0), M(X1 - 1, Y1 - 1, 0)), 4.4, 4.0, "light_warm", FLinearColor(1.f, 0.88f, 0.7f), 240.f, true);
	NavRegion(X0 + 0.6, X1 - 0.6, Y0 + 0.6, Y1 - 0.8, F, 2.0);
	Spawn(-19, -26, 0);
	Spawn(-26, -21, 0, true);
	Spawn(-36, -16.4, 0, true);
}

void AMallBuilder::BuildSecurity()
{
	// SECURITY OFFICE - L1 north, small and cluttered: monitors, lockers, desks, the AR15 locker.
	const double X0 = -24, X1 = -16, Y0 = CW, Y1 = 21, F = 0;
	StoreShell(X0, X1, Y0, Y1, F, "plaster", "carpet_grey", 3.4);
	WallX(X0, X1, Y1, F, C1, 0.3, "plaster");
	// Solid frontage with a glass door and a reception window.
	WallX(X0, X1, Y0, F, C1, 0.3, "plaster_navy", {FVector2D(-21.0, -19.4)}, 2.4);
	Box(M(-17.6, Y0, 1.6), M(2.4, 0.32, 1.2), "glass", true);
	Box(M(-20.2, Y0 - 0.17, 3.0), M(4.0, 0.04, 0.6), "plaster_navy", false);
	Text(TEXT("MALL SECURITY"), M(-20.2, Y0 - 0.2, 3.0), FRotator(0, -90, 0), 30.f, FColor(220, 230, 250));
	Text(TEXT("AUTHORISED PERSONNEL"), M(-20.2, Y0 - 0.2, 2.65), FRotator(0, -90, 0), 12.f, FColor(200, 205, 215));
	SecurityDoor = M(-20.2, Y0 - 1.0, 0);

	// Monitor wall and desk.
	Counter(M(-17.0, 15.5, 0), M(1.0, 4.0, 0.76), "laminate_white", "plaster_dark");
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			Screen(M(-16.18, 14.2 + Col * 1.3, 1.5 + Row * 0.8), FRotator(0, 180, 0), 1.15f, 0.7f, (Row + Col) % 3 == 0 ? FName("screen_static") : FName("screen_blue"));
		}
	}
	Box(M(-17.3, 15.0, 0.8), M(0.45, 0.2, 0.04), "plastic_black", false);
	Chair(M(-18.2, 15.2, 0), -90.f, false);
	Chair(M(-18.5, 17.0, 0), 200.f, true);
	// Desk with papers and a radio charger.
	Counter(M(-22.0, 12.0, 0), M(1.6, 0.8, 0.76), "wood", "plaster_dark");
	Papers(M(-22.0, 12.0, 0.78), 5, 0.35f, 33);
	Box(M(-21.6, 12.2, 0.84), M(0.25, 0.12, 0.1), "plastic_black", false);
	// Lockers along the west wall.
	for (int32 I = 0; I < 5; ++I)
	{
		Box(M(-23.6, 13.0 + I * 0.62, 0.95), M(0.5, 0.58, 1.9), "locker");
		Box(M(-23.33, 13.0 + I * 0.62, 1.6), M(0.02, 0.4, 0.15), "steel_dark", false);
	}
	// Open gun locker at the back (the AR15 sits here).
	Box(M(-20.0, 20.6, 1.0), M(1.6, 0.5, 2.0), "steel_dark");
	Box(M(-19.0, 20.0, 1.0), M(0.05, 0.9, 1.9), "steel_dark", false, FRotator(0, -55, 0));
	Box(M(-20.0, 20.3, 0.8), M(1.4, 0.3, 0.04), "steel", false);
	// Supply cabinet and noticeboard.
	Box(M(-22.6, 20.5, 1.0), M(1.2, 0.5, 2.0), "painted_metal");
	Box(M(-22.6, 20.24, 1.0), M(0.02, 0.04, 1.6), "steel_dark", false);
	Text(TEXT("SUPPLIES"), M(-22.6, 20.22, 1.85), FRotator(0, -90, 0), 10.f, FColor(230, 230, 230));
	// Wall map of the mall (objective clue).
	UTexture* Map = MakeTextTexture(512, 384, FColor(230, 228, 218), {
		{TEXT("LARKSPUR GALLERIA - FLOOR PLAN"), FColor(30, 40, 70)},
		{TEXT("L1: Entrance W | Clothing | Security | Food Court E"), FColor(40, 40, 40)},
		{TEXT("L2: Voltline | Pharmacy | Cinemas E"), FColor(40, 40, 40)},
		{TEXT("Maintenance + EMERGENCY RADIO: L2 east hall"), FColor(160, 30, 20)}}, 1.0f);
	if (Map)
	{
		Panel(M(-23.85, 17.6, 1.75), M(0.01, 1.6, 1.2), UMallMaterials::Get()->MakeImageMaterial(Map, 0.9f), FRotator(0, 0, 0));
	}
	NormalLights.Add(Rect(M(-20, 15, 3.35), FRotator(-90, 0, 0), FLinearColor(0.9f, 0.95f, 1.f), 250.f, 900.f, 120.f, 60.f));
	NavRegion(X0 + 0.6, X1 - 0.6, Y0 + 0.8, Y1 - 0.8, F, 1.5);
}

// ---------------------------------------------------------------------------------------------
// Upper floor locations
// ---------------------------------------------------------------------------------------------

void AMallBuilder::BuildTheater()
{
	// MARQUEE 6 CINEMAS - L2 east. Lobby with concessions, and a raked auditorium.
	const double F = F2;
	// Lobby X 26..32.
	Box(M(29, 0, F + 0.006), M(6, 24, 0.012), "floor_carpet", false);
	WallY(-28, -CW, 26, F, C2, 0.3, "plaster_dark");
	WallY(CW, 28, 26, F, C2, 0.3, "plaster_dark");
	Box(M(29, 12.15, F + 2.5), M(6, 0.3, 5.0), "plaster_dark");
	Box(M(29, -12.15, F + 2.5), M(6, 0.3, 5.0), "plaster_dark");
	Box(M(29, 0, F + 4.6), M(6, 24, 0.1), "ceiling", false);
	Box(M(26.0, 0, F + 4.0), M(0.5, 18, 1.4), "plaster_dark");
	Text(TEXT("MARQUEE 6 CINEMAS"), M(25.72, 0, F + 4.05), FRotator(0, 180, 0), 60.f, FColor(255, 70, 60));
	for (int32 I = 0; I < 20; ++I)
	{
		Box(M(25.72, -8.5 + I * 0.9, F + 3.42), M(0.04, 0.1, 0.1), "light_amber", false);
	}
	// Concessions.
	Counter(M(30.2, 0, F), M(1.0, 8.0, 1.05), "steel", "plaster_dark");
	Box(M(30.2, 0, F + 1.08), M(0.7, 7.6, 0.02), "glass", false);
	Box(M(31.0, -2.5, F + 1.6), M(0.7, 1.1, 1.1), "glass", true);
	Box(M(31.0, -2.5, F + 1.25), M(0.6, 1.0, 0.4), "plastic_yellow", false);
	Box(M(31.4, 1.5, F + 1.3), M(0.5, 1.2, 0.8), "steel", true);
	for (int32 I = 0; I < 4; ++I)
	{
		Cyl(M(31.15, 1.05 + I * 0.3, F + 1.25), 2.f, 14.f, "plastic_black");
	}
	UTexture* Menu = MakeTextTexture(512, 192, FColor(10, 10, 14), {{TEXT("CONCESSIONS"), FColor(255, 80, 60)}, {TEXT("POPCORN  SODA  NACHOS  CANDY"), FColor(240, 230, 200)}}, 1.4f);
	if (Menu)
	{
		Panel(M(31.85, 0, F + 3.2), M(0.02, 6.0, 1.6), UMallMaterials::Get()->MakeImageMaterial(Menu, 1.5f), FRotator(0, 180, 0));
	}
	// Ticket podium and queue posts.
	Box(M(27.5, -9.0, F + 0.55), M(0.8, 1.0, 1.1), "wood_dark", true);
	for (int32 I = 0; I < 4; ++I)
	{
		Cyl(M(27.5 + I * 1.0, -6.0, F + 0.5), 3.f, 100.f, "chrome");
		if (I < 3) Box(M(28.0 + I * 1.0, -6.0, F + 0.92), M(1.0, 0.04, 0.06), "fabric_red", false);
	}
	Cyl(M(30.5, -6.0, F + 0.5), 3.f, 100.f, "chrome", false, FRotator(80, 0, 0));
	// Fictional movie posters.
	Poster(M(26.17, -10.6, F + 1.8), FRotator(0, 0, 0), 1.0f, 1.5f, TEXT("NIGHT SHIFT"), TEXT("Some jobs never end"), FColor(20, 25, 50), FColor(240, 200, 120));
	Poster(M(26.17, 10.6, F + 1.8), FRotator(0, 0, 0), 1.0f, 1.5f, TEXT("PAPER MOON"), TEXT("A love story, 1962"), FColor(60, 20, 30), FColor(250, 230, 210));
	Poster(M(29.0, 11.98, F + 1.8), FRotator(0, -90, 0), 1.0f, 1.5f, TEXT("ORBITAL"), TEXT("Nobody hears you"), FColor(8, 10, 14), FColor(140, 200, 255));
	Poster(M(29.0, -11.98, F + 1.8), FRotator(0, 90, 0), 1.0f, 1.5f, TEXT("SUMMER LEAGUE"), TEXT("The underdogs return"), FColor(240, 180, 40), FColor(30, 30, 30));
	NormalLights.Add(Point(M(28.5, -5, F + 4.0), FLinearColor(1.f, 0.6f, 0.4f), 250.f, 900.f));
	NormalLights.Add(Point(M(28.5, 5, F + 4.0), FLinearColor(1.f, 0.6f, 0.4f), 250.f, 900.f));
	NavRegion(26.6, 29.4, -11.4, 11.4, F, 2.0);

	// Auditorium X 32..46, Y -10..10. Back cross-aisle at 6.0, rows step down to 4.8 at the screen.
	const double AX0 = 32, AX1 = 46, AY = 10;
	WallX(32, 46, AY, 4.4, C2, 0.3, "plaster_dark");
	WallX(32, 46, -AY, 4.4, C2, 0.3, "plaster_dark");
	WallY(-AY, AY, 32, F, C2, 0.3, "plaster_dark", {FVector2D(-1.6, 1.6)}, 2.6);
	Box(M(31.8, 0, F + 2.9), M(0.1, 2.4, 0.4), "plaster_dark", false);
	Text(TEXT("THEATRE 1"), M(31.83, 0, F + 2.95), FRotator(0, 180, 0), 18.f, FColor(255, 200, 120));
	Box(M(39, 0, C2 - 0.1), M(14, 20, 0.2), "plaster_dark");
	// Acoustic wall panels.
	for (double X = 33.5; X < 45; X += 2.0)
	{
		Box(M(X, AY - 0.2, 8.0), M(1.6, 0.08, 4.0), "fabric_black", false);
		Box(M(X, -AY + 0.2, 8.0), M(1.6, 0.08, 4.0), "fabric_black", false);
	}
	Box(M(33.0, 0, F - 0.05), M(2.0, 20, 0.1), "floor_carpet");
	const int32 Rows = 7;
	const double RowDrop = 0.17, RowStart = 34.0, RowDepth = 1.0;
	for (int32 I = 0; I < Rows; ++I)
	{
		const double X = RowStart + (I + 0.5) * RowDepth;
		const double Top = F - RowDrop * (I + 1);
		Box(M(X, 0, (4.4 + Top) * 0.5), M(RowDepth, 20, Top - 4.4), "floor_carpet", true);
		// Seat row (centre block), with cupholders.
		for (double Y = -7.2; Y <= 7.21; Y += 0.6)
		{
			Box(M(X + 0.1, Y, Top + 0.25), M(0.5, 0.52, 0.12), "fabric_red", false);
			Box(M(X - 0.22, Y, Top + 0.55), M(0.12, 0.52, 0.7), "fabric_red", false, FRotator(-10, 0, 0));
			Box(M(X + 0.05, Y + 0.29, Top + 0.4), M(0.5, 0.06, 0.3), "plastic_black", false);
		}
		Collider(M(X, 0, Top + 0.5), M(0.7, 15.0, 1.0), FRotator::ZeroRotator, true);
		// Aisle step lights.
		Box(M(X - 0.45, 8.6, Top + 0.03), M(0.06, 0.3, 0.04), "light_amber", false);
		Box(M(X - 0.45, -8.6, Top + 0.03), M(0.06, 0.3, 0.04), "light_amber", false);
	}
	const double FrontZ = F - RowDrop * Rows;
	Box(M(43.5, 0, (4.4 + FrontZ) * 0.5), M(5.0, 20, FrontZ - 4.4), "floor_carpet", true);
	// Side aisles are ramps for smooth walking.
	for (double Y : {8.6, -8.6})
	{
		Ramp(M(RowStart, Y, F), M(RowStart + Rows * RowDepth, Y, FrontZ), 1.6);
	}
	// Screen and its frame.
	Box(M(45.85, 0, FrontZ + 3.4), M(0.1, 16.5, 7.0), "plaster_dark", false);
	Box(M(45.78, 0, FrontZ + 3.4), M(0.02, 15.5, 6.4), "screen_static", false);
	Box(M(45.3, 0, FrontZ + 0.4), M(1.0, 17, 0.8), "plaster_dark");
	// Exit signs over the front side doors.
	for (double Y : {9.6, -9.6})
	{
		Box(M(44.6, Y, FrontZ + 2.2), M(1.2, 0.2, 2.2), "steel_dark", false);
		Box(M(44.6, Y * 0.985, FrontZ + 3.5), M(0.5, 0.1, 0.2), "light_green", false);
		Text(TEXT("EXIT"), M(44.6, Y * 0.97, FrontZ + 3.5), FRotator(0, Y > 0 ? -90.f : 90.f, 0), 12.f, FColor(80, 255, 120));
	}
	Point(M(44.0, 9.0, FrontZ + 3.4), FLinearColor(0.3f, 1.f, 0.4f), 60.f, 500.f);
	Point(M(44.0, -9.0, FrontZ + 3.4), FLinearColor(0.3f, 1.f, 0.4f), 60.f, 500.f);
	Rect(M(44.5, 0, FrontZ + 3.4), FRotator(0, 180, 0), FLinearColor(0.75f, 0.82f, 1.f), 120.f, 2400.f, 1500.f, 600.f);
	Point(M(33, 0, F + 3.0), FLinearColor(1.f, 0.7f, 0.5f), 40.f, 700.f);
	// Spilled popcorn and a dropped bag in the rows.
	Box(M(36.6, 3.0, F - 0.5 + 0.01), M(0.2, 0.15, 0.25), "prop_red", false, FRotator(0, 30, 80));
	Papers(M(40.5, -2.0, FrontZ - 0.0), 4, 0.6f, 51);

	NavRegion(32.4, 33.8, -9.4, 9.4, F, 1.5);
	NavRegion(34.2, 41.0, 7.9, 9.3, F - 1.2, 1.4);
	NavRegion(34.2, 41.0, -9.3, -7.9, F - 1.2, 1.4);
	NavRegion(41.4, 45.0, -9.4, 9.4, FrontZ, 2.0);
	Spawn(44.0, 6.0, FrontZ);
	Spawn(44.0, -6.0, FrontZ, true);
	Spawn(33.0, 5.0, F);
}

void AMallBuilder::BuildPharmacy()
{
	// CAREWELL PHARMACY - L2 north. Bright, cool, clinical. Medicine aisles and a dispensary.
	const double X0 = -24, X1 = -12, Y0 = CW, Y1 = 28, F = F2;
	StoreShell(X0, X1, Y0, Y1, F, "plaster_mint", "floor_vinyl", 4.2);
	Storefront(X0, X1, Y0, F, true, TEXT("CAREWELL PHARMACY"), "plaster_white", FColor(90, 220, 170), 4.0, true);
	for (double X : {-21.5, -18.0, -14.5})
	{
		Shelf(M(X, 16.5, F), 6.0f, 1.8f, 90.f, "laminate_white", int32(X * -10), 1);
	}
	Text(TEXT("PAIN RELIEF"), M(-21.5, 13.4, F + 2.1), FRotator(0, -90, 0), 14.f, FColor(30, 120, 100));
	Text(TEXT("FIRST AID"), M(-18.0, 13.4, F + 2.1), FRotator(0, -90, 0), 14.f, FColor(200, 40, 40));
	Text(TEXT("COLD & FLU"), M(-14.5, 13.4, F + 2.1), FRotator(0, -90, 0), 14.f, FColor(30, 120, 100));
	Counter(M(-14.0, 11.0, F), M(2.4, 0.8, 1.05), "laminate_white", "plaster_mint");
	Box(M(-14.4, 11.0, F + 1.2), M(0.35, 0.3, 0.25), "plastic_black", false);
	// Dispensary counter and back area.
	WallX(X0, X1, 22.5, F, 4.2, 0.2, "plaster_white", {FVector2D(-15.5, -13.9)}, 2.4);
	Counter(M(-19.5, 22.0, F), M(5.0, 0.7, 1.1), "laminate_white", "plaster_white");
	Box(M(-19.5, 22.4, F + 1.9), M(5.0, 0.05, 1.4), "glass", false);
	Text(TEXT("PRESCRIPTIONS"), M(-19.5, 22.32, F + 3.0), FRotator(0, -90, 0), 20.f, FColor(30, 130, 110));
	for (double X : {-22.0, -19.0})
	{
		Shelf(M(X, 26.5, F), 2.6f, 2.2f, 0.f, "laminate_white", int32(X * -7), 1);
	}
	Crate(M(-14.0, 26.5, F), M(0.6, 0.5, 0.5), 10.f);
	Box(M(-23.6, 25.0, F + 1.2), M(0.4, 1.2, 0.04), "steel", false);
	Papers(M(-17.5, 24.5, F), 7, 0.9f, 61);
	Box(M(-16.0, 15.0, F + 0.01), M(1.2, 0.6, 0.004), "blood", false, FRotator(0, 40, 0), false);

	CeilingLights(FBox(M(X0 + 0.5, Y0 + 0.5, 0), M(X1 - 0.5, 22, 0)), F + 4.2, 2.4, "light_cool", FLinearColor(0.85f, 0.93f, 1.f), 300.f, true);
	NormalLights.Add(Point(M(-18, 25.5, F + 3.6), FLinearColor(0.9f, 0.95f, 1.f), 200.f, 700.f));
	NavRegion(X0 + 0.6, X1 - 0.6, Y0 + 0.8, 22.0, F, 1.6);
	NavRegion(X0 + 0.6, X1 - 0.6, 23.0, Y1 - 0.6, F, 1.6);
	Spawn(-20, 25.5, F);
}

void AMallBuilder::BuildElectronics()
{
	// VOLTLINE - L2 north-west. Display tables, TV wall, phone bar and a storage room (the fuse).
	const double X0 = -40, X1 = -24, Y0 = CW, Y1 = 28, F = F2;
	StoreShell(X0, X1, Y0, Y1, F, "plaster_dark", "carpet_grey", 4.2);
	Storefront(X0, X1, Y0, F, true, TEXT("VOLTLINE"), "steel_dark", FColor(110, 200, 255), 4.0, true);
	ElectronicsDoor = M(-32, Y0 - 1.2, F);
	WallX(X0, X1, 23.0, F, 4.2, 0.2, "plaster_dark", {FVector2D(-27.0, -25.4)}, 2.4);
	Text(TEXT("STAFF ONLY - STORAGE"), M(-26.2, 22.88, F + 2.6), FRotator(0, -90, 0), 12.f, FColor(200, 210, 220));
	// TV wall on the west side.
	for (int32 R = 0; R < 2; ++R)
	{
		for (int32 C = 0; C < 4; ++C)
		{
			Screen(M(-39.75, 11.5 + C * 2.4, F + 1.4 + R * 1.2), FRotator(0, 0, 0), 2.0f, 1.1f, (R + C) % 2 ? FName("screen_blue") : FName("screen_off"));
		}
	}
	Rect(M(-38.5, 15, F + 3.0), FRotator(0, 180, 0), FLinearColor(0.5f, 0.7f, 1.f), 60.f, 600.f, 300.f, 200.f);
	// Display tables with laptops and phones.
	FRandomStream R(707);
	for (double X : {-35.5, -31.0})
	{
		for (double Y : {12.5, 16.5, 20.0})
		{
			Box(M(X, Y, F + 0.45), M(2.4, 1.2, 0.9), "laminate_white");
			for (int32 K = 0; K < 3; ++K)
			{
				const double KX = X - 0.75 + K * 0.75;
				if (R.FRand() < 0.25f) continue;   // looted spots
				Box(M(KX, Y, F + 0.91), M(0.34, 0.24, 0.02), "plastic_grey", false);
				Box(M(KX, Y + 0.1, F + 1.02), M(0.34, 0.02, 0.22), "plastic_grey", false, FRotator(0, 0, 0) + FRotator(0, 0, -15));
				Box(M(KX, Y + 0.088, F + 1.02), M(0.31, 0.005, 0.19), K % 2 ? FName("screen_blue") : FName("screen_off"), false, FRotator(0, 0, -15));
			}
		}
	}
	// Phone bar along the east side.
	Counter(M(-25.2, 15.5, F), M(0.8, 6.0, 1.0), "laminate_white", "plaster_dark");
	for (int32 K = 0; K < 6; ++K)
	{
		Box(M(-25.2, 13.2 + K * 0.9, F + 1.06), M(0.08, 0.16, 0.12), "plastic_black", false, FRotator(-70, 0, 0));
	}
	Box(M(-24.3, 15.5, F + 2.6), M(0.1, 5.0, 1.0), "plaster_navy", false);
	Text(TEXT("MOBILE"), M(-24.36, 15.5, F + 2.6), FRotator(0, 180, 0), 28.f, FColor(110, 200, 255));
	// Toppled TV box and a smashed display.
	Box(M(-28.5, 19.5, F + 0.35), M(1.2, 0.2, 0.7), "cardboard", true, FRotator(0, 25, 90));
	Box(M(-37.5, 22.0, F + 0.2), M(1.0, 0.5, 0.4), "cardboard", true, FRotator(0, -10, 0));
	// Storage room: shelving, workbench (fuse), boxes.
	Shelf(M(-38.0, 27.3, F), 3.5f, 2.2f, 0.f, "steel_dark", 811, 3);
	Shelf(M(-33.5, 27.3, F), 3.5f, 2.2f, 0.f, "steel_dark", 812, 3);
	Counter(M(-28.0, 26.9, F), M(2.4, 0.9, 0.92), "wood", "steel_dark");
	Box(M(-28.8, 27.2, F + 1.0), M(0.4, 0.3, 0.16), "plastic_black", false);
	Crate(M(-25.5, 24.5, F), M(0.6, 0.5, 0.45), 15.f);
	Crate(M(-36.0, 24.2, F), M(0.7, 0.6, 0.5), -10.f);
	FuseLocation = M(-28.0, 26.75, F + 0.92);

	CeilingLights(FBox(M(X0 + 1, Y0 + 1, 0), M(X1 - 1, 22.5, 0)), F + 4.2, 3.4, "light_cool", FLinearColor(0.8f, 0.9f, 1.f), 220.f, true);
	NormalLights.Add(Point(M(-31, 25.5, F + 3.6), FLinearColor(0.85f, 0.9f, 1.f), 140.f, 800.f));
	NavRegion(X0 + 0.6, X1 - 0.6, Y0 + 0.8, 22.4, F, 1.8);
	NavRegion(X0 + 0.6, X1 - 0.6, 23.5, Y1 - 0.6, F, 1.6);
	Spawn(-36, 25, F);
	Spawn(-34, 14, F, true);

	// Abandoned survivor sniper nest on the balcony just east of Voltline, overlooking the atrium.
	const FVector N = M(-19.6, 7.0, F);
	Box(N + M(-1.6, 0.3, 0.25), M(0.9, 2.0, 0.5), "fabric_green", false, FRotator(0, 10, 0));          // sleeping bag
	Bench(N + M(1.4, -1.5, 0), 20.f);
	Box(N + M(-0.2, -1.7, 0.4), M(1.4, 0.04, 0.8), "laminate_white", true, FRotator(0, 0, 0));        // tipped table as cover
	Cyl(N + M(-1.0, 1.3, 0.15), 8.f, 30.f, "painted_metal");                                            // lantern
	Sphere(N + M(-1.0, 1.3, 0.35), M(0.12, 0.12, 0.14), "light_warm");
	Point(N + M(-1.0, 1.3, 0.5), FLinearColor(1.f, 0.7f, 0.4f), 120.f, 350.f);
	for (int32 I = 0; I < 5; ++I)
	{
		Cyl(N + M(0.2 + I * 0.15, 0.9, 0.06), 3.3f, 12.f, I % 2 ? "prop_red" : "prop_white", false, FRotator(90, I * 40.f, 0));
	}
	Papers(N + M(0.0, 1.0, 0), 3, 0.3f, 91);
	Text(TEXT("NO MORE ROUNDS. GOING FOR HELP. - D."), N + M(0.5, 1.6, 0.03), FRotator(90, 0, 0), 8.f, FColor(30, 30, 30));
}

void AMallBuilder::BuildMaintenance()
{
	// Service hall (L2) X 10..13 running north, then the maintenance / mall management room.
	const double F = F2;
	WallY(CW, 28, 10.0, F, C2, 0.25, "concrete");
	WallY(CW, 18, 13.0, F, C2, 0.25, "concrete");
	WallY(24, 28, 13.0, F, C2, 0.25, "concrete");
	Box(M(11.5, 18.5, F + 0.006), M(3, 19, 0.012), "floor_concrete", false);
	Box(M(11.5, 18.5, F + 3.4), M(3, 19, 0.1), "concrete", false);
	// Doorway from the walkway with a steel frame and signage.
	WallX(10.0, 13.0, CW, F, C2, 0.3, "concrete", {FVector2D(10.6, 12.4)}, 2.5);
	Box(M(11.5, CW - 0.17, F + 2.95), M(1.8, 0.04, 0.4), "painted_yellow", false);
	Text(TEXT("MAINTENANCE - STAFF ONLY"), M(11.5, CW - 0.2, F + 2.95), FRotator(0, -90, 0), 12.f, FColor(20, 20, 20));
	MaintenanceDoor = M(11.5, CW - 1.0, F);
	// Pipes and conduits along the hall ceiling.
	for (int32 I = 0; I < 3; ++I)
	{
		Cyl(M(10.4 + I * 0.25, 18.5, F + 3.1 - I * 0.12), 6.f - I, 1900.f, I == 1 ? "painted_red" : "steel", false, FRotator(90, 90, 0));
	}
	Box(M(10.25, 15.0, F + 1.5), M(0.3, 0.6, 0.9), "painted_metal", false);
	EmergencyLights.Add(Point(M(11.5, 13.0, F + 3.0), FLinearColor(1.f, 0.08f, 0.04f), 400.f, 900.f));
	EmergencyLights.Add(Point(M(11.5, 24.0, F + 3.0), FLinearColor(1.f, 0.08f, 0.04f), 400.f, 900.f));
	Box(M(11.5, 13.0, F + 3.3), M(0.2, 0.2, 0.12), "light_red", false);
	Box(M(11.5, 24.0, F + 3.3), M(0.2, 0.2, 0.12), "light_red", false);
	NavRegion(10.5, 12.5, CW + 0.6, 27.4, F, 1.5);

	// Room X 13..26, Y 18..28 (entered from the hall at Y 18..24).
	const double X0 = 13, X1 = 26, Y0 = 18, Y1 = 28;
	Box(M(19.5, 23, F + 0.006), M(13, 10, 0.012), "floor_plate", false);
	Box(M(19.5, 23, F + 3.8), M(13, 10, 0.1), "concrete", false);
	WallX(X0, X1, Y0, F, C2, 0.25, "concrete");
	WallY(Y0, Y1, X1, F, C2, 0.25, "concrete");
	// Electrical panels along the north wall.
	for (int32 I = 0; I < 6; ++I)
	{
		const double X = 14.5 + I * 1.1;
		Box(M(X, 27.6, F + 1.3), M(0.9, 0.4, 2.0), "painted_metal");
		Box(M(X, 27.39, F + 1.75), M(0.6, 0.02, 0.4), "steel", false);
		Box(M(X + 0.25, 27.38, F + 1.4), M(0.06, 0.02, 0.2), "plastic_black", false);
		Box(M(X - 0.2, 27.38, F + 2.15), M(0.06, 0.02, 0.06), I % 2 ? FName("light_green") : FName("light_red"), false);
	}
	Box(M(17.2, 27.38, F + 2.6), M(1.6, 0.02, 0.3), "painted_yellow", false);
	Text(TEXT("DANGER - HIGH VOLTAGE"), M(17.2, 27.36, F + 2.6), FRotator(0, -90, 0), 10.f, FColor(10, 10, 10));
	// Tool shelving, workbench, mop sink, gas cylinders.
	Shelf(M(25.4, 21.5, F), 3.0f, 2.2f, 90.f, "steel_dark", 921, 0);
	Counter(M(16.0, 18.8, F), M(3.0, 0.9, 0.92), "wood", "steel_dark");
	Box(M(15.2, 18.8, F + 1.0), M(0.5, 0.3, 0.15), "painted_red", false);
	Box(M(16.6, 18.6, F + 0.95), M(0.3, 0.05, 0.03), "steel", false, FRotator(0, 30, 0));
	Box(M(16.0, 18.42, F + 1.7), M(2.6, 0.04, 1.0), "concrete", false);
	for (int32 I = 0; I < 8; ++I)
	{
		Box(M(15.0 + I * 0.28, 18.45, F + 1.75), M(0.04, 0.03, 0.35), "steel", false);
	}
	Cyl(M(14.0, 26.8, F + 0.7), 15.f, 140.f, "painted_green", true);
	Cyl(M(14.4, 27.1, F + 0.7), 15.f, 140.f, "painted_green", true);
	// Management desk with monitors and the emergency radio.
	Counter(M(23.5, 24.0, F), M(1.2, 3.0, 0.76), "wood_dark", "steel_dark");
	Screen(M(24.0, 22.9, F + 1.05), FRotator(0, 180, 0), 0.6f, 0.4f, "screen_off");
	Chair(M(22.3, 23.0, F), 90.f, false);
	RadioLocation = M(23.5, 24.6, F + 0.76);
	UTexture* Plan = MakeTextTexture(512, 256, FColor(220, 220, 210), {{TEXT("EMERGENCY PROCEDURES"), FColor(150, 20, 20)}, {TEXT("1. Replace 30A radio fuse"), FColor(30, 30, 30)}, {TEXT("2. Transmit on channel 9"), FColor(30, 30, 30)}}, 1.0f);
	if (Plan)
	{
		Panel(M(25.85, 24.6, F + 1.8), M(0.01, 1.4, 0.7), UMallMaterials::Get()->MakeImageMaterial(Plan, 0.8f), FRotator(0, 180, 0));
	}
	// Lighting: one tired fluorescent plus red emergency lamps.
	NormalLights.Add(Rect(M(19.0, 22.5, F + 3.75), FRotator(-90, 0, 0), FLinearColor(0.85f, 0.95f, 0.9f), 120.f, 800.f, 120.f, 20.f));
	Box(M(19.0, 22.5, F + 3.77), M(1.2, 0.2, 0.04), "light_cool", false);
	EmergencyLights.Add(Point(M(15.0, 24.0, F + 3.4), FLinearColor(1.f, 0.08f, 0.04f), 400.f, 900.f));
	EmergencyLights.Add(Point(M(24.0, 20.0, F + 3.4), FLinearColor(1.f, 0.08f, 0.04f), 400.f, 900.f));
	NavRegion(X0 + 0.4, X1 - 0.6, Y0 + 0.6, Y1 - 0.8, F, 1.6);
	Spawn(11.5, 27, F);
	Spawn(20, 26.5, F, true);
}

void AMallBuilder::BuildClosedShops()
{
	struct FShop { double X0, X1; bool bNorth; double F; FString Name; FName Frame; FColor Color; };
	const FShop Shops[] = {
		{-16, -4, true, 0, TEXT("BLOOM BEAUTY"), "plaster_white", FColor(255, 160, 190)},
		{-4, 8, true, 0, TEXT("PAGEBOUND BOOKS"), "wood_dark", FColor(240, 220, 180)},
		{8, 17, true, 0, TEXT("LITTLE LARK KIDS"), "plaster_mint", FColor(255, 220, 90)},
		{20, 26, true, 0, TEXT("CINDER & CO."), "plaster_dark", FColor(240, 200, 160)},
		{-16, -6, false, 0, TEXT("GLEAM JEWELERS"), "bronze", FColor(255, 230, 170)},
		{-3, 8, false, 0, TEXT("SOLE STREET"), "plaster_dark", FColor(255, 255, 255)},
		{8, 17, false, 0, TEXT("ORCHID NAILS"), "plaster_white", FColor(220, 140, 220)},
		{20, 26, false, 0, TEXT("BRIGHT BITES"), "painted_red", FColor(255, 240, 200)},
		{-12, 0, true, F2, TEXT("LUXE OPTICAL"), "steel_dark", FColor(220, 230, 240)},
		{0, 10, true, F2, TEXT("VELVET VINYL"), "plaster_navy", FColor(255, 120, 120)},
		{13, 26, true, F2, TEXT("ATLAS TRAVEL"), "plaster_white", FColor(120, 180, 255)},
		{-40, -28, false, F2, TEXT("THE KNIT LOFT"), "wood_light", FColor(250, 210, 170)},
		{-28, -16, false, F2, TEXT("POCKET GAMES"), "plaster_dark", FColor(120, 255, 160)},
		{-16, -4, false, F2, TEXT("ARC LIGHT GALLERY"), "plaster_white", FColor(240, 240, 240)},
		{-4, 8, false, F2, TEXT("FIT FORGE"), "steel_dark", FColor(255, 140, 60)},
		{8, 17, false, F2, TEXT("NOMAD LUGGAGE"), "wood_dark", FColor(230, 200, 150)},
		{20, 26, false, F2, TEXT("SCOOP SHACK"), "plaster_mint", FColor(255, 200, 220)}};
	for (const FShop& S : Shops)
	{
		Storefront(S.X0, S.X1, S.bNorth ? CW : -CW, S.F, S.bNorth, S.Name, S.Frame, S.Color, FMath::Min(4.0, S.X1 - S.X0 - 3.0), false);
		// Party walls behind the display so neighbouring shops read as separate units.
		const double YB = S.bNorth ? CW + 2.0 : -CW - 2.0;
		Box(M(S.X0 + 0.05, (CW + 1.0) * (S.bNorth ? 1 : -1), S.F + 2.7), M(0.1, 2.0, 5.4), "plaster_dark");
		Box(M((S.X0 + S.X1) * 0.5, YB, S.F + 2.7), M(S.X1 - S.X0, 0.2, 5.4), "plaster_dark");
	}
}

void AMallBuilder::BuildServiceAreas()
{
	// L1 north service corridor (X 17..20) - loading dock door at the back.
	{
		const double F = 0;
		WallY(CW, 28, 17.0, F, C1, 0.25, "concrete");
		WallY(CW, 28, 20.0, F, C1, 0.25, "concrete");
		WallX(17.0, 20.0, CW, F, C1, 0.3, "concrete", {FVector2D(17.6, 19.4)}, 2.5);
		Box(M(18.5, 18.5, F + 0.006), M(3, 19, 0.012), "floor_concrete", false);
		Box(M(18.5, 18.5, F + 3.2), M(3, 19, 0.1), "concrete", false);
		Box(M(18.5, 27.8, F + 1.5), M(2.6, 0.2, 3.0), "painted_metal");
		Text(TEXT("LOADING DOCK"), M(18.5, 27.68, F + 2.6), FRotator(0, -90, 0), 14.f, FColor(230, 200, 40));
		Box(M(18.5, CW - 0.17, F + 2.85), M(1.6, 0.04, 0.3), "painted_metal", false);
		Text(TEXT("SERVICE"), M(18.5, CW - 0.2, F + 2.85), FRotator(0, -90, 0), 12.f, FColor(230, 230, 230));
		Crate(M(17.6, 22.0, F), M(0.7, 0.6, 0.6), 5.f);
		Crate(M(19.4, 25.0, F), M(0.7, 0.6, 0.5), 20.f);
		Cyl(M(18.5, 18.5, F + 3.0), 7.f, 1900.f, "steel", false, FRotator(90, 90, 0));
		EmergencyLights.Add(Point(M(18.5, 15.0, F + 2.9), FLinearColor(1.f, 0.08f, 0.04f), 300.f, 800.f));
		EmergencyLights.Add(Point(M(18.5, 25.0, F + 2.9), FLinearColor(1.f, 0.08f, 0.04f), 300.f, 800.f));
		NormalLights.Add(Point(M(18.5, 20.0, F + 3.0), FLinearColor(0.85f, 0.95f, 0.9f), 80.f, 700.f));
		NavRegion(17.4, 19.6, CW + 0.6, 27.2, F, 1.5);
		Spawn(18.5, 26.8, F);
	}
	// L1 restroom corridor (south, X -6..-3).
	{
		const double F = 0;
		WallY(-28, -CW, -6.0, F, C1, 0.25, "plaster_white");
		WallY(-28, -CW, -3.0, F, C1, 0.25, "plaster_white");
		WallX(-6.0, -3.0, -CW, F, C1, 0.3, "plaster_white", {FVector2D(-5.4, -3.6)}, 2.5);
		Box(M(-4.5, -18.5, F + 0.006), M(3, 19, 0.012), "floor_vinyl", false);
		Box(M(-4.5, -18.5, F + 3.0), M(3, 19, 0.1), "ceiling", false);
		Text(TEXT("RESTROOMS"), M(-4.5, -CW + 0.2, F + 2.85), FRotator(0, 90, 0), 14.f, FColor(230, 230, 230));
		Box(M(-4.5, -CW + 0.17, F + 2.85), M(1.6, 0.04, 0.3), "plaster_navy", false);
		Box(M(-5.85, -16.0, F + 1.1), M(0.05, 1.0, 2.2), "wood_light", false);
		Box(M(-3.15, -20.0, F + 1.1), M(0.05, 1.0, 2.2), "wood_light", false);
		Box(M(-4.5, -27.8, F + 1.1), M(1.2, 0.2, 2.2), "steel_dark");
		Vending(M(-3.6, -12.5, F), 90.f, "plastic_blue", "light_cool");
		NormalLights.Add(Point(M(-4.5, -16.0, F + 2.8), FLinearColor(0.85f, 0.95f, 0.9f), 70.f, 700.f));
		EmergencyLights.Add(Point(M(-4.5, -26.0, F + 2.8), FLinearColor(1.f, 0.08f, 0.04f), 300.f, 700.f));
		NavRegion(-5.6, -3.4, -27.2, -CW - 0.6, F, 1.5);
		Spawn(-4.5, -26.8, F);
	}
	// L2 south restrooms / service (X 17..20).
	{
		const double F = F2;
		WallY(-28, -CW, 17.0, F, C2, 0.25, "plaster_white");
		WallY(-28, -CW, 20.0, F, C2, 0.25, "plaster_white");
		WallX(17.0, 20.0, -CW, F, C2, 0.3, "plaster_white", {FVector2D(17.6, 19.4)}, 2.5);
		Box(M(18.5, -18.5, F + 0.006), M(3, 19, 0.012), "floor_vinyl", false);
		Box(M(18.5, -18.5, F + 3.0), M(3, 19, 0.1), "ceiling", false);
		Text(TEXT("RESTROOMS"), M(18.5, -CW + 0.2, F + 2.85), FRotator(0, 90, 0), 14.f, FColor(230, 230, 230));
		Box(M(18.5, -27.8, F + 1.1), M(1.2, 0.2, 2.2), "steel_dark");
		NormalLights.Add(Point(M(18.5, -16.0, F + 2.8), FLinearColor(0.85f, 0.95f, 0.9f), 60.f, 700.f));
		EmergencyLights.Add(Point(M(18.5, -26.0, F + 2.8), FLinearColor(1.f, 0.08f, 0.04f), 300.f, 700.f));
		NavRegion(17.4, 19.6, -27.2, -CW - 0.6, F, 1.5);
		Spawn(18.5, -26.8, F);
	}
}

void AMallBuilder::BuildExterior()
{
	// Plaza outside the west entrance: bollards, a bus shelter, parked cars and light poles.
	for (double Y = -6; Y <= 6.01; Y += 2.0)
	{
		Cyl(M(-47.5, Y, 0.45), 11.f, 90.f, "painted_yellow", true);
	}
	Box(M(-52, 9, 1.3), M(4, 1.6, 0.08), "glass", false);
	Box(M(-52, 9, 2.6), M(4.2, 1.8, 0.1), "steel_dark", false);
	for (double X : {-53.9, -50.1}) Box(M(X, 9.7, 1.3), M(0.1, 0.1, 2.6), "steel_dark", false);
	auto Car = [this](const FVector& P, float Yaw, FName Paint)
	{
		const FRotator R(0, Yaw, 0);
		Box(P + R.RotateVector(M(0, 0, 0.65)), M(4.4, 1.8, 0.7), Paint, true, R);
		Box(P + R.RotateVector(M(-0.3, 0, 1.25)), M(2.4, 1.6, 0.55), "glass_dark", false, R);
		Box(P + R.RotateVector(M(-0.3, 0, 1.54)), M(2.2, 1.6, 0.05), Paint, false, R);
		for (double WX : {-1.4, 1.4})
		{
			for (double WY : {-0.85, 0.85})
			{
				Cyl(P + R.RotateVector(M(WX, WY, 0.33)), 33.f, 22.f, "rubber", false, R + FRotator(0, 0, 90));
			}
		}
	};
	Car(M(-58, -8, 0), 90.f, "painted_red");
	Car(M(-58, -2.5, 0), 92.f, "painted_metal");
	Car(M(-63, 6, 0), -88.f, "plastic_white");
	for (double Y : {-14.0, 14.0})
	{
		Cyl(M(-55, Y, 4.0), 9.f, 800.f, "steel_dark");
		Box(M(-55, Y, 8.0), M(1.2, 0.3, 0.15), "steel_dark", false);
	}
	// Painted bays.
	for (double Y = -12; Y <= 12.01; Y += 3.0)
	{
		Box(M(-60, Y, 0.001), M(5, 0.1, 0.01), "plastic_white", false, FRotator::ZeroRotator, false);
	}
}

void AMallBuilder::BuildLighting()
{
	// Concourse downlights on both levels (skipping the void).
	const FLinearColor Warm(1.f, 0.84f, 0.66f);
	CeilingLights(FBox(M(-40, -CW, 0), M(-VX, CW, 0)), C1, 4.5, "light_warm", Warm, 420.f, true);
	CeilingLights(FBox(M(VX, -CW, 0), M(26, CW, 0)), C1, 4.5, "light_warm", Warm, 420.f, true);
	CeilingLights(FBox(M(-VX, VY, 0), M(VX, CW, 0)), C1, 4.25, "light_warm", Warm, 300.f, true);
	CeilingLights(FBox(M(-VX, -CW, 0), M(VX, -VY, 0)), C1, 4.25, "light_warm", Warm, 300.f, true);
	CeilingLights(FBox(M(-40, -CW, 0), M(-VX, CW, 0)), C2, 4.5, "light_warm", Warm, 380.f, true);
	CeilingLights(FBox(M(VX, -CW, 0), M(26, CW, 0)), C2, 4.5, "light_warm", Warm, 380.f, true);
	CeilingLights(FBox(M(-VX, VY + 0.6, 0), M(VX, CW, 0)), C2 - 0.6, 4.25, "light_warm", Warm, 260.f, true);
	CeilingLights(FBox(M(-VX, -CW, 0), M(VX, -VY - 0.6, 0)), C2 - 0.6, 4.25, "light_warm", Warm, 260.f, true);
	// HVAC grilles.
	for (double X = -36; X < 26; X += 9.0)
	{
		if (FMath::Abs(X) < VX) continue;
		Box(M(X, 4.5, C1 - 0.02), M(1.2, 0.4, 0.04), "steel_dark", false);
		Box(M(X, -4.5, C2 - 0.02), M(1.2, 0.4, 0.04), "steel_dark", false);
	}
	// Soft fill in the atrium so the shaded side of the void is never black.
	Point(M(0, 0, 8.0), FLinearColor(0.85f, 0.9f, 1.f), 900.f, 2800.f);
	// Emergency lamps near entrances.
	for (const FVector& P : {M(-39.5, 4.0, 3.2), M(-39.5, -4.0, 3.2), M(25.5, 8.0, 3.2), M(25.5, -8.0, F2 + 3.2)})
	{
		Box(P, M(0.2, 0.2, 0.12), "light_red", false);
		EmergencyLights.Add(Point(P + M(0.3, 0, 0), FLinearColor(1.f, 0.08f, 0.04f), 400.f, 800.f));
	}
}

void AMallBuilder::PlaceLoot()
{
	using E = EMallItem;
	auto L = [this](E Item, int32 Amount, const FVector& P, float Yaw, const TCHAR* Prompt, const TCHAR* Label = TEXT("")) -> AMallInteractable*
	{
		return Loot(uint8(Item), Amount, P, Yaw, Prompt, Label);
	};
	auto Bag = [this](const FVector& P, float Yaw, const TCHAR* Prompt, const TCHAR* Label, std::initializer_list<FMallLoot> Contents) -> AMallInteractable*
	{
		AMallInteractable* I = Loot(uint8(EMallItem::Container), 0, P, Yaw, Prompt, Label);
		for (const FMallLoot& C : Contents) I->Contents.Add(C);
		return I;
	};
	auto Item = [](E Kind, int32 Count) { FMallLoot Out; Out.Item = Kind; Out.Amount = Count; return Out; };

	// Start: the vestibule.
	Bag(M(-41.2, -3.0, 0), 30.f, TEXT("Search abandoned backpack"), TEXT("backpack"), {Item(E::AmmoPistol, 15), Item(E::Food, 10)});
	ExitItem = L(E::Exit, 0, M(-43.2, 0, 0), 0.f, TEXT("Open entrance shutter"));
	ExitItem->bEnabled = true;

	// Security office: AR15, rifle ammo, 9mm, first aid, incident board.
	L(E::WeaponAR, 60, M(-20.0, 20.3, 0.82), 180.f, TEXT("Take AR15 carbine"));
	L(E::AmmoRifle, 60, M(-22.3, 12.0, 0.78), 90.f, TEXT("Pick up 5.56 ammo can"));
	L(E::AmmoPistol, 30, M(-17.2, 16.9, 0.78), 0.f, TEXT("Pick up 9mm ammo"));
	Bag(M(-22.6, 20.2, 1.2), -90.f, TEXT("Search supply cabinet"), TEXT("first-aid cabinet"), {Item(E::Medkit, 1), Item(E::AmmoPistol, 15)});
	L(E::Note, 0, M(-23.85, 19.6, 1.75), 0.f, TEXT("Read incident board"));

	// Sporting goods: shotgun at the hunting counter, shells on the shelves.
	L(E::WeaponShotgun, 16, M(-33.6, -23.8, 1.1), 0.f, TEXT("Take pump shotgun"));
	L(E::AmmoShells, 12, M(-31.2, -23.8, 1.1), 20.f, TEXT("Pick up 12ga shells"));
	L(E::AmmoShells, 12, M(-38.5, -21.0, 0), 90.f, TEXT("Pick up 12ga shells"));
	L(E::AmmoPistol, 20, M(-24.0, -12.6, 0), 0.f, TEXT("Pick up 9mm ammo"));
	L(E::Food, 15, M(-30.5, -16.4, 0), 0.f, TEXT("Eat energy bar"), TEXT("energy bar"));
	Bag(M(-20.5, -24.6, 0), 15.f, TEXT("Search duffel bag"), TEXT("green duffel bag"), {Item(E::AmmoShells, 8), Item(E::Food, 15)});

	// Clothing store: stockroom bag and a fitting-room backpack.
	Bag(M(-36.0, 26.6, 0), 0.f, TEXT("Search stockroom box"), TEXT("cardboard box"), {Item(E::AmmoPistol, 15)});
	Bag(M(-26.0, 18.4, 0), 90.f, TEXT("Search backpack"), TEXT("backpack"), {Item(E::Medkit, 1)});

	// Food court: snacks and drinks.
	L(E::Food, 15, M(40.2, -14.0, 1.1), 0.f, TEXT("Drink soda"), TEXT("soda"));
	L(E::Food, 15, M(40.2, 6.5, 1.1), 0.f, TEXT("Drink water"), TEXT("water"));
	L(E::Food, 20, M(43.6, 1.0, 0.93), 0.f, TEXT("Eat burger"), TEXT("chips"));
	L(E::Food, 15, M(40.2, -6.5, 1.1), 0.f, TEXT("Drink energy drink"), TEXT("energy drink"));
	L(E::Food, 15, M(40.2, 14.5, 1.1), 0.f, TEXT("Eat chips"), TEXT("chips"));
	Bag(M(44.2, 2.9, 0), 0.f, TEXT("Search kitchen supply box"), TEXT("cardboard box"), {Item(E::Food, 20), Item(E::AmmoPistol, 10)});

	// Pharmacy: the best medical supplies.
	L(E::Medkit, 1, M(-18.0, 12.9, F2), 0.f, TEXT("Take medkit"));
	L(E::Medkit, 1, M(-19.5, 22.0, F2 + 1.1), 0.f, TEXT("Take medkit"));
	L(E::Medkit, 1, M(-22.0, 25.6, F2), 0.f, TEXT("Take medkit"));
	L(E::Food, 25, M(-14.0, 11.0, F2 + 1.06), 0.f, TEXT("Take painkillers & water"), TEXT("water"));

	// Electronics: the fuse in storage, a little 9mm at the counter.
	FuseItem = L(E::Fuse, 0, FuseLocation, 0.f, TEXT("Take replacement fuse"));
	L(E::AmmoPistol, 15, M(-25.2, 17.5, F2 + 1.02), 0.f, TEXT("Pick up 9mm ammo"));

	// Sniper nest on the balcony.
	L(E::WeaponSniper, 10, M(-19.3, 8.1, F2 + 0.1), 180.f, TEXT("Take bolt-action rifle"));
	L(E::AmmoSniper, 10, M(-18.5, 7.3, F2), 0.f, TEXT("Pick up .308 rounds"));
	Bag(M(-21.4, 8.3, F2), 0.f, TEXT("Search survivor's pack"), TEXT("backpack"), {Item(E::AmmoSniper, 5), Item(E::Food, 15)});

	// Cinema: concessions and a dropped security bag.
	L(E::Food, 15, M(30.2, 2.5, F2 + 1.1), 0.f, TEXT("Eat popcorn"), TEXT("popcorn"));
	L(E::Food, 15, M(30.2, -1.0, F2 + 1.1), 0.f, TEXT("Drink soda"), TEXT("soda"));
	Bag(M(42.5, -6.0, F2 - 1.19), 30.f, TEXT("Search security duffel"), TEXT("duffel bag"), {Item(E::AmmoRifle, 30), Item(E::AmmoShells, 8)});

	// Maintenance: the radio, first aid and rifle ammo.
	RadioItem = L(E::Radio, 0, RadioLocation, 180.f, TEXT("Install fuse & restore emergency radio"));
	Bag(M(25.7, 19.5, F2 + 1.4), 180.f, TEXT("Search first-aid cabinet"), TEXT("first-aid cabinet"), {Item(E::Medkit, 1)});
	L(E::AmmoRifle, 30, M(16.6, 18.8, F2 + 0.92), 0.f, TEXT("Pick up 5.56 ammo can"));

	// Scattered evacuation bags on the concourses.
	Bag(M(-12.0, 7.6, 0), 60.f, TEXT("Search shopping bag"), TEXT("shopping bag"), {Item(E::AmmoPistol, 10)});
	Bag(M(23.5, -6.5, 0), 0.f, TEXT("Search dropped backpack"), TEXT("backpack"), {Item(E::Food, 15), Item(E::AmmoShells, 4)});
	Bag(M(8.0, -7.8, F2), 0.f, TEXT("Search duffel bag"), TEXT("duffel bag"), {Item(E::AmmoRifle, 20)});
	Bag(M(-35.5, -6.0, F2), 20.f, TEXT("Search backpack"), TEXT("backpack"), {Item(E::Medkit, 1)});
}

void AMallBuilder::BuildNavGraph()
{
	UWorld* W = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallNavBuild), false);
	const FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	// Grid nodes over every walkable region; drop the ones inside fixtures or without floor.
	for (const FNavRegion& R : NavRegions)
	{
		const double Sp = R.Spacing * 100.0;
		for (double X = R.Box.Min.X + Sp * 0.5; X <= R.Box.Max.X; X += Sp)
		{
			for (double Y = R.Box.Min.Y + Sp * 0.5; Y <= R.Box.Max.Y; Y += Sp)
			{
				FHitResult Floor;
				const FVector Top(X, Y, R.Box.Min.Z + 250.0);
				if (!W->LineTraceSingleByObjectType(Floor, Top, FVector(X, Y, R.Box.Min.Z - 200.0), Obj, Params))
				{
					continue;
				}
				const FVector P = Floor.ImpactPoint + FVector(0, 0, 90.0);
				if (W->OverlapAnyTestByObjectType(P + FVector(0, 0, 25), FQuat::Identity, Obj, FCollisionShape::MakeCapsule(30.f, 70.f), Params))
				{
					continue;
				}
				Nav.Add(P);
			}
		}
	}
	// Doorway and route anchor nodes so narrow openings always connect.
	auto Anchor = [this, W, &Params, &Obj](const FVector& FloorPt)
	{
		const FVector P = FloorPt + FVector(0, 0, 90.0);
		if (!W->OverlapAnyTestByObjectType(P + FVector(0, 0, 25), FQuat::Identity, Obj, FCollisionShape::MakeCapsule(28.f, 70.f), Params))
		{
			return Nav.Add(P);
		}
		return int32(INDEX_NONE);
	};
	// Store entrances (inside and outside each opening).
	const FVector2D Doors[] = {{-32, CW}, {-20.2, CW}, {-28, -CW}, {-18, CW}, {-32, CW}, {11.5, CW}, {18.5, CW}, {-4.5, -CW}, {18.5, -CW}};
	const double DoorF[] = {0, 0, 0, F2, F2, F2, 0, 0, F2};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Doors); ++I)
	{
		const double S = Doors[I].Y > 0 ? 1.0 : -1.0;
		Anchor(M(Doors[I].X, Doors[I].Y - S * 1.0, DoorF[I]));
		Anchor(M(Doors[I].X, Doors[I].Y + S * 1.0, DoorF[I]));
	}
	Anchor(M(-30.2, 23.0, 0));      // clothing stockroom door
	Anchor(M(-26.2, 23.0, F2));     // electronics storage door
	Anchor(M(-14.7, 22.5, F2));     // pharmacy back door
	Anchor(M(-23.5, -23.0, 0));     // sporting stock door
	Anchor(M(11.5, 21.0, F2));      // maintenance hall -> room
	Anchor(M(13.6, 21.0, F2));
	Anchor(M(31.0, 0, F2));         // auditorium doors
	Anchor(M(33.0, 0, F2));
	Anchor(M(-40.5, 0, 0));         // vestibule
	const int32 SB = Anchor(StairBottom);
	const int32 ST = Anchor(StairTop);
	const int32 EB = Anchor(EscBottom);
	const int32 ET = Anchor(EscTop);

	Nav.AutoLink(W, 520.f, 26.f);
	Nav.Link(SB, ST);
	Nav.Link(EB, ET);
	const int32 Before = Nav.Nodes.Num();
	Nav.PruneToComponent(Nav.Nearest(W, PlayerStart + FVector(0, 0, 90), false));
	UE_LOG(LogTemp, Display, TEXT("MALL NAV: pruned %d isolated nodes"), Before - Nav.Nodes.Num());

	// Keep only spawn points that sit on the graph.
	auto OnGraph = [this, W](TArray<FVector>& List)
	{
		List.RemoveAll([this, W](const FVector& S)
		{
			const int32 N = Nav.Nearest(W, S + FVector(0, 0, 90));
			return N == INDEX_NONE || FVector::Dist(Nav.Nodes[N].Pos, S + FVector(0, 0, 90)) > 450.f;
		});
	};
	OnGraph(ZombieSpawns);
	OnGraph(StartZombies);
	UE_LOG(LogTemp, Display, TEXT("MALL NAV: %d nodes, %d links, %d spawns, %d start spots (stairs %d-%d, esc %d-%d)"),
		Nav.Nodes.Num(), Nav.NumLinks(), ZombieSpawns.Num(), StartZombies.Num(), SB, ST, EB, ET);
}
