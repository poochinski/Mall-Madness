#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MallMaterials.generated.h"

class UMaterial;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

/** Surface preset: which master, which procedural pattern, and its tint/roughness values. */
struct FMallSurface
{
	FName Master = TEXT("World");     // World (triplanar), UV, Glass, Unlit
	FName Pattern = TEXT("noise");
	FLinearColor ColorA = FLinearColor::White;  // pattern high
	FLinearColor ColorB = FLinearColor::Gray;   // pattern low
	float RoughA = 0.6f;
	float RoughB = 0.8f;
	float Metallic = 0.f;
	float Tile = 100.f;               // world cm per texture repeat (World) or UV repeats (UV)
	FLinearColor Emissive = FLinearColor::Black;
	float Opacity = 0.25f;
};

/**
 * Builds every material the game uses at runtime: procedural tileable textures with full mip chains,
 * a few master materials built from expression graphs, and cached dynamic instances per surface name.
 */
UCLASS()
class UMallMaterials : public UObject
{
	GENERATED_BODY()

public:
	static UMallMaterials* Get();
	static void Shutdown();

	void Init();

	/** Cached material for a named surface preset (e.g. "floor_main", "glass"). */
	UMaterialInterface* Mat(FName Surface);
	/** New dynamic instance of a surface preset that the caller can tweak (emissive pulses etc). */
	UMaterialInstanceDynamic* MakeDynamic(FName Surface);
	/** Emissive unlit material showing a texture (posters, screens, menu boards). */
	UMaterialInstanceDynamic* MakeImageMaterial(UTexture* Image, float Intensity);
	/** Solid-colour material created on the fly and cached by colour. */
	UMaterialInstanceDynamic* MakeZombie(const FLinearColor& Skin, const FLinearColor& Shirt, const FLinearColor& Pants, float Tile);
	UMaterialInterface* Solid(const FLinearColor& Color, float Rough = 0.6f, float Metal = 0.f, FName Master = TEXT("World"));

	UTexture2D* Pattern(FName Name) const;

	UPROPERTY() TObjectPtr<UMaterial> MasterWorld;
	UPROPERTY() TObjectPtr<UMaterial> MasterUV;
	UPROPERTY() TObjectPtr<UMaterial> MasterGlass;
	UPROPERTY() TObjectPtr<UMaterial> MasterUnlit;
	UPROPERTY() TObjectPtr<UMaterial> MasterImage;
	UPROPERTY() TObjectPtr<UMaterial> MasterZombie;

private:
	UMaterial* BuildMaster(FName Name, bool bUVMapped, int32 Kind);
	void BuildPatterns();
	void DefineSurfaces();
	UMaterialInstanceDynamic* CreateFromSurface(const FMallSurface& S);

	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> Patterns;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> Cache;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> SolidCache;
	TMap<FName, FMallSurface> Surfaces;

	static UMallMaterials* Instance;
};

/** Helpers for procedural texture generation. */
namespace MallTex
{
	UTexture2D* MakeTexture(const TArray<FColor>& Pixels, int32 Size, bool bSRGB);
}
