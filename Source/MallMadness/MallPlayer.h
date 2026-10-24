#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MallWeapons.h"
#include "MallPlayer.generated.h"

class UCameraComponent;
class UPointLightComponent;
class AMallInteractable;
class AMallPlayerController;

UCLASS()
class AMallPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	AMallPlayer();

	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// ---- gameplay API (also used by the automated test harness) ----
	void TryFire(bool bJustPressed);
	void StartReload();
	void SwitchTo(int32 Slot);
	void Cycle(int32 Dir);
	void UseMedkit();
	bool Interact();
	void TakeHit(float Amount, const FVector& FromLocation);
	void GiveWeapon(int32 Slot, int32 Ammo);
	void AddAmmo(int32 Slot, int32 Amount);
	void Heal(float Amount);

	bool IsDead() const { return Health <= 0.f; }
	bool IsScoped() const;
	float GetAdsAlpha() const { return AdsAlpha; }
	float CurrentSpread() const;
	int32 GetSlot() const { return Slot; }
	const FMallWeaponDef& CurrentDef() const { return MallWeapons::Def(Slot); }
	bool IsReloading() const { return bReloading; }
	bool IsBusy() const { return SwitchTimer > 0.f || HealTimer > 0.f; }
	FVector GetEyeLocation() const;
	AMallPlayerController* GetMallPC() const;

	// Inventory
	bool bOwned[4] = {true, false, false, false};
	int32 Mag[4] = {15, 0, 0, 0};
	int32 Reserve[4] = {45, 0, 0, 0};
	int32 Meds = 1;
	bool bHasFuse = false;
	float Health = 100.f;
	float MaxHealth = 100.f;

	// Feedback for the HUD
	float HitMarkerTime = 0.f;
	bool bHitMarkerKill = false;
	bool bHitMarkerHead = false;
	float DamageFlash = 0.f;
	TArray<TPair<float, float>> DamageDirs;   // (world yaw to attacker, time left)
	float HealProgress() const { return HealTimer > 0.f ? 1.f - HealTimer / 1.6f : 0.f; }
	float ReloadProgress() const;
	UPROPERTY() TObjectPtr<AMallInteractable> Focus;
	float InteractHold = 0.f;
	int32 ShotsFired = 0;
	int32 Kills = 0;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY() TObjectPtr<UPointLightComponent> MuzzleLight;

	float LookSensitivity = 0.12f;
	bool bScriptedMove = false;       // test harness drives movement
	FVector ScriptedMoveDir = FVector::ZeroVector;

private:
	void HandleInput(float Dt);
	void UpdateMovementState(float Dt);
	void UpdateWeapon(float Dt);
	void UpdateViewmodel(float Dt);
	void UpdateFocus();
	void FireShot();
	void FinishReload();
	void EjectShell();
	bool CanFire() const;

	FMallWeaponModel Models[4];
	int32 Slot = 0;
	int32 PendingSlot = -1;
	float SwitchTimer = 0.f;
	float FireCooldown = 0.f;
	float CycleTimer = 0.f;
	bool bCycleEjected = false;
	bool bReloading = false;
	float ReloadTimer = 0.f;
	float ReloadTotal = 0.f;
	float HealTimer = 0.f;
	float AdsAlpha = 0.f;
	float SprintAlpha = 0.f;
	bool bSprinting = false;
	float SprintBlock = 0.f;
	float Bloom = 0.f;
	float RecoilPending = 0.f;
	float RecoilYawPending = 0.f;
	float RecoilRecover = 0.f;
	float Kick = 0.f;
	float FlashTimer = 0.f;
	float BobPhase = 0.f;
	float StepAccum = 0.f;
	FVector2D Sway = FVector2D::ZeroVector;
	float EyeZ = 0.f;
	float TargetEyeStand = 72.f;
	float TargetEyeCrouch = 38.f;
	bool bTriggerHeld = false;
	bool bWantsAds = false;
	bool bCrouchToggle = false;
	float LandDip = 0.f;
	bool bWasFalling = false;
	float FovCurrent = 90.f;
	float HurtSoundCooldown = 0.f;
	bool bHiddenForScope = false;
};
