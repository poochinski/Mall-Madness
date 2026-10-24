#include "MallBuilder.h"

#include "MallMaterials.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	/** Local frame helper: offsets in metres, rotated by yaw around an origin (cm). */
	struct FXf
	{
		FVector O;
		float Yaw;
		FVector operator()(double X, double Y, double Z) const { return O + FRotator(0, Yaw, 0).RotateVector(FVector(X, Y, Z) * 100.0); }
		FRotator R(float P = 0, float Y = 0, float Ro = 0) const { return FRotator(P, Yaw + Y, Ro); }
	};

	const FName ProductMats[] = {"plastic_red", "plastic_blue", "plastic_white", "plastic_green", "plastic_yellow", "plastic_orange", "cardboard", "plastic_grey"};
}

void AMallBuilder::Bench(const FVector& P, float Yaw)
{
	FXf X{P, Yaw};
	Box(X(0, 0, 0.44), M(1.8, 0.48, 0.06), "wood", true, X.R());
	Box(X(0, -0.2, 0.75), M(1.8, 0.06, 0.4), "wood", false, X.R(0, 0, -8));
	for (double S : {-0.75, 0.75})
	{
		Box(X(S, 0, 0.21), M(0.06, 0.44, 0.42), "steel_dark", false, X.R());
		Box(X(S, -0.2, 0.6), M(0.05, 0.05, 0.4), "steel_dark", false, X.R());
	}
}

void AMallBuilder::Planter(const FVector& P, float W, float D)
{
	Box(P + M(0, 0, 0.3), M(W, D, 0.6), "stone_clad");
	Box(P + M(0, 0, 0.6), M(W - 0.12, D - 0.12, 0.04), "soil", false);
	FRandomStream R(int32(P.X * 7 + P.Y * 13));
	const int32 Bushes = FMath::Max(2, int32(W * D));
	for (int32 I = 0; I < Bushes; ++I)
	{
		const FVector L = P + M(R.FRandRange(-W * 0.35, W * 0.35), R.FRandRange(-D * 0.35, D * 0.35), 0.75);
		const float S = R.FRandRange(0.45f, 0.8f);
		Sphere(L, M(S, S, S * 0.8), "foliage");
	}
	if (W >= 1.5 && D >= 1.5)
	{
		Cyl(P + M(0, 0, 1.4), 6.f, 170.f, "wood_dark");
		Sphere(P + M(0, 0, 2.6), M(1.5, 1.5, 1.2), "foliage");
		Sphere(P + M(0.3, 0.2, 2.2), M(1.0, 1.0, 0.9), "foliage");
	}
}

void AMallBuilder::TrashCan(const FVector& P)
{
	Cyl(P + M(0, 0, 0.45), 26.f, 90.f, "steel", true);
	Cyl(P + M(0, 0, 0.92), 28.f, 6.f, "steel_dark");
	Box(P + M(0, 0.27, 0.75), M(0.25, 0.02, 0.12), "plastic_black", false);
}

void AMallBuilder::Table(const FVector& P, bool bRound, float Yaw)
{
	FXf X{P, Yaw};
	if (bRound)
	{
		Cyl(X(0, 0, 0.74), 45.f, 3.f, "laminate_white", true);
		Cyl(X(0, 0, 0.37), 4.f, 72.f, "steel_dark");
		Cyl(X(0, 0, 0.02), 25.f, 3.f, "steel_dark");
	}
	else
	{
		Box(X(0, 0, 0.74), M(0.8, 0.8, 0.035), "laminate_white", true, X.R());
		Box(X(0, 0, 0.37), M(0.08, 0.08, 0.72), "steel_dark", false, X.R());
		Box(X(0, 0, 0.02), M(0.5, 0.5, 0.03), "steel_dark", false, X.R());
	}
}

void AMallBuilder::Chair(const FVector& P, float Yaw, bool bTipped)
{
	if (bTipped)
	{
		// Knocked over: lying on its back.
		FXf X{P, Yaw};
		Box(X(0, 0, 0.23), M(0.44, 0.06, 0.44), "plastic_grey", false, X.R(0, 0, 90));
		Box(X(0, 0.25, 0.05), M(0.44, 0.44, 0.04), "plastic_grey", false, X.R(0, 0, 0));
		for (double S : {-0.18, 0.18})
		{
			Box(X(S, -0.22, 0.2), M(0.03, 0.03, 0.42), "steel_dark", false, X.R(90, 0, 0));
		}
		return;
	}
	FXf X{P, Yaw};
	Box(X(0, 0, 0.45), M(0.44, 0.44, 0.04), "plastic_grey", false, X.R());
	Box(X(0, -0.21, 0.72), M(0.44, 0.03, 0.42), "plastic_grey", false, X.R(0, 0, -6));
	for (double SX : {-0.19, 0.19})
	{
		for (double SY : {-0.19, 0.19})
		{
			Box(X(SX, SY, 0.22), M(0.025, 0.025, 0.44), "steel_dark", false, X.R());
		}
	}
}

void AMallBuilder::Shelf(const FVector& P, float Len, float Height, float Yaw, FName Mat, int32 Seed, int32 Kind)
{
	FXf X{P, Yaw};
	const double D = Kind == 2 ? 0.9 : 0.6;
	Collider(X(0, 0, Height * 0.5), M(Len, D, Height), X.R(), true);
	Box(X(0, 0, Height * 0.5), M(Len, 0.04, Height), Mat, false, X.R());                    // back panel
	for (double S : {-Len * 0.5 + 0.03, Len * 0.5 - 0.03})
	{
		Box(X(S, 0, Height * 0.5), M(0.05, D, Height), "steel_dark", false, X.R());
	}
	Box(X(0, 0, 0.08), M(Len, D, 0.16), Mat, false, X.R());                                   // kick base
	FRandomStream R(Seed);
	const int32 Levels = FMath::Max(2, int32(Height / 0.45));
	for (int32 L = 0; L < Levels; ++L)
	{
		const double Z = 0.18 + L * (Height - 0.3) / Levels;
		Box(X(0, 0, Z), M(Len - 0.1, D, 0.025), "steel", false, X.R());
		if (L == Levels - 1 && Height > 1.9) continue;
		// Products on both faces of a gondola.
		for (double Side : {-1.0, 1.0})
		{
			double Cursor = -Len * 0.5 + 0.12;
			while (Cursor < Len * 0.5 - 0.15)
			{
				if (R.FRand() < 0.18f)
				{
					Cursor += 0.25;   // gaps from looting
					continue;
				}
				double W = 0.1, H = 0.2, Dp = 0.18;
				FName PM = ProductMats[R.RandRange(0, 7)];
				switch (Kind)
				{
				case 1: W = 0.06; H = R.FRandRange(0.08f, 0.16f); Dp = 0.06; PM = R.FRand() < 0.5f ? FName("plastic_white") : PM; break;   // pharmacy
				case 2: W = R.FRandRange(0.2f, 0.4f); H = R.FRandRange(0.15f, 0.35f); Dp = 0.35; PM = R.FRand() < 0.5f ? FName("prop_olive") : (R.FRand() < 0.5f ? FName("plastic_orange") : FName("fabric_green")); break; // sports
				case 3: W = R.FRandRange(0.12f, 0.3f); H = R.FRandRange(0.06f, 0.25f); Dp = 0.2; PM = R.FRand() < 0.6f ? FName("plastic_black") : FName("plastic_white"); break; // electronics
				case 4: W = 0.08; H = R.FRandRange(0.12f, 0.25f); Dp = 0.08; break;   // food
				default: W = R.FRandRange(0.1f, 0.25f); H = R.FRandRange(0.1f, 0.3f); break;
				}
				if (Kind == 1 || Kind == 4)
				{
					Cyl(X(Cursor + W * 0.5, Side * D * 0.3, Z + H * 0.5 + 0.015), W * 50.f, H * 100.f, PM);
				}
				else
				{
					Box(X(Cursor + W * 0.5, Side * D * 0.28, Z + H * 0.5 + 0.015), M(W * 0.9, Dp, H), PM, false, X.R(0, R.FRandRange(-6.f, 6.f), 0));
				}
				Cursor += W + 0.02;
			}
		}
	}
	// A few items knocked onto the floor.
	for (int32 I = 0; I < 2; ++I)
	{
		if (R.FRand() < 0.6f)
		{
			Box(X(R.FRandRange(-Len * 0.4, Len * 0.4), ((R.FRand() < 0.5f) ? 1 : -1) * (D * 0.5 + 0.3), 0.05), M(0.18, 0.12, 0.09), ProductMats[R.RandRange(0, 7)], false, X.R(0, R.FRandRange(0.f, 90.f), 0));
		}
	}
}

void AMallBuilder::Counter(const FVector& P, const FVector& Size, FName Top, FName Body)
{
	Box(P + FVector(0, 0, Size.Z * 0.5 - 2.5), FVector(Size.X, Size.Y, Size.Z - 5), Body);
	Box(P + FVector(0, 0, Size.Z - 2.5), FVector(Size.X + 4, Size.Y + 4, 5), Top, false);
	Box(P + FVector(0, 0, 5), FVector(Size.X - 6, Size.Y - 6, 10), "plastic_black", false);
}

void AMallBuilder::ClothesRack(const FVector& P, float Yaw, int32 Seed)
{
	FXf X{P, Yaw};
	FRandomStream R(Seed);
	Collider(X(0, 0, 0.75), M(1.5, 0.55, 1.5), X.R(), false);
	Box(X(0, 0, 1.45), M(1.5, 0.03, 0.03), "chrome", false, X.R());
	for (double S : {-0.74, 0.74})
	{
		Box(X(S, 0, 0.73), M(0.03, 0.03, 1.45), "chrome", false, X.R());
		Box(X(S, 0, 0.02), M(0.05, 0.5, 0.03), "chrome", false, X.R());
	}
	static const FName Fabrics[] = {"fabric_red", "fabric_blue", "fabric_grey", "fabric_beige", "fabric_green", "fabric_black"};
	const FName Base = Fabrics[R.RandRange(0, 5)];
	const int32 Count = R.RandRange(6, 11);
	for (int32 I = 0; I < Count; ++I)
	{
		const double XX = -0.65 + I * (1.3 / Count);
		const FName F = R.FRand() < 0.7f ? Base : Fabrics[R.RandRange(0, 5)];
		const double Len = R.FRandRange(0.6f, 1.0f);
		Box(X(XX, 0, 1.42 - Len * 0.5), M(0.035, 0.46, Len), F, false, X.R(0, R.FRandRange(-8.f, 8.f), R.FRandRange(-3.f, 3.f)));
	}
}

void AMallBuilder::Mannequin(const FVector& P, float Yaw, FName Shirt, FName Pants, bool bFallen)
{
	FXf X{P, Yaw};
	if (bFallen)
	{
		// Toppled display figure lying on the floor.
		Box(X(0, 0, 0.13), M(0.55, 0.3, 0.24), Shirt, false, X.R(0, 0, 0));
		Sphere(X(0.42, 0, 0.12), M(0.2, 0.2, 0.24), "plastic_white");
		Box(X(-0.55, 0.08, 0.1), M(0.8, 0.12, 0.14), Pants, false, X.R(0, 4, 0));
		Box(X(-0.55, -0.1, 0.1), M(0.8, 0.12, 0.14), Pants, false, X.R(0, -6, 0));
		Box(X(0.1, 0.3, 0.08), M(0.5, 0.08, 0.08), "plastic_white", false, X.R(0, 30, 0));
		return;
	}
	Cyl(X(0, 0, 0.02), 18.f, 3.f, "chrome");
	Cyl(X(0, 0, 0.45), 1.5f, 85.f, "chrome");
	Box(X(0, 0, 0.92), M(0.32, 0.2, 0.25), Pants, false, X.R());
	Box(X(0.0, 0.08, 0.5), M(0.13, 0.13, 0.75), Pants, false, X.R());
	Box(X(0.0, -0.08, 0.5), M(0.13, 0.13, 0.75), Pants, false, X.R());
	Box(X(0, 0, 1.28), M(0.36, 0.22, 0.5), Shirt, false, X.R());
	Box(X(0, 0.22, 1.2), M(0.1, 0.1, 0.55), Shirt, false, X.R(0, 0, 6));
	Box(X(0, -0.22, 1.2), M(0.1, 0.1, 0.55), Shirt, false, X.R(0, 0, -6));
	Cyl(X(0, 0, 1.58), 5.f, 10.f, "plastic_white");
	Sphere(X(0, 0, 1.72), M(0.19, 0.17, 0.24), "plastic_white");
	Collider(X(0, 0, 0.9), M(0.5, 0.5, 1.8), X.R(), false);
}

void AMallBuilder::Tray(const FVector& P, float Yaw, int32 Seed)
{
	FXf X{P, Yaw};
	FRandomStream R(Seed);
	Box(X(0, 0, 0.008), M(0.45, 0.33, 0.015), (R.FRand() < 0.5f) ? FName("plastic_red") : FName("plastic_orange"), false, X.R());
	Cyl(X(0.1, 0.06, 0.08), 4.5f, 14.f, "plastic_white");
	Cyl(X(0.1, 0.06, 0.155), 4.8f, 1.f, "plastic_red");
	Box(X(-0.08, -0.04, 0.03), M(0.14, 0.1, 0.04), "paper", false, X.R(0, 15, 0));
}

void AMallBuilder::ShoppingBag(const FVector& P, float Yaw, FName Mat)
{
	FXf X{P, Yaw};
	Box(X(0, 0, 0.2), M(0.32, 0.14, 0.4), Mat, false, X.R());
	Box(X(0, 0, 0.44), M(0.14, 0.01, 0.08), "fabric_black", false, X.R());
}

void AMallBuilder::Suitcase(const FVector& P, float Yaw, FName Mat, bool bFallen)
{
	FXf X{P, Yaw};
	if (bFallen)
	{
		Box(X(0, 0, 0.13), M(0.7, 0.45, 0.25), Mat, true, X.R());
		Box(X(0.37, 0, 0.13), M(0.04, 0.15, 0.03), "plastic_black", false, X.R());
	}
	else
	{
		Box(X(0, 0, 0.38), M(0.45, 0.25, 0.65), Mat, true, X.R());
		Box(X(0, 0, 0.78), M(0.15, 0.03, 0.1), "steel_dark", false, X.R());
		Cyl(X(0.18, 0.1, 0.03), 3.f, 2.f, "plastic_black", false, X.R(0, 0, 90));
		Cyl(X(-0.18, 0.1, 0.03), 3.f, 2.f, "plastic_black", false, X.R(0, 0, 90));
	}
}

void AMallBuilder::Papers(const FVector& P, int32 Count, float Spread, int32 Seed)
{
	FRandomStream R(Seed);
	for (int32 I = 0; I < Count; ++I)
	{
		Box(P + M(R.FRandRange(-Spread, Spread), R.FRandRange(-Spread, Spread), 0.004 + I * 0.0005), M(0.21, 0.297, 0.002), R.FRand() < 0.85f ? FName("paper") : FName("cardboard"), false, FRotator(0, R.FRandRange(0.f, 360.f), 0), false);
	}
}

void AMallBuilder::Vending(const FVector& P, float Yaw, FName Body, FName Glow)
{
	FXf X{P, Yaw};
	Box(X(0, 0, 0.95), M(0.95, 0.8, 1.9), Body, true, X.R());
	Box(X(0.05, 0.41, 1.15), M(0.6, 0.02, 1.2), "glass_dark", false, X.R());
	Box(X(0.05, 0.395, 1.15), M(0.58, 0.01, 1.18), Glow, false, X.R());
	FRandomStream R(int32(P.X + P.Y));
	for (int32 Row = 0; Row < 5; ++Row)
	{
		for (int32 Col = 0; Col < 5; ++Col)
		{
			if (R.FRand() < 0.25f) continue;
			Cyl(X(-0.2 + Col * 0.1, 0.33, 0.66 + Row * 0.23), 3.2f, 12.f, ProductMats[R.RandRange(0, 5)]);
		}
	}
	Box(X(-0.35, 0.41, 1.1), M(0.15, 0.03, 0.4), "plastic_black", false, X.R());
	Box(X(0.05, 0.41, 0.3), M(0.55, 0.04, 0.18), "plastic_black", false, X.R());
}

void AMallBuilder::Crate(const FVector& P, const FVector& Size, float Yaw)
{
	Box(P + FVector(0, 0, Size.Z * 0.5), Size, "cardboard", true, FRotator(0, Yaw, 0));
	Box(P + FVector(0, 0, Size.Z + 0.2), FVector(Size.X * 0.98, 6, 0.4), "plastic_yellow", false, FRotator(0, Yaw, 0), false);
}

void AMallBuilder::Poster(const FVector& P, const FRotator& Rot, float W, float H, const FString& Title, const FString& Sub, const FColor& Bg, const FColor& Fg)
{
	UTexture* Tex = MakeTextTexture(256, 384, Bg, {{Title, Fg}, {Sub, FColor(Fg.R * 0.8, Fg.G * 0.8, Fg.B * 0.8)}}, 1.6f);
	if (Tex)
	{
		UMaterialInstanceDynamic* MID = UMallMaterials::Get()->MakeImageMaterial(Tex, 1.2f);
		Panel(P, FVector(1.f, W * 100.f, H * 100.f), MID, Rot);
	}
	Box(P - Rot.RotateVector(FVector(1.5f, 0, 0)), FVector(2.f, W * 100.f + 8.f, H * 100.f + 8.f), "steel_dark", false, Rot, false);
}

void AMallBuilder::Screen(const FVector& P, const FRotator& Rot, float W, float H, FName Emissive)
{
	Box(P, FVector(4.f, W * 100.f + 3.f, H * 100.f + 3.f), "plastic_black", false, Rot, false);
	Box(P + Rot.RotateVector(FVector(2.2f, 0, 0)), FVector(0.5f, W * 100.f - 2.f, H * 100.f - 2.f), Emissive, false, Rot, false);
}

void AMallBuilder::CeilingLights(const FBox& Area, double Z, double Spacing, FName Mat, const FLinearColor& Color, float Intensity, bool bAddLights)
{
	// Recessed panels on a grid; real lights on a coarser grid to keep the light count sane.
	const FVector Min = Area.Min / 100.0, Max = Area.Max / 100.0;
	int32 I = 0;
	for (double X = Min.X + Spacing * 0.5; X < Max.X; X += Spacing)
	{
		for (double Y = Min.Y + Spacing * 0.5; Y < Max.Y; Y += Spacing)
		{
			Box(M(X, Y, Z - 0.02), M(0.6, 0.6, 0.04), Mat, false, FRotator::ZeroRotator, false);
			Box(M(X, Y, Z - 0.005), M(0.68, 0.68, 0.02), "steel", false, FRotator::ZeroRotator, false);
			if (bAddLights && (I++ % 2 == 0))
			{
				NormalLights.Add(Rect(M(X, Y, Z - 0.06), FRotator(-90, 0, 0), Color, Intensity, Spacing * 170.f, 60.f, 60.f));
			}
		}
	}
}

void AMallBuilder::Storefront(double X0, double X1, double Y, double Floor, bool bStoreNorth, const FString& Name, FName FrameMat, const FColor& SignColor, double DoorW, bool bOpen, FName ShutterMat)
{
	// Y is the frontage line; the store lies on +Y if bStoreNorth.
	const double S = bStoreNorth ? 1.0 : -1.0;
	const double Mid = (X0 + X1) * 0.5;
	const double Top = Floor + 5.4;
	const double GlassTop = Floor + 3.6;
	const FRotator Face(0, bStoreNorth ? -90.f : 90.f, 0);

	// Stone pilasters and the fascia band.
	Box(M(X0 + 0.3, Y, (Floor + Top) * 0.5), M(0.6, 0.7, Top - Floor), "stone_clad");
	Box(M(X1 - 0.3, Y, (Floor + Top) * 0.5), M(0.6, 0.7, Top - Floor), "stone_clad");
	Box(M(Mid, Y - S * 0.05, (GlassTop + Top) * 0.5 + 0.05), M(X1 - X0 - 1.2, 0.5, Top - GlassTop - 0.1), FrameMat);
	Box(M(Mid, Y - S * 0.32, GlassTop + 0.02), M(X1 - X0 - 1.2, 0.08, 0.06), "steel");
	// Sign lettering with a soft wash light.
	Text(Name, M(Mid, Y - S * 0.31, GlassTop + 0.85), Face, 62.f, SignColor);
	if (bOpen)
	{
		Point(M(Mid, Y - S * 1.2, GlassTop + 0.9), FLinearColor(SignColor) * 0.5f + FLinearColor(0.5f, 0.45f, 0.4f), 250.f, 450.f);
	}

	// Glazing with mullions either side of the door.
	const double D0 = Mid - DoorW * 0.5, D1 = Mid + DoorW * 0.5;
	auto Glaze = [&](double A, double B)
	{
		if (B - A < 0.1) return;
		Box(M((A + B) * 0.5, Y, Floor + 0.15), M(B - A, 0.12, 0.3), FrameMat);
		Box(M((A + B) * 0.5, Y, Floor + 0.3 + (GlassTop - Floor - 0.3) * 0.5), M(B - A, 0.03, GlassTop - Floor - 0.3), bOpen ? "glass" : "glass_dark");
		for (double MX = A; MX <= B + 0.01; MX += FMath::Max(0.5, (B - A) / FMath::Max(1.0, FMath::RoundToDouble((B - A) / 2.2))))
		{
			Box(M(MX, Y, (Floor + GlassTop) * 0.5), M(0.07, 0.1, GlassTop - Floor), FrameMat, false);
		}
	};
	Glaze(X0 + 0.6, D0);
	Glaze(D1, X1 - 0.6);
	Box(M(Mid, Y, GlassTop - 0.1), M(DoorW, 0.14, 0.2), FrameMat);

	if (!bOpen)
	{
		// Security grille pulled down over the entrance.
		const FName SM = ShutterMat.IsNone() ? FName("painted_metal") : ShutterMat;
		Box(M(Mid, Y - S * 0.1, (Floor + GlassTop) * 0.5), M(DoorW, 0.06, GlassTop - Floor), SM);
		for (double Z = Floor + 0.15; Z < GlassTop; Z += 0.15)
		{
			Box(M(Mid, Y - S * 0.14, Z), M(DoorW, 0.02, 0.03), "steel_dark", false, FRotator::ZeroRotator, false);
		}
		Box(M(Mid, Y - S * 0.14, Floor + 0.9), M(0.25, 0.04, 0.12), "steel", false);
		// Shallow display behind dark glass so the shop doesn't read as a flat wall.
		Box(M(Mid, Y + S * 1.8, (Floor + GlassTop) * 0.5 + 0.9), M(X1 - X0 - 1.2, 0.1, GlassTop - Floor + 1.8), "plaster_dark");
		Box(M(Mid, Y + S * 0.9, Floor + 0.01), M(X1 - X0 - 1.2, 1.8, 0.02), "floor_wood", false);
		Box(M(Mid, Y + S * 0.9, GlassTop + 0.05), M(X1 - X0 - 1.2, 1.8, 0.1), "ceiling", false);
		FRandomStream R(int32(X0 * 31 + Y * 17 + Floor));
		const int32 Displays = R.RandRange(1, 2);
		for (int32 I = 0; I < Displays; ++I)
		{
			const double PX = FMath::Lerp(X0 + 1.5, X1 - 1.5, (I + 0.5) / Displays);
			if (R.FRand() < 0.5f)
			{
				Mannequin(M(PX, Y + S * 1.0, Floor), bStoreNorth ? -90.f : 90.f, ProductMats[R.RandRange(0, 5)], "fabric_black", false);
			}
			else
			{
				Box(M(PX, Y + S * 1.0, Floor + 0.45), M(1.2, 0.6, 0.9), "wood_light", false);
				Box(M(PX - 0.2, Y + S * 1.0, Floor + 1.0), M(0.3, 0.2, 0.2), ProductMats[R.RandRange(0, 7)], false);
				Box(M(PX + 0.25, Y + S * 1.0, Floor + 0.98), M(0.25, 0.25, 0.16), ProductMats[R.RandRange(0, 7)], false);
			}
		}
		Point(M(Mid, Y + S * 1.0, GlassTop - 0.3), FLinearColor(1.f, 0.85f, 0.65f), 60.f, 400.f);
	}
}
