#include "MallInteractable.h"

#include "MallMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"

AMallInteractable::AMallInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AMallInteractable::AddPart(UStaticMesh* Mesh, const FVector& Rel, const FVector& SizeCm, UMaterialInterface* Mat, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Mat);
	C->SetupAttachment(RootComponent);
	C->SetRelativeTransform(FTransform(Rot, Rel, SizeCm / 100.0));
	// Items block shots and sight but never the player's movement, so they can't snag anyone.
	C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	C->SetCollisionObjectType(ECC_WorldDynamic);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

void AMallInteractable::AddGlint(const FVector& Rel)
{
	// A small, faint price-tag style marker: readable up close, invisible across the room.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Glint = NewObject<UStaticMeshComponent>(this);
	Glint->SetStaticMesh(Cube);
	Glint->SetMaterial(0, UMallMaterials::Get()->Mat("marker"));
	Glint->SetupAttachment(RootComponent);
	Glint->SetRelativeTransform(FTransform(FRotator(0, 0, 0), Rel, FVector(0.035, 0.035, 0.035)));
	Glint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glint->SetCastShadow(false);
	Glint->RegisterComponent();
}

void AMallInteractable::SetUsed()
{
	bUsed = true;
	if (Glint)
	{
		Glint->SetVisibility(false);
	}
	if (!bKeepAfterUse)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}
