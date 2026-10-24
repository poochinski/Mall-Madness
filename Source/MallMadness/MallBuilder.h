#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallNav.h"
#include "MallBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;
class UPointLightComponent;
class URectLightComponent;
class UStaticMeshComponent;
class AMallInteractable;
class ULightComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class UTexture;

/** Meters -> Unreal centimetres. */
FORCEINLINE FVector M(double X, double Y, double Z) { return FVector(X, Y, Z) * 100.0; }

/** Named area of the mall used for objectives, spawns and the minimap clue. */
struct FMallArea
{
	FString Name;
	FBox Bounds;
	int32 Floor = 0;
};

/**
 * Procedurally constructs the whole two-storey mall: architecture, props, lights, signage,
 * loot placement and the zombie navigation graph. Geometry is batched into instanced meshes
 * keyed by mesh+material so thousands of parts stay cheap to render.
 */
UCLASS()
class AMallBuilder : public AActor
{
	GENERATED_BODY()

public:
	AMallBuilder();

	void Build();

	// ---- primitives (all positions/sizes in centimetres) ----
	void Box(const FVector& Center, const FVector& Size, FName Mat, bool bCollide = true, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true);
	void Cyl(const FVector& Center, float Radius, float Height, FName Mat, bool bCollide = false, const FRotator& Rot = FRotator::ZeroRotator);
	void Sphere(const FVector& Center, const FVector& Size, FName Mat, bool bCollide = false);
	void Cone(const FVector& Center, float Radius, float Height, FName Mat, const FRotator& Rot = FRotator::ZeroRotator);
	/** Invisible blocking volume (ramps over stairs, rail caps, etc). */
	void Collider(const FVector& Center, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator, bool bBlockShots = false);
	UTextRenderComponent* Text(const FString& Str, const FVector& Pos, const FRotator& Rot, float Height, const FColor& Color, bool bCenter = true);
	UPointLightComponent* Point(const FVector& Pos, const FLinearColor& Color, float Intensity, float Radius, bool bShadow = false);
	URectLightComponent* Rect(const FVector& Pos, const FRotator& Rot, const FLinearColor& Color, float Intensity, float Radius, float W, float H, bool bShadow = false);
	UStaticMeshComponent* Panel(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);

	/** Wall along X (at fixed Y) or along Y (at fixed X) from A to B with door openings (start,end pairs along the wall). */
	void WallX(double X0, double X1, double Y, double Z0, double Z1, double T, FName Mat, const TArray<FVector2D>& Openings = {}, double OpenTop = 3.2);
	void WallY(double Y0, double Y1, double X, double Z0, double Z1, double T, FName Mat, const TArray<FVector2D>& Openings = {}, double OpenTop = 3.2);

	FMallNavGraph Nav;
	TArray<FMallArea> Areas;
	TArray<FVector> ZombieSpawns;      // cm
	TArray<FVector> StartZombies;      // cm, initial population spots
	FVector PlayerStart = FVector::ZeroVector;
	FRotator PlayerStartRot = FRotator::ZeroRotator;
	FVector ExitLocation = FVector::ZeroVector;
	FVector RadioLocation = FVector::ZeroVector;
	FVector FuseLocation = FVector::ZeroVector;
	FVector MaintenanceDoor = FVector::ZeroVector;
	FVector ElectronicsDoor = FVector::ZeroVector;
	FVector SecurityDoor = FVector::ZeroVector;
	FVector StairBottom = FVector::ZeroVector, StairTop = FVector::ZeroVector;
	FVector EscBottom = FVector::ZeroVector, EscTop = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<AMallInteractable> RadioItem;
	UPROPERTY() TObjectPtr<AMallInteractable> ExitItem;
	UPROPERTY() TObjectPtr<AMallInteractable> FuseItem;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> EmergencyGlow;
	UPROPERTY() TArray<TObjectPtr<ULightComponent>> NormalLights;

	UPROPERTY() TArray<TObjectPtr<AMallInteractable>> Interactables;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ExitShutter;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> EmergencyLights;

	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* CylMesh = nullptr;
	UStaticMesh* SphereMesh = nullptr;
	UStaticMesh* ConeMesh = nullptr;
	UStaticMesh* PlaneMesh = nullptr;

private:
	UInstancedStaticMeshComponent* GetISM(UStaticMesh* Mesh, FName Mat, bool bCollide, bool bShadow);

	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> ISMs;
	int32 LightCount = 0;

	// ---- layout sections (MallLayout.cpp) ----
	void BuildShell();
	void BuildAtrium();
	void BuildVerticalRoutes();
	void BuildConcourseDressing();
	void BuildFoodCourt();
	void BuildClothing();
	void BuildSporting();
	void BuildSecurity();
	void BuildTheater();
	void BuildPharmacy();
	void BuildElectronics();
	void BuildMaintenance();
	void BuildClosedShops();
	void BuildServiceAreas();
	void BuildExterior();
	void BuildLighting();
	void BuildNavGraph();
	void PlaceLoot();

	// ---- prop kit (MallProps.cpp) ----
public:
	void Bench(const FVector& P, float Yaw);
	void Planter(const FVector& P, float W, float D);
	void TrashCan(const FVector& P);
	void Table(const FVector& P, bool bRound, float Yaw = 0.f);
	void Chair(const FVector& P, float Yaw, bool bTipped = false);
	void Shelf(const FVector& P, float Len, float Height, float Yaw, FName Mat, int32 Seed, int32 Kind);
	void Counter(const FVector& P, const FVector& Size, FName Top, FName Body);
	void ClothesRack(const FVector& P, float Yaw, int32 Seed);
	void Mannequin(const FVector& P, float Yaw, FName Shirt, FName Pants, bool bFallen = false);
	void Tray(const FVector& P, float Yaw, int32 Seed);
	void ShoppingBag(const FVector& P, float Yaw, FName Mat);
	void Suitcase(const FVector& P, float Yaw, FName Mat, bool bFallen);
	void Papers(const FVector& P, int32 Count, float Spread, int32 Seed);
	void Vending(const FVector& P, float Yaw, FName Body, FName Glow);
	void Storefront(double X0, double X1, double Y, double Floor, bool bFacingNorth, const FString& Name, FName FrameMat, const FColor& SignColor, double DoorW, bool bOpen, FName ShutterMat = NAME_None);
	void CeilingLights(const FBox& Area, double Z, double Spacing, FName Mat, const FLinearColor& Color, float Intensity, bool bAddLights);
	void Crate(const FVector& P, const FVector& Size, float Yaw);
	void Poster(const FVector& P, const FRotator& Rot, float W, float H, const FString& Title, const FString& Sub, const FColor& Bg, const FColor& Fg);
	void Screen(const FVector& P, const FRotator& Rot, float W, float H, FName Emissive);
	UTexture* MakeTextTexture(int32 W, int32 H, const FColor& Bg, const TArray<TPair<FString, FColor>>& Lines, float FontScale);
	AMallInteractable* Loot(uint8 Item, int32 Amount, const FVector& At, float Yaw, const FString& Prompt, const FString& Label = FString());
	void GunProp(AMallInteractable* I, int32 Slot, const FVector& Rel, const FRotator& Rot);
	void Railing(const FVector& A, const FVector& B);
	void SlopedRail(const FVector& A, const FVector& B, bool bGlass);
	void Ramp(const FVector& Bottom, const FVector& Top, double Width);
	void StoreShell(double X0, double X1, double Y0, double Y1, double F, FName Wall, FName Floor, double CeilH);
	void HangingSign(const FVector& P, float Yaw, const FString& Line1, float W);
	void NavRegion(double X0, double X1, double Y0, double Y1, double F, double Spacing);
	void Spawn(double X, double Y, double F, bool bStart = false);

	struct FNavRegion { FBox Box; double Spacing; };
	TArray<FNavRegion> NavRegions;
};
