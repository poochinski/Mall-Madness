#include "MallMaterials.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionSaturate.h"

UMallMaterials* UMallMaterials::Instance = nullptr;

// Bump when a master graph changes so the shader DDC key changes too.
static constexpr uint32 GMasterVersion = 7;

UMallMaterials* UMallMaterials::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UMallMaterials>(GetTransientPackage(), TEXT("MallMaterialsLib"));
		Instance->AddToRoot();
		Instance->Init();
	}
	return Instance;
}

void UMallMaterials::Shutdown()
{
	if (Instance)
	{
		Instance->RemoveFromRoot();
		Instance = nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// Procedural textures
// ---------------------------------------------------------------------------------------------

namespace
{
	uint32 Hash2(int32 X, int32 Y, uint32 Seed)
	{
		uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	float Rand2(int32 X, int32 Y, uint32 Seed) { return (Hash2(X, Y, Seed) & 0xFFFFFF) / float(0xFFFFFF); }

	/** Tileable value noise: lattice of Period cells wrapped across the texture. */
	float ValueNoise(float U, float V, int32 Period, uint32 Seed)
	{
		const float X = U * Period, Y = V * Period;
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		auto W = [Period](int32 I) { return ((I % Period) + Period) % Period; };
		const float A = Rand2(W(X0), W(Y0), Seed), B = Rand2(W(X0 + 1), W(Y0), Seed);
		const float C = Rand2(W(X0), W(Y0 + 1), Seed), D = Rand2(W(X0 + 1), W(Y0 + 1), Seed);
		const float SX = FX * FX * (3 - 2 * FX), SY = FY * FY * (3 - 2 * FY);
		return FMath::Lerp(FMath::Lerp(A, B, SX), FMath::Lerp(C, D, SX), SY);
	}

	float Fbm(float U, float V, int32 BasePeriod, int32 Octaves, uint32 Seed)
	{
		float Sum = 0, Amp = 0.5f, Norm = 0;
		int32 P = BasePeriod;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += ValueNoise(U, V, P, Seed + O * 17) * Amp;
			Norm += Amp;
			Amp *= 0.5f;
			P *= 2;
		}
		return Sum / Norm;
	}

	FColor Pack(float R, float G, float B)
	{
		return FColor(FMath::Clamp(int32(R * 255.f), 0, 255), FMath::Clamp(int32(G * 255.f), 0, 255), FMath::Clamp(int32(B * 255.f), 0, 255), 255);
	}

	template <typename FFunc>
	TArray<FColor> Generate(int32 Size, FFunc&& Func)
	{
		TArray<FColor> Px;
		Px.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				Px[Y * Size + X] = Func((X + 0.5f) / Size, (Y + 0.5f) / Size);
			}
		}
		return Px;
	}

	/** Grid of tiles: returns R = tile/grout mask with per-tile variation, G = gloss mask. */
	FColor TileFunc(float U, float V, int32 Count, float Grout, float Variation, uint32 Seed, float Vein)
	{
		const float X = U * Count, Y = V * Count;
		const int32 TX = FMath::FloorToInt(X), TY = FMath::FloorToInt(Y);
		const float FX = X - TX, FY = Y - TY;
		const float Edge = FMath::Min(FMath::Min(FX, 1 - FX), FMath::Min(FY, 1 - FY));
		const bool bGrout = Edge < Grout;
		const float Var = Rand2(TX, TY, Seed);
		const float N = Fbm(U, V, 8, 4, Seed + 3);
		const float Veins = Vein > 0 ? FMath::Pow(1.f - FMath::Abs(FMath::Sin((U * 3 + N * 2.5f) * PI * 2)), 12.f) * Vein : 0;
		if (bGrout)
		{
			return Pack(0.0f, 0.1f, 0.2f);
		}
		const float R = 1.f - Variation + Variation * Var - Veins * 0.25f + (N - 0.5f) * 0.08f;
		return Pack(R, 0.85f + 0.15f * N, 1.f);
	}
}

UTexture2D* MallTex::MakeTexture(const TArray<FColor>& Pixels, int32 Size, bool bSRGB)
{
	UTexture2D* Tex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	FTexturePlatformData* PD = Tex->GetPlatformData();

	// Mip 0
	{
		FTexture2DMipMap& Mip = PD->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
	}

	// Box-filtered mip chain so distant floors don't shimmer.
	TArray<FColor> Prev = Pixels;
	int32 PrevSize = Size;
	while (PrevSize > 1)
	{
		const int32 S = PrevSize / 2;
		TArray<FColor> Next;
		Next.SetNumUninitialized(S * S);
		for (int32 Y = 0; Y < S; ++Y)
		{
			for (int32 X = 0; X < S; ++X)
			{
				int32 Acc[4] = {0, 0, 0, 0};
				for (int32 K = 0; K < 4; ++K)
				{
					const FColor& C = Prev[(Y * 2 + K / 2) * PrevSize + X * 2 + K % 2];
					Acc[0] += C.R; Acc[1] += C.G; Acc[2] += C.B; Acc[3] += C.A;
				}
				Next[Y * S + X] = FColor(Acc[0] / 4, Acc[1] / 4, Acc[2] / 4, Acc[3] / 4);
			}
		}
		FTexture2DMipMap* Mip = new FTexture2DMipMap(S, S, 1);
		PD->Mips.Add(Mip);
		void* Data = Mip->BulkData.Lock(LOCK_READ_WRITE);
		Data = Mip->BulkData.Realloc(S * S * sizeof(FColor));
		FMemory::Memcpy(Data, Next.GetData(), Next.Num() * sizeof(FColor));
		Mip->BulkData.Unlock();
		Prev = MoveTemp(Next);
		PrevSize = S;
	}

	Tex->SRGB = bSRGB;
	Tex->Filter = TF_Trilinear;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Wrap;
	Tex->NeverStream = true;
	Tex->LODGroup = TEXTUREGROUP_World;
	Tex->UpdateResource();
	return Tex;
}

void UMallMaterials::BuildPatterns()
{
	const int32 S = 256;
	auto Add = [this](FName Name, const TArray<FColor>& Px, int32 Size)
	{
		if (UTexture2D* T = MallTex::MakeTexture(Px, Size, false))
		{
			Patterns.Add(Name, T);
		}
	};

	// Large polished stone tiles (texture = 2x2 tiles).
	Add("tile", Generate(512, [](float U, float V) { return TileFunc(U, V, 2, 0.006f, 0.12f, 11, 0.6f); }), 512);
	// Smaller vinyl/ceramic tiles (4x4).
	Add("tile_small", Generate(S, [](float U, float V) { return TileFunc(U, V, 4, 0.02f, 0.10f, 21, 0.f); }), S);
	// Checkerboard (food court) - R is the checker, variation in G.
	Add("checker", Generate(S, [](float U, float V)
	{
		const int32 TX = FMath::FloorToInt(U * 4), TY = FMath::FloorToInt(V * 4);
		const float FX = U * 4 - TX, FY = V * 4 - TY;
		const float Edge = FMath::Min(FMath::Min(FX, 1 - FX), FMath::Min(FY, 1 - FY));
		const float N = Fbm(U, V, 8, 3, 5);
		const float C = ((TX + TY) & 1) ? 1.f : 0.f;
		return Edge < 0.02f ? Pack(0.5f, 0.2f, 0.f) : Pack(C * (0.92f + 0.08f * N) + (1 - C) * 0.05f * N, 0.75f + 0.25f * N, 1.f);
	}), S);
	// Plaster / paint.
	Add("plaster", Generate(S, [](float U, float V)
	{
		const float N = Fbm(U, V, 4, 5, 31);
		const float Speck = Rand2(int32(U * 256), int32(V * 256), 7) > 0.985f ? 0.15f : 0.f;
		return Pack(0.82f + N * 0.18f - Speck, 0.5f + N * 0.5f, N);
	}), S);
	// Concrete.
	Add("concrete", Generate(S, [](float U, float V)
	{
		const float N = Fbm(U, V, 4, 6, 41);
		const float Pores = Rand2(int32(U * 256), int32(V * 256), 9) > 0.97f ? 0.25f : 0.f;
		const float Stain = FMath::Clamp((Fbm(U, V, 2, 3, 77) - 0.55f) * 2.f, 0.f, 1.f) * 0.2f;
		return Pack(0.7f + N * 0.3f - Pores - Stain, N, N);
	}), S);
	// Wood laminate (grain runs along U).
	Add("wood", Generate(S, [](float U, float V)
	{
		const float Warp = Fbm(U, V, 4, 3, 51);
		const float Grain = 0.5f + 0.5f * FMath::Sin((V * 22.f + Warp * 6.f) * PI * 2);
		const float Plank = FMath::Frac(V * 4) < 0.012f ? 0.3f : 0.f;
		const float Fine = Fbm(U * 0.25f, V, 16, 3, 52);
		return Pack(0.55f + Grain * 0.3f + Fine * 0.15f - Plank, 0.6f + Fine * 0.4f, Grain);
	}), S);
	// Brushed metal (fine streaks along U).
	Add("brushed", Generate(S, [](float U, float V)
	{
		const float Streak = ValueNoise(U * 0.02f, V, 128, 61) * 0.6f + ValueNoise(U * 0.05f, V, 64, 62) * 0.4f;
		return Pack(0.8f + Streak * 0.2f, Streak, Streak);
	}), S);
	// Carpet (theatre) - small repeating motif with fibre noise.
	Add("carpet", Generate(S, [](float U, float V)
	{
		const float Fibre = Rand2(int32(U * 256), int32(V * 256), 71);
		const float CX = FMath::Frac(U * 8) - 0.5f, CY = FMath::Frac(V * 8) - 0.5f;
		const float Motif = (FMath::Abs(CX) + FMath::Abs(CY)) < 0.25f ? 1.f : 0.f;
		const float Ring = FMath::Abs((FMath::Abs(CX) + FMath::Abs(CY)) - 0.38f) < 0.03f ? 0.6f : 0.f;
		return Pack(FMath::Max(Motif, Ring) * 0.85f + Fibre * 0.15f, 0.9f + Fibre * 0.1f, Fibre);
	}), S);
	// Acoustic ceiling tiles (texture = 2x2 tiles of 60cm).
	Add("ceiling", Generate(S, [](float U, float V)
	{
		const float FX = FMath::Frac(U * 2), FY = FMath::Frac(V * 2);
		const float Edge = FMath::Min(FMath::Min(FX, 1 - FX), FMath::Min(FY, 1 - FY));
		const float Speck = Rand2(int32(U * 256), int32(V * 256), 81) > 0.9f ? 0.12f : 0.f;
		if (Edge < 0.012f)
		{
			return Pack(0.2f, 0.2f, 0.f);
		}
		return Pack(0.92f - Speck + Fbm(U, V, 4, 3, 82) * 0.08f, 1.f, 1.f);
	}), S);
	// Woven fabric.
	Add("fabric", Generate(S, [](float U, float V)
	{
		const float WX = 0.5f + 0.5f * FMath::Sin(U * PI * 2 * 64), WY = 0.5f + 0.5f * FMath::Sin(V * PI * 2 * 64);
		const float Weave = (FMath::Frac(U * 32) < 0.5f) == (FMath::Frac(V * 32) < 0.5f) ? WX : WY;
		const float N = Fbm(U, V, 8, 3, 91);
		return Pack(0.75f + Weave * 0.15f + N * 0.1f, 0.9f, Weave);
	}), S);
	// Diamond plate / industrial floor.
	Add("plate", Generate(S, [](float U, float V)
	{
		const float CX = FMath::Frac(U * 8 + (FMath::FloorToInt(V * 8) & 1) * 0.5f) - 0.5f;
		const float CY = FMath::Frac(V * 8) - 0.5f;
		const float D = FMath::Abs(CX * 0.4f) + FMath::Abs(CY);
		const float N = Fbm(U, V, 4, 4, 101);
		return Pack(D < 0.18f ? 1.f : 0.55f + N * 0.2f, 0.5f + N * 0.5f, 1.f);
	}), S);
	// Rubber stair tread grooves.
	Add("tread", Generate(S, [](float U, float V)
	{
		const float G = FMath::Frac(V * 12) < 0.35f ? 0.f : 1.f;
		return Pack(0.6f + G * 0.4f, G, G);
	}), S);
	// Plain noise (plastics, painted metal).
	Add("noise", Generate(S, [](float U, float V)
	{
		const float N = Fbm(U, V, 4, 4, 111);
		return Pack(0.85f + N * 0.15f, N, N);
	}), S);
	// Skin / grime for zombies.
	Add("grime", Generate(S, [](float U, float V)
	{
		const float N = Fbm(U, V, 6, 5, 121);
		const float Blotch = FMath::Clamp((Fbm(U, V, 3, 4, 122) - 0.5f) * 3.f, 0.f, 1.f);
		return Pack(0.9f - Blotch * 0.55f + N * 0.1f, 0.6f + N * 0.4f, N);
	}), S);
	// Flat white.
	Add("flat", Generate(8, [](float, float) { return Pack(1, 1, 1); }), 8);
}

// ---------------------------------------------------------------------------------------------
// Master materials
// ---------------------------------------------------------------------------------------------

namespace
{
	template <typename T>
	T* NewExpr(UMaterial* M)
	{
		T* E = NewObject<T>(M);
		E->Material = M;
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	UMaterialExpressionComponentMask* Mask(UMaterial* M, UMaterialExpression* In, bool R, bool G, bool B)
	{
		auto* E = NewExpr<UMaterialExpressionComponentMask>(M);
		E->Input.Connect(0, In);
		E->R = R; E->G = G; E->B = B; E->A = false;
		return E;
	}

	UMaterialExpression* Mul(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B)
	{
		auto* E = NewExpr<UMaterialExpressionMultiply>(M);
		E->A.Connect(0, A);
		E->B.Connect(0, B);
		return E;
	}

	UMaterialExpression* AddE(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B)
	{
		auto* E = NewExpr<UMaterialExpressionAdd>(M);
		E->A.Connect(0, A);
		E->B.Connect(0, B);
		return E;
	}

	UMaterialExpression* DivE(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B)
	{
		auto* E = NewExpr<UMaterialExpressionDivide>(M);
		E->A.Connect(0, A);
		E->B.Connect(0, B);
		return E;
	}

	UMaterialExpressionScalarParameter* Scalar(UMaterial* M, FName Name, float Default)
	{
		auto* E = NewExpr<UMaterialExpressionScalarParameter>(M);
		E->ParameterName = Name;
		E->DefaultValue = Default;
		return E;
	}

	UMaterialExpressionVectorParameter* Vector(UMaterial* M, FName Name, FLinearColor Default)
	{
		auto* E = NewExpr<UMaterialExpressionVectorParameter>(M);
		E->ParameterName = Name;
		E->DefaultValue = Default;
		return E;
	}

	UMaterialExpressionTextureSampleParameter2D* Sample(UMaterial* M, FName Name, UTexture* Tex, UMaterialExpression* Coords)
	{
		auto* E = NewExpr<UMaterialExpressionTextureSampleParameter2D>(M);
		E->ParameterName = Name;
		E->Texture = Tex;
		E->SamplerType = SAMPLERTYPE_LinearColor;
		if (Coords)
		{
			E->Coordinates.Connect(0, Coords);
		}
		return E;
	}
}

UMaterial* UMallMaterials::BuildMaster(FName Name, bool bUVMapped, int32 Kind)
{
	UMaterial* M = NewObject<UMaterial>(this, Name);
	M->bUsedWithInstancedStaticMeshes = true;
	M->bUsedWithSkeletalMesh = true;
	UMaterialEditorOnlyData* ED = M->GetEditorOnlyData();
	UTexture2D* DefaultPattern = Patterns.FindRef("noise");

	if (Kind == 0)
	{
		UMaterialExpression* Pattern = nullptr;
		UMaterialExpressionScalarParameter* Tile = Scalar(M, "Tile", 100.f);
		if (bUVMapped)
		{
			auto* TC = NewExpr<UMaterialExpressionTextureCoordinate>(M);
			Pattern = Sample(M, "Pattern", DefaultPattern, Mul(M, TC, Tile));
		}
		else
		{
			// Triplanar world-aligned projection: tiles stay consistent across arbitrarily scaled boxes.
			auto* WP = NewExpr<UMaterialExpressionWorldPosition>(M);
			auto* SXY = Sample(M, "Pattern", DefaultPattern, DivE(M, Mask(M, WP, true, true, false), Tile));
			auto* SYZ = Sample(M, "Pattern", DefaultPattern, DivE(M, Mask(M, WP, false, true, true), Tile));
			auto* SXZ = Sample(M, "Pattern", DefaultPattern, DivE(M, Mask(M, WP, true, false, true), Tile));
			auto* N = NewExpr<UMaterialExpressionVertexNormalWS>(M);
			auto* AbsN = NewExpr<UMaterialExpressionAbs>(M);
			AbsN->Input.Connect(0, N);
			UMaterialExpression* WX = Mask(M, AbsN, true, false, false);
			UMaterialExpression* WY = Mask(M, AbsN, false, true, false);
			UMaterialExpression* WZ = Mask(M, AbsN, false, false, true);
			UMaterialExpression* Sum = AddE(M, AddE(M, Mul(M, SXY, WZ), Mul(M, SYZ, WX)), Mul(M, SXZ, WY));
			UMaterialExpression* WSum = AddE(M, AddE(M, WX, WY), WZ);
			Pattern = DivE(M, Sum, WSum);
		}

		UMaterialExpression* PR = Mask(M, Pattern, true, false, false);
		UMaterialExpression* PG = Mask(M, Pattern, false, true, false);

		auto* ColorLerp = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		ColorLerp->A.Connect(0, Vector(M, "ColorB", FLinearColor(0.3f, 0.3f, 0.3f)));
		ColorLerp->B.Connect(0, Vector(M, "ColorA", FLinearColor(0.8f, 0.8f, 0.8f)));
		ColorLerp->Alpha.Connect(0, PR);
		ED->BaseColor.Connect(0, ColorLerp);

		auto* RoughLerp = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		RoughLerp->A.Connect(0, Scalar(M, "RoughB", 0.8f));
		RoughLerp->B.Connect(0, Scalar(M, "RoughA", 0.5f));
		RoughLerp->Alpha.Connect(0, PG);
		ED->Roughness.Connect(0, RoughLerp);

		ED->Metallic.Connect(0, Scalar(M, "Metallic", 0.f));
		ED->EmissiveColor.Connect(0, Vector(M, "Emissive", FLinearColor::Black));
	}
	else if (Kind == 1)
	{
		M->BlendMode = BLEND_Translucent;
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		M->TwoSided = true;
		ED->BaseColor.Connect(0, Vector(M, "ColorA", FLinearColor(0.6f, 0.7f, 0.7f)));
		ED->Opacity.Connect(0, Scalar(M, "Opacity", 0.2f));
		ED->Roughness.Connect(0, Scalar(M, "RoughA", 0.05f));
		ED->Metallic.Connect(0, Scalar(M, "Metallic", 0.f));
		ED->EmissiveColor.Connect(0, Vector(M, "Emissive", FLinearColor::Black));
	}
	else if (Kind == 2)
	{
		M->SetShadingModel(MSM_Unlit);
		ED->EmissiveColor.Connect(0, Vector(M, "Emissive", FLinearColor::White));
	}
	else if (Kind == 4)
	{
		// Zombie clothing zones from mesh-local height: trousers below the belt, skin above the collar.
		auto* TC = NewExpr<UMaterialExpressionTextureCoordinate>(M);
		auto* Pat = Sample(M, "Pattern", Patterns.FindRef("grime"), Mul(M, TC, Scalar(M, "Tile", 3.f)));
		auto* LP = NewExpr<UMaterialExpressionLocalPosition>(M);
		UMaterialExpression* Z = Mask(M, LP, false, false, true);
		auto Band = [&](float Offset, float Scale) -> UMaterialExpression*
		{
			auto* A = NewExpr<UMaterialExpressionAdd>(M);
			A->A.Connect(0, Z);
			A->ConstB = Offset;
			auto* Mu = NewExpr<UMaterialExpressionMultiply>(M);
			Mu->A.Connect(0, A);
			Mu->ConstB = Scale;
			auto* Sat = NewExpr<UMaterialExpressionSaturate>(M);
			Sat->Input.Connect(0, Mu);
			return Sat;
		};
		UMaterialExpression* Pants = Band(-96.f, -0.3f);
		UMaterialExpression* Head = Band(-148.f, 0.3f);
		auto* L1 = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		L1->A.Connect(0, Vector(M, "Shirt", FLinearColor(0.2f, 0.2f, 0.25f)));
		L1->B.Connect(0, Vector(M, "Pants", FLinearColor(0.1f, 0.1f, 0.12f)));
		L1->Alpha.Connect(0, Pants);
		auto* L2 = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		L2->A.Connect(0, L1);
		L2->B.Connect(0, Vector(M, "Skin", FLinearColor(0.35f, 0.36f, 0.3f)));
		L2->Alpha.Connect(0, Head);
		auto* Grime = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		Grime->ConstA = 0.35f;
		Grime->ConstB = 1.0f;
		Grime->Alpha.Connect(0, Mask(M, Pat, true, false, false));
		ED->BaseColor.Connect(0, Mul(M, L2, Grime));
		auto* Rough = NewExpr<UMaterialExpressionLinearInterpolate>(M);
		Rough->ConstA = 0.9f;
		Rough->ConstB = 0.55f;
		Rough->Alpha.Connect(0, Head);
		ED->Roughness.Connect(0, Rough);
	}
	else if (Kind == 3)
	{
		M->SetShadingModel(MSM_Unlit);
		auto* TC = NewExpr<UMaterialExpressionTextureCoordinate>(M);
		auto* Img = Sample(M, "Image", DefaultPattern, TC);
		Img->SamplerType = SAMPLERTYPE_Color;
		ED->EmissiveColor.Connect(0, Mul(M, Img, Scalar(M, "Intensity", 1.f)));
	}

	// Deterministic id so the compiled shaders are reused from the DDC on every launch.
	M->StateId = FGuid(0x4D414C4C, GMasterVersion, (uint32)Kind, bUVMapped ? 1u : 0u);
	M->ForceRecompileForRendering();
	return M;
}

void UMallMaterials::Init()
{
	BuildPatterns();
	MasterWorld = BuildMaster("M_MallWorld", false, 0);
	MasterUV = BuildMaster("M_MallUV", true, 0);
	MasterGlass = BuildMaster("M_MallGlass", false, 1);
	MasterUnlit = BuildMaster("M_MallUnlit", false, 2);
	MasterImage = BuildMaster("M_MallImage", true, 3);
	MasterZombie = BuildMaster("M_MallZombie", true, 4);
	DefineSurfaces();
}

UTexture2D* UMallMaterials::Pattern(FName Name) const
{
	return Patterns.FindRef(Name);
}

void UMallMaterials::DefineSurfaces()
{
	auto Def = [this](FName Name, FName Pattern, FLinearColor A, FLinearColor B, float RA, float RB, float Metal, float Tile, FName Master = TEXT("World"))
	{
		FMallSurface S;
		S.Master = Master; S.Pattern = Pattern; S.ColorA = A; S.ColorB = B; S.RoughA = RA; S.RoughB = RB; S.Metallic = Metal; S.Tile = Tile;
		Surfaces.Add(Name, S);
	};
	auto Emit = [this](FName Name, FLinearColor E)
	{
		FMallSurface S;
		S.Master = TEXT("Unlit");
		S.Emissive = E;
		Surfaces.Add(Name, S);
	};
	auto Glass = [this](FName Name, FLinearColor C, float Opacity, float Rough)
	{
		FMallSurface S;
		S.Master = TEXT("Glass"); S.ColorA = C; S.Opacity = Opacity; S.RoughA = Rough;
		Surfaces.Add(Name, S);
	};

	// Floors
	Def("floor_main", "tile", FLinearColor(0.62f, 0.57f, 0.50f), FLinearColor(0.25f, 0.23f, 0.21f), 0.18f, 0.6f, 0.f, 240.f);
	Def("floor_upper", "tile", FLinearColor(0.50f, 0.50f, 0.49f), FLinearColor(0.18f, 0.18f, 0.18f), 0.22f, 0.6f, 0.f, 240.f);
	Def("floor_food", "checker", FLinearColor(0.70f, 0.66f, 0.58f), FLinearColor(0.20f, 0.08f, 0.05f), 0.3f, 0.45f, 0.f, 240.f);
	Def("floor_vinyl", "tile_small", FLinearColor(0.78f, 0.80f, 0.82f), FLinearColor(0.45f, 0.47f, 0.50f), 0.25f, 0.5f, 0.f, 240.f);
	Def("floor_wood", "wood", FLinearColor(0.42f, 0.28f, 0.17f), FLinearColor(0.20f, 0.12f, 0.07f), 0.35f, 0.55f, 0.f, 300.f);
	Def("floor_carpet", "carpet", FLinearColor(0.35f, 0.08f, 0.10f), FLinearColor(0.06f, 0.04f, 0.06f), 0.95f, 0.95f, 0.f, 160.f);
	Def("floor_concrete", "concrete", FLinearColor(0.42f, 0.42f, 0.40f), FLinearColor(0.22f, 0.22f, 0.21f), 0.75f, 0.9f, 0.f, 300.f);
	Def("floor_plate", "plate", FLinearColor(0.45f, 0.46f, 0.47f), FLinearColor(0.2f, 0.2f, 0.2f), 0.35f, 0.6f, 0.9f, 120.f);
	Def("carpet_grey", "fabric", FLinearColor(0.22f, 0.24f, 0.27f), FLinearColor(0.12f, 0.13f, 0.15f), 0.95f, 0.95f, 0.f, 60.f);

	// Walls / ceilings
	Def("plaster", "plaster", FLinearColor(0.78f, 0.75f, 0.70f), FLinearColor(0.60f, 0.57f, 0.53f), 0.8f, 0.9f, 0.f, 200.f);
	Def("plaster_warm", "plaster", FLinearColor(0.72f, 0.62f, 0.50f), FLinearColor(0.55f, 0.46f, 0.37f), 0.8f, 0.9f, 0.f, 200.f);
	Def("plaster_dark", "plaster", FLinearColor(0.10f, 0.10f, 0.11f), FLinearColor(0.05f, 0.05f, 0.06f), 0.85f, 0.9f, 0.f, 200.f);
	Def("plaster_mint", "plaster", FLinearColor(0.70f, 0.82f, 0.80f), FLinearColor(0.55f, 0.66f, 0.64f), 0.7f, 0.85f, 0.f, 200.f);
	Def("plaster_white", "plaster", FLinearColor(0.85f, 0.86f, 0.87f), FLinearColor(0.70f, 0.71f, 0.72f), 0.6f, 0.8f, 0.f, 200.f);
	Def("plaster_olive", "plaster", FLinearColor(0.36f, 0.38f, 0.30f), FLinearColor(0.26f, 0.28f, 0.22f), 0.8f, 0.9f, 0.f, 200.f);
	Def("plaster_navy", "plaster", FLinearColor(0.10f, 0.13f, 0.20f), FLinearColor(0.06f, 0.08f, 0.12f), 0.7f, 0.85f, 0.f, 200.f);
	Def("concrete", "concrete", FLinearColor(0.50f, 0.50f, 0.48f), FLinearColor(0.28f, 0.28f, 0.27f), 0.8f, 0.95f, 0.f, 300.f);
	Def("ceiling", "ceiling", FLinearColor(0.82f, 0.82f, 0.80f), FLinearColor(0.35f, 0.35f, 0.35f), 0.9f, 0.6f, 0.f, 120.f);
	Def("stone_clad", "tile", FLinearColor(0.58f, 0.55f, 0.50f), FLinearColor(0.35f, 0.33f, 0.30f), 0.35f, 0.7f, 0.f, 300.f);

	// Metals
	Def("steel", "brushed", FLinearColor(0.75f, 0.75f, 0.76f), FLinearColor(0.55f, 0.55f, 0.56f), 0.28f, 0.42f, 1.f, 80.f);
	Def("steel_dark", "brushed", FLinearColor(0.22f, 0.22f, 0.23f), FLinearColor(0.12f, 0.12f, 0.13f), 0.35f, 0.5f, 1.f, 80.f);
	Def("bronze", "brushed", FLinearColor(0.32f, 0.22f, 0.14f), FLinearColor(0.20f, 0.14f, 0.09f), 0.32f, 0.45f, 1.f, 80.f);
	Def("chrome", "brushed", FLinearColor(0.9f, 0.9f, 0.9f), FLinearColor(0.8f, 0.8f, 0.8f), 0.08f, 0.15f, 1.f, 80.f);
	Def("painted_metal", "noise", FLinearColor(0.30f, 0.33f, 0.36f), FLinearColor(0.22f, 0.24f, 0.26f), 0.45f, 0.6f, 0.3f, 100.f);
	Def("painted_yellow", "noise", FLinearColor(0.75f, 0.55f, 0.05f), FLinearColor(0.55f, 0.40f, 0.04f), 0.5f, 0.7f, 0.1f, 100.f);
	Def("painted_red", "noise", FLinearColor(0.55f, 0.05f, 0.04f), FLinearColor(0.40f, 0.04f, 0.03f), 0.5f, 0.7f, 0.1f, 100.f);
	Def("painted_green", "noise", FLinearColor(0.10f, 0.25f, 0.15f), FLinearColor(0.06f, 0.16f, 0.10f), 0.5f, 0.7f, 0.1f, 100.f);
	Def("locker", "brushed", FLinearColor(0.32f, 0.36f, 0.40f), FLinearColor(0.25f, 0.28f, 0.31f), 0.45f, 0.55f, 0.7f, 80.f);

	// Woods / plastics / fabrics
	Def("wood", "wood", FLinearColor(0.50f, 0.34f, 0.20f), FLinearColor(0.28f, 0.18f, 0.10f), 0.45f, 0.6f, 0.f, 150.f);
	Def("wood_light", "wood", FLinearColor(0.72f, 0.58f, 0.40f), FLinearColor(0.52f, 0.40f, 0.26f), 0.45f, 0.6f, 0.f, 150.f);
	Def("wood_dark", "wood", FLinearColor(0.22f, 0.14f, 0.09f), FLinearColor(0.10f, 0.06f, 0.04f), 0.4f, 0.55f, 0.f, 150.f);
	Def("laminate_white", "noise", FLinearColor(0.82f, 0.82f, 0.80f), FLinearColor(0.74f, 0.74f, 0.72f), 0.3f, 0.4f, 0.f, 100.f);
	Def("plastic_black", "noise", FLinearColor(0.035f, 0.035f, 0.04f), FLinearColor(0.02f, 0.02f, 0.025f), 0.45f, 0.6f, 0.f, 50.f);
	Def("plastic_grey", "noise", FLinearColor(0.35f, 0.36f, 0.38f), FLinearColor(0.28f, 0.29f, 0.3f), 0.5f, 0.6f, 0.f, 50.f);
	Def("plastic_white", "noise", FLinearColor(0.85f, 0.85f, 0.84f), FLinearColor(0.75f, 0.75f, 0.74f), 0.35f, 0.5f, 0.f, 50.f);
	Def("plastic_red", "noise", FLinearColor(0.6f, 0.06f, 0.05f), FLinearColor(0.45f, 0.04f, 0.04f), 0.4f, 0.5f, 0.f, 50.f);
	Def("plastic_orange", "noise", FLinearColor(0.85f, 0.32f, 0.04f), FLinearColor(0.65f, 0.22f, 0.03f), 0.4f, 0.5f, 0.f, 50.f);
	Def("plastic_blue", "noise", FLinearColor(0.05f, 0.18f, 0.5f), FLinearColor(0.04f, 0.12f, 0.35f), 0.4f, 0.5f, 0.f, 50.f);
	Def("plastic_green", "noise", FLinearColor(0.12f, 0.45f, 0.18f), FLinearColor(0.08f, 0.32f, 0.12f), 0.4f, 0.5f, 0.f, 50.f);
	Def("plastic_yellow", "noise", FLinearColor(0.85f, 0.65f, 0.08f), FLinearColor(0.7f, 0.5f, 0.05f), 0.4f, 0.5f, 0.f, 50.f);
	Def("rubber", "tread", FLinearColor(0.05f, 0.05f, 0.05f), FLinearColor(0.02f, 0.02f, 0.02f), 0.7f, 0.9f, 0.f, 30.f);
	Def("fabric_red", "fabric", FLinearColor(0.45f, 0.06f, 0.07f), FLinearColor(0.30f, 0.04f, 0.05f), 0.95f, 0.95f, 0.f, 25.f);
	Def("fabric_blue", "fabric", FLinearColor(0.10f, 0.16f, 0.35f), FLinearColor(0.06f, 0.10f, 0.24f), 0.95f, 0.95f, 0.f, 25.f);
	Def("fabric_grey", "fabric", FLinearColor(0.35f, 0.35f, 0.36f), FLinearColor(0.22f, 0.22f, 0.23f), 0.95f, 0.95f, 0.f, 25.f);
	Def("fabric_beige", "fabric", FLinearColor(0.62f, 0.54f, 0.42f), FLinearColor(0.45f, 0.39f, 0.30f), 0.95f, 0.95f, 0.f, 25.f);
	Def("fabric_green", "fabric", FLinearColor(0.20f, 0.30f, 0.18f), FLinearColor(0.12f, 0.20f, 0.11f), 0.95f, 0.95f, 0.f, 25.f);
	Def("fabric_black", "fabric", FLinearColor(0.04f, 0.04f, 0.045f), FLinearColor(0.02f, 0.02f, 0.025f), 0.95f, 0.95f, 0.f, 25.f);
	Def("leather", "noise", FLinearColor(0.20f, 0.10f, 0.05f), FLinearColor(0.12f, 0.06f, 0.03f), 0.5f, 0.7f, 0.f, 30.f);
	Def("cardboard", "noise", FLinearColor(0.55f, 0.40f, 0.24f), FLinearColor(0.45f, 0.32f, 0.19f), 0.85f, 0.9f, 0.f, 60.f);
	Def("paper", "noise", FLinearColor(0.86f, 0.85f, 0.80f), FLinearColor(0.78f, 0.77f, 0.72f), 0.85f, 0.9f, 0.f, 60.f);
	Def("soil", "concrete", FLinearColor(0.16f, 0.11f, 0.07f), FLinearColor(0.08f, 0.06f, 0.04f), 0.95f, 1.f, 0.f, 80.f);
	Def("foliage", "fabric", FLinearColor(0.08f, 0.20f, 0.06f), FLinearColor(0.04f, 0.10f, 0.03f), 0.7f, 0.85f, 0.f, 40.f);
	Def("tarmac", "concrete", FLinearColor(0.10f, 0.10f, 0.10f), FLinearColor(0.05f, 0.05f, 0.05f), 0.8f, 0.95f, 0.f, 400.f);

	// Weapon / prop materials (UV-mapped for moving objects).
	Def("gun_metal", "brushed", FLinearColor(0.08f, 0.08f, 0.085f), FLinearColor(0.04f, 0.04f, 0.045f), 0.32f, 0.45f, 1.f, 2.f, TEXT("UV"));
	Def("gun_polymer", "noise", FLinearColor(0.045f, 0.045f, 0.045f), FLinearColor(0.03f, 0.03f, 0.03f), 0.55f, 0.7f, 0.f, 3.f, TEXT("UV"));
	Def("gun_tan", "noise", FLinearColor(0.42f, 0.34f, 0.22f), FLinearColor(0.33f, 0.26f, 0.17f), 0.55f, 0.7f, 0.f, 3.f, TEXT("UV"));
	Def("gun_wood", "wood", FLinearColor(0.38f, 0.20f, 0.09f), FLinearColor(0.20f, 0.10f, 0.04f), 0.4f, 0.55f, 0.f, 1.f, TEXT("UV"));
	Def("gun_olive", "noise", FLinearColor(0.16f, 0.18f, 0.11f), FLinearColor(0.11f, 0.12f, 0.07f), 0.55f, 0.7f, 0.f, 3.f, TEXT("UV"));
	Def("gun_steel", "brushed", FLinearColor(0.55f, 0.55f, 0.56f), FLinearColor(0.4f, 0.4f, 0.41f), 0.25f, 0.4f, 1.f, 2.f, TEXT("UV"));
	Def("brass", "brushed", FLinearColor(0.75f, 0.55f, 0.22f), FLinearColor(0.6f, 0.42f, 0.16f), 0.25f, 0.35f, 1.f, 1.f, TEXT("UV"));
	Def("glove", "fabric", FLinearColor(0.07f, 0.07f, 0.065f), FLinearColor(0.04f, 0.04f, 0.035f), 0.85f, 0.9f, 0.f, 3.f, TEXT("UV"));
	Def("sleeve", "fabric", FLinearColor(0.12f, 0.14f, 0.16f), FLinearColor(0.07f, 0.08f, 0.09f), 0.9f, 0.95f, 0.f, 3.f, TEXT("UV"));
	Def("prop_red", "noise", FLinearColor(0.6f, 0.06f, 0.05f), FLinearColor(0.45f, 0.04f, 0.04f), 0.4f, 0.5f, 0.f, 2.f, TEXT("UV"));
	Def("prop_white", "noise", FLinearColor(0.85f, 0.85f, 0.84f), FLinearColor(0.75f, 0.75f, 0.74f), 0.4f, 0.5f, 0.f, 2.f, TEXT("UV"));
	Def("prop_green", "noise", FLinearColor(0.10f, 0.35f, 0.15f), FLinearColor(0.07f, 0.25f, 0.1f), 0.4f, 0.5f, 0.f, 2.f, TEXT("UV"));
	Def("prop_olive", "noise", FLinearColor(0.20f, 0.22f, 0.13f), FLinearColor(0.14f, 0.15f, 0.09f), 0.6f, 0.75f, 0.f, 2.f, TEXT("UV"));
	Def("prop_dark", "noise", FLinearColor(0.06f, 0.06f, 0.065f), FLinearColor(0.04f, 0.04f, 0.045f), 0.5f, 0.6f, 0.f, 2.f, TEXT("UV"));
	Def("prop_orange", "noise", FLinearColor(0.85f, 0.32f, 0.04f), FLinearColor(0.65f, 0.22f, 0.03f), 0.4f, 0.5f, 0.f, 2.f, TEXT("UV"));
	Def("prop_copper", "brushed", FLinearColor(0.75f, 0.42f, 0.25f), FLinearColor(0.6f, 0.32f, 0.18f), 0.3f, 0.4f, 1.f, 2.f, TEXT("UV"));

	// Zombie skin / clothing (UV mapped on the skeletal mesh).
	Def("z_skin", "grime", FLinearColor(0.42f, 0.42f, 0.36f), FLinearColor(0.16f, 0.12f, 0.10f), 0.55f, 0.75f, 0.f, 3.f, TEXT("UV"));

	// Glass
	Glass("glass", FLinearColor(0.55f, 0.65f, 0.65f), 0.16f, 0.03f);
	Glass("glass_dark", FLinearColor(0.15f, 0.18f, 0.2f), 0.45f, 0.05f);
	Glass("glass_sky", FLinearColor(0.75f, 0.85f, 0.9f), 0.08f, 0.02f);
	Glass("glass_frost", FLinearColor(0.85f, 0.88f, 0.9f), 0.55f, 0.4f);

	// Emissive
	Emit("light_warm", FLinearColor(18.f, 13.f, 8.f));
	Emit("light_cool", FLinearColor(12.f, 14.f, 17.f));
	Emit("light_dim", FLinearColor(2.f, 1.7f, 1.3f));
	Emit("light_red", FLinearColor(25.f, 0.6f, 0.3f));
	Emit("light_green", FLinearColor(0.4f, 12.f, 2.f));
	Emit("light_amber", FLinearColor(14.f, 6.f, 0.5f));
	Emit("screen_off", FLinearColor(0.01f, 0.012f, 0.015f));
	Emit("screen_blue", FLinearColor(0.6f, 1.2f, 2.4f));
	Emit("screen_static", FLinearColor(1.2f, 1.25f, 1.3f));
	Emit("sign_white", FLinearColor(6.f, 6.f, 6.f));
	Emit("marker", FLinearColor(1.5f, 3.0f, 0.6f));
	Emit("muzzle", FLinearColor(60.f, 32.f, 10.f));
	Emit("black", FLinearColor(0.f, 0.f, 0.f));
	Emit("blood", FLinearColor(0.06f, 0.0f, 0.0f));
}

UMaterialInstanceDynamic* UMallMaterials::CreateFromSurface(const FMallSurface& S)
{
	UMaterial* Base = MasterWorld;
	if (S.Master == "UV") Base = MasterUV;
	else if (S.Master == "Glass") Base = MasterGlass;
	else if (S.Master == "Unlit") Base = MasterUnlit;

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	MID->SetVectorParameterValue("ColorA", S.ColorA);
	MID->SetVectorParameterValue("ColorB", S.ColorB);
	MID->SetScalarParameterValue("RoughA", S.RoughA);
	MID->SetScalarParameterValue("RoughB", S.RoughB);
	MID->SetScalarParameterValue("Metallic", S.Metallic);
	MID->SetScalarParameterValue("Tile", S.Tile);
	MID->SetScalarParameterValue("Opacity", S.Opacity);
	MID->SetVectorParameterValue("Emissive", S.Emissive);
	if (UTexture2D* P = Patterns.FindRef(S.Pattern))
	{
		MID->SetTextureParameterValue("Pattern", P);
	}
	return MID;
}

UMaterialInterface* UMallMaterials::Mat(FName Surface)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Surface))
	{
		return *Found;
	}
	const FMallSurface* S = Surfaces.Find(Surface);
	if (!S)
	{
		UE_LOG(LogTemp, Warning, TEXT("MallMaterials: unknown surface %s"), *Surface.ToString());
		static const FMallSurface Fallback;
		S = &Fallback;
	}
	UMaterialInstanceDynamic* MID = CreateFromSurface(*S);
	Cache.Add(Surface, MID);
	return MID;
}

UMaterialInstanceDynamic* UMallMaterials::MakeDynamic(FName Surface)
{
	const FMallSurface* S = Surfaces.Find(Surface);
	static const FMallSurface Fallback;
	return CreateFromSurface(S ? *S : Fallback);
}

UMaterialInstanceDynamic* UMallMaterials::MakeImageMaterial(UTexture* Image, float Intensity)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterImage, this);
	MID->SetTextureParameterValue("Image", Image);
	MID->SetScalarParameterValue("Intensity", Intensity);
	return MID;
}

UMaterialInstanceDynamic* UMallMaterials::MakeZombie(const FLinearColor& Skin, const FLinearColor& Shirt, const FLinearColor& Pants, float Tile)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterZombie, this);
	MID->SetVectorParameterValue("Skin", Skin);
	MID->SetVectorParameterValue("Shirt", Shirt);
	MID->SetVectorParameterValue("Pants", Pants);
	MID->SetScalarParameterValue("Tile", Tile);
	return MID;
}

UMaterialInterface* UMallMaterials::Solid(const FLinearColor& Color, float Rough, float Metal, FName Master)
{
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(Color.ToFColor(true)), GetTypeHash(int32(Rough * 100) + int32(Metal * 10000))), GetTypeHash(Master));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = SolidCache.Find(Key))
	{
		return *Found;
	}
	FMallSurface S;
	S.Master = Master;
	S.Pattern = TEXT("noise");
	S.ColorA = Color;
	S.ColorB = Color * 0.8f;
	S.RoughA = Rough;
	S.RoughB = FMath::Min(1.f, Rough + 0.1f);
	S.Metallic = Metal;
	S.Tile = Master == "UV" ? 2.f : 100.f;
	if (Master == "Unlit")
	{
		S.Emissive = Color;
	}
	UMaterialInstanceDynamic* MID = CreateFromSurface(S);
	SolidCache.Add(Key, MID);
	return MID;
}
