#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MallZombie.generated.h"

class AMallPlayer;
class UAnimationAsset;
struct FMallNavGraph;

UENUM()
enum class EZombieKind : uint8 { Walker, Runner, Brute };

UENUM()
enum class EZombieState : uint8 { Idle, Wander, Investigate, Chase, Attack, Stagger, Dead };

UCLASS()
class AMallZombie : public ACharacter
{
	GENERATED_BODY()

public:
	AMallZombie();

	void Setup(EZombieKind InKind, int32 Seed);
	virtual void Tick(float Dt) override;

	/** Returns true if the hit killed the zombie. */
	bool ApplyHit(float Damage, bool bHead, const FVector& Dir, AActor* InInstigator);
	void HearNoise(const FVector& Location, float Radius);
	bool IsDead() const { return State == EZombieState::Dead; }

	EZombieKind Kind = EZombieKind::Walker;
	EZombieState State = EZombieState::Idle;
	float Health = 100.f;
	float DeathTime = 0.f;
	float StuckTime = 0.f;

private:
	void Think();
	void PlanTo(const FVector& Target);
	bool DirectReachable(const FVector& From, const FVector& To) const;
	bool CanSeePlayer(AMallPlayer* P) const;
	void FollowPath(float Dt);
	void PlayLocomotion();
	void PlayOnce(UAnimationAsset* Anim, float Rate);
	void BeginAttack();
	void Die(const FVector& Dir);
	AMallPlayer* GetPlayer() const;
	const FMallNavGraph* GetNav() const;

	float Speed = 120.f;
	float AttackDamage = 15.f;
	float AttackCooldown = 1.5f;
	float AttackWindup = 0.45f;
	float AttackLength = 1.1f;
	float SightRange = 2400.f;
	float AnimRate = 0.8f;

	float ThinkTimer = 0.f;
	float StateTimer = 0.f;
	float CooldownTimer = 0.f;
	float LastSeenTime = -100.f;
	float RepathTimer = 0.f;
	float VoiceTimer = 3.f;
	float Age = 0.f;
	bool bAttackLanded = false;
	bool bLocomotionActive = false;
	FVector TargetLoc = FVector::ZeroVector;
	FVector LastProgressLoc = FVector::ZeroVector;
	float ProgressTimer = 0.f;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	FVector WanderHome = FVector::ZeroVector;
	int32 Variant = 0;

	UPROPERTY() TObjectPtr<UAnimationAsset> LocoAnim;
	UPROPERTY() TArray<TObjectPtr<UAnimationAsset>> AttackAnims;
	UPROPERTY() TArray<TObjectPtr<UAnimationAsset>> HitAnims;
	UPROPERTY() TArray<TObjectPtr<UAnimationAsset>> DeathAnims;
};
