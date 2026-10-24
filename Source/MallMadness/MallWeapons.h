#pragma once

#include "CoreMinimal.h"

class USceneComponent;
class UStaticMeshComponent;
class AActor;

enum class EMallWeapon : uint8 { Handgun = 0, AR15 = 1, Shotgun = 2, Sniper = 3, Count = 4 };

/** Tuning for one weapon. Distances in metres, angles in degrees, times in seconds. */
struct FMallWeaponDef
{
	FString Name;
	int32 MagSize = 15;
	float Damage = 30.f;
	int32 Pellets = 1;
	float FireInterval = 0.15f;
	bool bAutomatic = false;
	float HipSpread = 1.5f;
	float AdsSpread = 0.3f;
	float MoveSpread = 1.5f;     // added at full run speed
	float RecoilPitch = 1.2f;
	float RecoilYaw = 0.3f;
	float ReloadTime = 1.6f;
	bool bShellReload = false;   // shotgun loads one shell at a time
	float ShellTime = 0.5f;
	float AdsFov = 70.f;
	float AdsTime = 0.18f;
	float Range = 80.f;
	float FalloffStart = 15.f;
	float FalloffEnd = 40.f;
	float MinDamageFrac = 0.5f;
	float HeadMult = 2.f;
	float NoiseRadius = 30.f;
	float CycleTime = 0.f;       // pump / bolt animation after each shot
	bool bScope = false;
	FName Sound;
	FName CycleSound;
	FString AmmoName;
};

/** Built first-person model and the animated sub-parts the player drives. */
struct FMallWeaponModel
{
	USceneComponent* Root = nullptr;
	USceneComponent* Mag = nullptr;      // drops during reload
	USceneComponent* Moving = nullptr;   // slide / pump / bolt
	USceneComponent* Flash = nullptr;
	USceneComponent* Shell = nullptr;    // shotgun shell being loaded
	FVector Muzzle = FVector::ZeroVector;
	FVector HipOffset = FVector(30, 12, -12);
	float AdsDist = 20.f;
	FVector MovingAxis = FVector(-1, 0, 0);
	float MovingTravel = 2.f;
};

namespace MallWeapons
{
	const FMallWeaponDef& Def(int32 Slot);
	/** Builds the multi-part first-person model for a slot under Parent. */
	FMallWeaponModel Build(int32 Slot, AActor* Owner, USceneComponent* Parent);
}
