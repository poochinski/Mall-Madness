#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallInteractable.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPointLightComponent;

UENUM()
enum class EMallItem : uint8
{
	None,
	AmmoPistol,
	AmmoRifle,
	AmmoShells,
	AmmoSniper,
	Medkit,
	Food,
	WeaponAR,
	WeaponShotgun,
	WeaponSniper,
	Fuse,
	Radio,
	Container,
	Note,
	Exit
};

USTRUCT()
struct FMallLoot
{
	GENERATED_BODY()
	UPROPERTY() EMallItem Item = EMallItem::None;
	UPROPERTY() int32 Amount = 0;
};

/**
 * Anything the player can press E on: loose pickups, searchable containers, weapon racks,
 * the fuse, the emergency radio and the exit doors. Loot is finite: after use a pickup
 * disappears and a container stays in place but is flagged empty.
 */
UCLASS()
class AMallInteractable : public AActor
{
	GENERATED_BODY()

public:
	AMallInteractable();

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Rel, const FVector& SizeCm, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);
	void AddGlint(const FVector& Rel);
	void SetUsed();
	/** Centre used for the look-at test. */
	FVector FocusPoint() const { return GetActorLocation() + FocusOffset; }

	EMallItem Item = EMallItem::None;
	int32 Amount = 0;
	TArray<FMallLoot> Contents;
	FString Prompt;
	FString Label;
	bool bUsed = false;
	bool bKeepAfterUse = false;   // containers / radio stay visible
	bool bEnabled = true;          // e.g. the exit only becomes usable at the end
	float UseTime = 0.f;           // hold time (radio repair)
	FVector FocusOffset = FVector::ZeroVector;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Glint;
	UPROPERTY() TObjectPtr<UPointLightComponent> StatusLight;
};
