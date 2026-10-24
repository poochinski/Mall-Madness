#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallFX.generated.h"

class UStaticMeshComponent;

/** Bounded, pooled combat effects: bullet holes, impact debris, blood puffs and ejected shells. */
UCLASS()
class AMallFX : public AActor
{
	GENERATED_BODY()

public:
	AMallFX();
	static AMallFX* Get(UWorld* World);

	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Surface: 0 = world, 1 = flesh. */
	void Impact(const FVector& Loc, const FVector& Normal, int32 Surface);
	void Shell(const FVector& Loc, const FVector& Vel, int32 Kind);

private:
	struct FParticle
	{
		UStaticMeshComponent* Comp = nullptr;
		FVector Vel = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Life = 0.f;
		float MaxLife = 1.f;
		float Size = 1.f;
		float FloorZ = -1e9f;
		bool bShell = false;
	};

	UStaticMeshComponent* MakeComp(UStaticMesh* Mesh, FName Mat);
	FParticle& NextParticle(TArray<FParticle>& Pool, int32& Cursor);

	TArray<FParticle> Debris;
	TArray<FParticle> Shells;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Holes;
	int32 DebrisCursor = 0;
	int32 ShellCursor = 0;
	int32 HoleCursor = 0;
};
