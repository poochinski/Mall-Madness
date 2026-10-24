#include "MallPlayer.h"

#include "MallAudio.h"
#include "MallBuilder.h"
#include "MallFX.h"
#include "MallGameMode.h"
#include "MallInteractable.h"
#include "MallPlayerController.h"
#include "MallZombie.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AMallPlayer::AMallPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->MaxWalkSpeed = 450.f;
	CM->MaxWalkSpeedCrouched = 220.f;
	CM->MaxAcceleration = 2600.f;
	CM->BrakingDecelerationWalking = 2300.f;
	CM->GroundFriction = 9.f;
	CM->JumpZVelocity = 420.f;
	CM->GravityScale = 1.3f;
	CM->AirControl = 0.35f;
	CM->MaxStepHeight = 42.f;
	CM->SetWalkableFloorAngle(48.f);
	CM->GetNavAgentPropertiesRef().bCanCrouch = true;
	CM->SetCrouchedHalfHeight(56.f);
	CM->bCanWalkOffLedgesWhenCrouching = true;
	CM->PerchRadiusThreshold = 10.f;

	BaseEyeHeight = 72.f;
	CrouchedEyeHeight = 38.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.f);
	Camera->SetEnableFirstPersonFieldOfView(true);
	Camera->SetFirstPersonFieldOfView(68.f);
	Camera->SetEnableFirstPersonScale(true);
	Camera->SetFirstPersonScale(0.3f);

	ViewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
	ViewRoot->SetupAttachment(Camera);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Camera);
	MuzzleLight->SetRelativeLocation(FVector(70, 15, -12));
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(900.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	MuzzleLight->SetCastShadows(false);

	EyeZ = BaseEyeHeight;
}

void AMallPlayer::BeginPlay()
{
	Super::BeginPlay();
	for (int32 I = 0; I < 4; ++I)
	{
		Models[I] = MallWeapons::Build(I, this, ViewRoot);
		Models[I].Root->SetVisibility(I == Slot, true);
		if (Models[I].Flash)
		{
			Models[I].Flash->SetVisibility(false, true);
		}
		if (Models[I].Shell)
		{
			Models[I].Shell->SetVisibility(false, true);
		}
	}
	Mag[0] = MallWeapons::Def(0).MagSize;
	SwitchTimer = 0.45f;   // raise the first weapon
	PendingSlot = -1;
}

AMallPlayerController* AMallPlayer::GetMallPC() const
{
	return Cast<AMallPlayerController>(GetController());
}

FVector AMallPlayer::GetEyeLocation() const
{
	return Camera->GetComponentLocation();
}

bool AMallPlayer::IsScoped() const
{
	return CurrentDef().bScope && AdsAlpha > 0.92f && SwitchTimer <= 0.f && !bReloading;
}

float AMallPlayer::ReloadProgress() const
{
	return bReloading && ReloadTotal > 0.f ? 1.f - ReloadTimer / ReloadTotal : 0.f;
}

float AMallPlayer::CurrentSpread() const
{
	const FMallWeaponDef& D = CurrentDef();
	const float SpeedFrac = FMath::Clamp(GetVelocity().Size2D() / 450.f, 0.f, 1.5f);
	float Spread = FMath::Lerp(D.HipSpread, D.AdsSpread, AdsAlpha) + D.MoveSpread * SpeedFrac * (1.f - AdsAlpha * 0.6f) + Bloom;
	if (GetCharacterMovement()->IsFalling())
	{
		Spread += 3.f;
	}
	if (bIsCrouched)
	{
		Spread *= 0.75f;
	}
	return Spread;
}

void AMallPlayer::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// Keep the eye where it was; it then eases down instead of snapping.
	EyeZ += ScaledHalfHeightAdjust;
}

void AMallPlayer::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	EyeZ -= ScaledHalfHeightAdjust;
}

void AMallPlayer::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);

	HitMarkerTime = FMath::Max(0.f, HitMarkerTime - Dt);
	DamageFlash = FMath::Max(0.f, DamageFlash - Dt * 1.6f);
	HurtSoundCooldown -= Dt;
	for (int32 I = DamageDirs.Num() - 1; I >= 0; --I)
	{
		DamageDirs[I].Value -= Dt;
		if (DamageDirs[I].Value <= 0.f)
		{
			DamageDirs.RemoveAt(I);
		}
	}

	if (IsDead())
	{
		return;
	}

	HandleInput(Dt);
	UpdateMovementState(Dt);
	UpdateWeapon(Dt);
	UpdateViewmodel(Dt);
	UpdateFocus();
}

void AMallPlayer::HandleInput(float Dt)
{
	AMallPlayerController* PC = GetMallPC();
	const bool bInput = PC && PC->IsGameplayInputEnabled();

	FVector MoveDir = FVector::ZeroVector;
	if (bScriptedMove)
	{
		MoveDir = ScriptedMoveDir;
	}
	else if (bInput)
	{
		// Mouse look; scale down while aiming so zoomed sensitivity feels consistent.
		float DX = 0, DY = 0;
		PC->GetInputMouseDelta(DX, DY);
		const float ZoomScale = FMath::Lerp(1.f, CurrentDef().AdsFov / 90.f, AdsAlpha);
		const float Sens = LookSensitivity * 10.f * ZoomScale;
		AddControllerYawInput(DX * Sens);
		AddControllerPitchInput(-DY * Sens);
		Sway.X = FMath::Clamp(Sway.X - DX * 0.06f, -2.5f, 2.5f);
		Sway.Y = FMath::Clamp(Sway.Y - DY * 0.06f, -2.5f, 2.5f);

		const FRotator Yaw(0, GetControlRotation().Yaw, 0);
		const FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		float F = 0, R = 0;
		if (PC->IsInputKeyDown(EKeys::W)) F += 1;
		if (PC->IsInputKeyDown(EKeys::S)) F -= 1;
		if (PC->IsInputKeyDown(EKeys::D)) R += 1;
		if (PC->IsInputKeyDown(EKeys::A)) R -= 1;
		MoveDir = (Fwd * F + Right * R).GetClampedToMaxSize(1.f);

		const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
		bSprinting = bShift && F > 0.1f && !bWantsAds && !bIsCrouched && HealTimer <= 0.f && SprintBlock <= 0.f && !GetCharacterMovement()->IsFalling();
		if (bSprinting && bReloading && !CurrentDef().bShellReload)
		{
			bSprinting = false;   // magazine reloads keep the weapon up; you can't sprint through them
		}

		if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			if (bIsCrouched)
			{
				bCrouchToggle = false;
			}
			else
			{
				Jump();
			}
		}
		if (PC->WasInputKeyJustPressed(EKeys::C))
		{
			bCrouchToggle = !bCrouchToggle;
		}
		const bool bWantCrouch = bCrouchToggle || PC->IsInputKeyDown(EKeys::LeftControl);
		if (bWantCrouch && !bIsCrouched)
		{
			Crouch();
			bSprinting = false;
		}
		else if (!bWantCrouch && bIsCrouched)
		{
			UnCrouch();   // the movement component keeps us crouched if there is no headroom
		}

		bWantsAds = PC->IsInputKeyDown(EKeys::RightMouseButton);
		const bool bFireDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
		const bool bFirePressed = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
		if (bFireDown)
		{
			TryFire(bFirePressed);
		}
		bTriggerHeld = bFireDown;

		if (PC->WasInputKeyJustPressed(EKeys::R)) StartReload();
		if (PC->WasInputKeyJustPressed(EKeys::One)) SwitchTo(0);
		if (PC->WasInputKeyJustPressed(EKeys::Two)) SwitchTo(1);
		if (PC->WasInputKeyJustPressed(EKeys::Three)) SwitchTo(2);
		if (PC->WasInputKeyJustPressed(EKeys::Four)) SwitchTo(3);
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) Cycle(-1);
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) Cycle(1);
		if (PC->WasInputKeyJustPressed(EKeys::Q)) UseMedkit();

		// E: tap to pick up, hold for timed interactions (radio repair).
		if (Focus && PC->IsInputKeyDown(EKeys::E))
		{
			if (Focus->UseTime > 0.f)
			{
				InteractHold += Dt;
				if (InteractHold >= Focus->UseTime)
				{
					InteractHold = 0.f;
					Interact();
				}
			}
			else if (PC->WasInputKeyJustPressed(EKeys::E))
			{
				Interact();
			}
		}
		else
		{
			InteractHold = 0.f;
		}
	}
	else
	{
		bTriggerHeld = false;
		bWantsAds = false;
		bSprinting = false;
	}

	if (!MoveDir.IsNearlyZero())
	{
		AddMovementInput(MoveDir, 1.f);
	}
}

void AMallPlayer::UpdateMovementState(float Dt)
{
	UCharacterMovementComponent* CM = GetCharacterMovement();
	SprintBlock = FMath::Max(0.f, SprintBlock - Dt);
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bSprinting && GetVelocity().Size2D() > 300.f ? 1.f : 0.f, Dt, 9.f);

	float Speed = 450.f;
	if (bSprinting) Speed = 700.f;
	Speed = FMath::Lerp(Speed, 260.f, AdsAlpha * (CurrentDef().bScope ? 1.f : 0.8f));
	if (HealTimer > 0.f) Speed = FMath::Min(Speed, 250.f);
	CM->MaxWalkSpeed = Speed;
	CM->MaxWalkSpeedCrouched = FMath::Lerp(220.f, 160.f, AdsAlpha);

	// Eye height: ease toward standing/crouched target, plus a small landing dip.
	const bool bFalling = CM->IsFalling();
	if (bWasFalling && !bFalling)
	{
		LandDip = FMath::Clamp(-GetVelocity().Z * 0.0f + 4.f, 0.f, 6.f);
	}
	bWasFalling = bFalling;
	LandDip = FMath::FInterpTo(LandDip, 0.f, Dt, 8.f);
	const float Target = bIsCrouched ? CrouchedEyeHeight : BaseEyeHeight;
	EyeZ = FMath::FInterpTo(EyeZ, Target, Dt, 12.f);

	// Footsteps + restrained bob.
	const float HSpeed = GetVelocity().Size2D();
	if (!bFalling && HSpeed > 40.f)
	{
		BobPhase += HSpeed * Dt * 0.018f;
		StepAccum += HSpeed * Dt;
		const float StepLen = bSprinting ? 175.f : (bIsCrouched ? 105.f : 145.f);
		if (StepAccum > StepLen)
		{
			StepAccum = 0.f;
			const float Vol = bSprinting ? 0.55f : (bIsCrouched ? 0.12f : 0.32f);
			UMallAudio::Get()->Play2D(GetWorld(), "step", Vol, FMath::FRandRange(0.85f, 1.1f));
			if (bSprinting)
			{
				if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
				{
					GM->ReportNoise(GetActorLocation(), 700.f);
				}
			}
		}
	}
	const float Bob = FMath::Sin(BobPhase * 2.f) * (bSprinting ? 0.9f : 0.35f) * FMath::Clamp(HSpeed / 450.f, 0.f, 1.f) * (1.f - AdsAlpha);
	Camera->SetRelativeLocation(FVector(0, 0, EyeZ - LandDip + Bob));
}

bool AMallPlayer::CanFire() const
{
	return !IsDead() && SwitchTimer <= 0.f && HealTimer <= 0.f && FireCooldown <= 0.f && CycleTimer <= 0.f && SprintAlpha < 0.35f;
}

void AMallPlayer::TryFire(bool bJustPressed)
{
	const FMallWeaponDef& D = CurrentDef();
	if (bSprinting || SprintAlpha > 0.35f)
	{
		// Firing interrupts sprint; the shot happens once the weapon is back up.
		bSprinting = false;
		SprintBlock = 0.35f;
		return;
	}
	if (!D.bAutomatic && !bJustPressed)
	{
		return;
	}
	if (bReloading)
	{
		// Shell-by-shell reloads can be interrupted by the trigger when a shell is chambered.
		if (D.bShellReload && Mag[Slot] > 0 && bJustPressed)
		{
			bReloading = false;
			if (Models[Slot].Shell) Models[Slot].Shell->SetVisibility(false, true);
		}
		else
		{
			return;
		}
	}
	if (!CanFire())
	{
		return;
	}
	if (Mag[Slot] <= 0)
	{
		if (bJustPressed)
		{
			UMallAudio::Get()->Play2D(GetWorld(), "dry", 0.7f);
			FireCooldown = 0.25f;
			if (Reserve[Slot] > 0)
			{
				StartReload();
			}
			else if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
			{
				GM->Notify(FString::Printf(TEXT("Out of %s ammo"), *D.AmmoName), FColor(230, 80, 60));
			}
		}
		return;
	}
	FireShot();
}

void AMallPlayer::FireShot()
{
	const FMallWeaponDef& D = CurrentDef();
	UWorld* W = GetWorld();
	AMallGameMode* GM = W->GetAuthGameMode<AMallGameMode>();
	--Mag[Slot];
	++ShotsFired;
	FireCooldown = D.FireInterval;
	if (D.CycleTime > 0.f)
	{
		CycleTimer = D.CycleTime;
		bCycleEjected = false;
	}

	const FVector CamLoc = Camera->GetComponentLocation();
	const FRotator CamRot = Camera->GetComponentRotation();
	const FVector Fwd = CamRot.Vector();
	const float Spread = CurrentSpread();

	// Approximate world-space muzzle so a wall right in front of the barrel stops the shot.
	const FVector MuzzleWorld = CamLoc + CamRot.RotateVector(FVector(FMath::Min(65.f, Models[Slot].Muzzle.X * 0.9f + 15.f), 9.f, -9.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallShot), true, this);
	Params.bReturnPhysicalMaterial = false;

	bool bAnyHit = false, bAnyKill = false, bAnyHead = false;
	TMap<AMallZombie*, float> PendingDamage;
	TMap<AMallZombie*, TPair<bool, FHitResult>> PendingInfo;

	for (int32 P = 0; P < D.Pellets; ++P)
	{
		const FVector Dir = Spread > 0.01f ? FMath::VRandCone(Fwd, FMath::DegreesToRadians(Spread * (D.Pellets > 1 ? FMath::Sqrt(FMath::FRand()) : FMath::FRand()))) : Fwd;
		const FVector End = CamLoc + Dir * D.Range * 100.f;
		FHitResult Hit;
		bool bHit = W->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params);
		const FVector Target = bHit ? Hit.ImpactPoint : End;

		// Muzzle obstruction checks.
		FHitResult Block;
		if (W->LineTraceSingleByChannel(Block, CamLoc, MuzzleWorld, ECC_Visibility, Params))
		{
			Hit = Block;
			bHit = true;
		}
		else if (W->LineTraceSingleByChannel(Block, MuzzleWorld, Target + Dir * 5.f, ECC_Visibility, Params) &&
			FVector::Dist(Block.ImpactPoint, Target) > 12.f)
		{
			Hit = Block;
			bHit = true;
		}

		if (!bHit)
		{
			continue;
		}
		const float DistM = FVector::Dist(CamLoc, Hit.ImpactPoint) / 100.f;
		const float Falloff = FMath::GetMappedRangeValueClamped(FVector2D(D.FalloffStart, D.FalloffEnd), FVector2D(1.f, D.MinDamageFrac), DistM);

		if (AMallZombie* Z = Cast<AMallZombie>(Hit.GetActor()))
		{
			const bool bHead = Hit.BoneName.ToString().Contains(TEXT("head")) || Hit.BoneName.ToString().Contains(TEXT("neck"));
			float Dmg = D.Damage * Falloff * (bHead ? D.HeadMult : 1.f);
			PendingDamage.FindOrAdd(Z) += Dmg;
			TPair<bool, FHitResult>& Info = PendingInfo.FindOrAdd(Z);
			Info.Key = Info.Key || bHead;
			Info.Value = Hit;
			if (AMallFX* FX = AMallFX::Get(W))
			{
				FX->Impact(Hit.ImpactPoint, -Dir, 1);
			}
		}
		else if (AMallFX* FX = AMallFX::Get(W))
		{
			FX->Impact(Hit.ImpactPoint, Hit.ImpactNormal, 0);
			if (P == 0 || FMath::FRand() < 0.3f)
			{
				UMallAudio::Get()->PlayAt(W, "impact", Hit.ImpactPoint, 0.5f, FMath::FRandRange(0.8f, 1.2f), 1500.f);
			}
		}
	}

	for (const TPair<AMallZombie*, float>& PD : PendingDamage)
	{
		const TPair<bool, FHitResult>& Info = PendingInfo[PD.Key];
		const bool bKilled = PD.Key->ApplyHit(PD.Value, Info.Key, Fwd, this);
		bAnyHit = true;
		bAnyKill |= bKilled;
		bAnyHead |= Info.Key;
		if (bKilled)
		{
			++Kills;
		}
	}
	if (bAnyHit)
	{
		HitMarkerTime = 0.18f;
		bHitMarkerKill = bAnyKill;
		bHitMarkerHead = bAnyHead;
		UMallAudio::Get()->Play2D(W, bAnyHead ? "headshot" : "hitmark", bAnyHead ? 0.55f : 0.4f);
	}

	// Feedback: recoil, bloom, flash, sound, noise.
	const float RecoilScale = (bIsCrouched ? 0.8f : 1.f) * FMath::Lerp(1.f, 0.7f, AdsAlpha);
	RecoilPending += D.RecoilPitch * RecoilScale;
	RecoilYawPending += FMath::FRandRange(-D.RecoilYaw, D.RecoilYaw) * RecoilScale;
	Bloom = FMath::Min(Bloom + D.HipSpread * 0.18f, 4.f);
	Kick = 1.f;
	FlashTimer = 0.05f;
	if (Models[Slot].Flash && !IsScoped())
	{
		Models[Slot].Flash->SetVisibility(true, true);
		Models[Slot].Flash->SetRelativeRotation(FRotator(0, 0, FMath::FRandRange(0.f, 360.f)));
		Models[Slot].Flash->SetRelativeScale3D(FVector(FMath::FRandRange(0.8f, 1.3f)));
	}
	MuzzleLight->SetIntensity(D.Pellets > 1 ? 9000.f : 5000.f);
	UMallAudio::Get()->Play2D(W, D.Sound, 0.85f, FMath::FRandRange(0.95f, 1.05f));
	if (GM)
	{
		GM->ReportNoise(GetActorLocation(), D.NoiseRadius * 100.f);
	}
	if (Slot == 0 || Slot == 1)
	{
		EjectShell();
	}
	if (Mag[Slot] == 0 && Reserve[Slot] > 0 && Slot != 2)
	{
		// Auto-reload shortly after running dry, like most modern shooters.
		FireCooldown = FMath::Max(FireCooldown, 0.25f);
	}
}

void AMallPlayer::EjectShell()
{
	if (AMallFX* FX = AMallFX::Get(GetWorld()))
	{
		const FRotator CamRot = Camera->GetComponentRotation();
		const FVector Port = Camera->GetComponentLocation() + CamRot.RotateVector(FVector(30.f, 10.f, -8.f));
		const FVector Vel = CamRot.RotateVector(FVector(FMath::FRandRange(-40.f, 20.f), FMath::FRandRange(180.f, 260.f), FMath::FRandRange(120.f, 200.f))) + GetVelocity();
		FX->Shell(Port, Vel, Slot == 2 ? 1 : 0);
	}
}

void AMallPlayer::StartReload()
{
	const FMallWeaponDef& D = CurrentDef();
	if (bReloading || IsDead() || SwitchTimer > 0.f || HealTimer > 0.f || Mag[Slot] >= D.MagSize || Reserve[Slot] <= 0)
	{
		return;
	}
	bReloading = true;
	bSprinting = false;
	ReloadTotal = D.bShellReload ? D.ShellTime + 0.25f : D.ReloadTime;
	ReloadTimer = ReloadTotal;
	UMallAudio::Get()->Play2D(GetWorld(), D.bShellReload ? "shell_in" : "mag_out", 0.6f);
	if (D.bShellReload && Models[Slot].Shell)
	{
		Models[Slot].Shell->SetVisibility(true, true);
	}
}

void AMallPlayer::FinishReload()
{
	const FMallWeaponDef& D = CurrentDef();
	if (D.bShellReload)
	{
		// One shell per cycle; ammo only ever moves from reserve to the tube.
		if (Reserve[Slot] > 0 && Mag[Slot] < D.MagSize)
		{
			--Reserve[Slot];
			++Mag[Slot];
			UMallAudio::Get()->Play2D(GetWorld(), "shell_in", 0.6f, FMath::FRandRange(0.95f, 1.05f));
		}
		if (Reserve[Slot] > 0 && Mag[Slot] < D.MagSize)
		{
			ReloadTotal = D.ShellTime;
			ReloadTimer = D.ShellTime;
			return;
		}
		bReloading = false;
		if (Models[Slot].Shell) Models[Slot].Shell->SetVisibility(false, true);
		UMallAudio::Get()->Play2D(GetWorld(), "pump", 0.6f);
		CycleTimer = 0.45f;
		bCycleEjected = true;
		return;
	}
	const int32 Need = D.MagSize - Mag[Slot];
	const int32 Take = FMath::Min(Need, Reserve[Slot]);
	Reserve[Slot] -= Take;
	Mag[Slot] += Take;
	bReloading = false;
	UMallAudio::Get()->Play2D(GetWorld(), Slot == 3 ? "bolt" : "rack", 0.6f);
}

void AMallPlayer::SwitchTo(int32 NewSlot)
{
	if (NewSlot < 0 || NewSlot > 3 || !bOwned[NewSlot] || IsDead() || HealTimer > 0.f)
	{
		return;
	}
	if (NewSlot == Slot && PendingSlot < 0)
	{
		return;
	}
	// Rule: switching cancels an in-progress reload. No ammo moves until a reload completes,
	// so a cancelled magazine reload loses nothing and shotgun shells already loaded stay loaded.
	if (bReloading)
	{
		bReloading = false;
		if (Models[Slot].Shell) Models[Slot].Shell->SetVisibility(false, true);
	}
	PendingSlot = NewSlot;
	SwitchTimer = 0.55f;
	CycleTimer = 0.f;
	UMallAudio::Get()->Play2D(GetWorld(), "mag_out", 0.25f, 1.3f);
}

void AMallPlayer::Cycle(int32 Dir)
{
	int32 S = PendingSlot >= 0 ? PendingSlot : Slot;
	for (int32 I = 0; I < 4; ++I)
	{
		S = (S + Dir + 4) % 4;
		if (bOwned[S])
		{
			SwitchTo(S);
			return;
		}
	}
}

void AMallPlayer::UseMedkit()
{
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	if (HealTimer > 0.f || IsDead())
	{
		return;
	}
	if (Meds <= 0)
	{
		if (GM) GM->Notify(TEXT("No medical supplies"), FColor(230, 80, 60));
		return;
	}
	if (Health >= MaxHealth)
	{
		if (GM) GM->Notify(TEXT("Already at full health"), FColor(200, 200, 200));
		return;
	}
	bReloading = false;
	HealTimer = 1.6f;
	bSprinting = false;
	UMallAudio::Get()->Play2D(GetWorld(), "heal", 0.7f);
}

void AMallPlayer::Heal(float Amount)
{
	Health = FMath::Min(MaxHealth, Health + Amount);
}

void AMallPlayer::GiveWeapon(int32 NewSlot, int32 Ammo)
{
	const bool bNew = !bOwned[NewSlot];
	bOwned[NewSlot] = true;
	if (bNew)
	{
		Mag[NewSlot] = FMath::Min(Ammo, MallWeapons::Def(NewSlot).MagSize);
		Reserve[NewSlot] += Ammo - Mag[NewSlot];
		SwitchTo(NewSlot);
	}
	else
	{
		Reserve[NewSlot] += Ammo;
	}
}

void AMallPlayer::AddAmmo(int32 AmmoSlot, int32 Amount)
{
	Reserve[AmmoSlot] += Amount;
}

void AMallPlayer::TakeHit(float Amount, const FVector& FromLocation)
{
	if (IsDead())
	{
		return;
	}
	Health = FMath::Max(0.f, Health - Amount);
	DamageFlash = 1.f;
	const FVector To = FromLocation - GetActorLocation();
	DamageDirs.Add({FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), 1.4f});
	if (DamageDirs.Num() > 6)
	{
		DamageDirs.RemoveAt(0);
	}
	AddControllerPitchInput(-FMath::FRandRange(0.8f, 1.6f));
	AddControllerYawInput(FMath::FRandRange(-1.2f, 1.2f));
	if (HurtSoundCooldown <= 0.f)
	{
		UMallAudio::Get()->Play2D(GetWorld(), "hurt", 0.7f, FMath::FRandRange(0.9f, 1.1f));
		HurtSoundCooldown = 0.4f;
	}
	if (Health <= 0.f)
	{
		bReloading = false;
		HealTimer = 0.f;
		if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
		{
			GM->OnPlayerDied();
		}
	}
}

bool AMallPlayer::Interact()
{
	if (!Focus || Focus->bUsed || !Focus->bEnabled)
	{
		return false;
	}
	if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
	{
		return GM->HandleInteract(this, Focus);
	}
	return false;
}

void AMallPlayer::UpdateFocus()
{
	// Pick the interactable nearest the crosshair within reach, then confirm line of sight.
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	AMallInteractable* Best = nullptr;
	float BestScore = 0.f;
	const FVector Eye = Camera->GetComponentLocation();
	const FVector Fwd = Camera->GetForwardVector();
	if (GM && GM->Builder)
	{
		for (AMallInteractable* I : GM->Builder->Interactables)
		{
			if (!I || I->bUsed || !I->bEnabled)
			{
				continue;
			}
			const FVector To = I->FocusPoint() - Eye;
			const float Dist = To.Size();
			const float Reach = I->Item == EMallItem::Exit ? 400.f : 230.f;
			if (Dist > Reach || Dist < 1.f)
			{
				continue;
			}
			const float Dot = FVector::DotProduct(To / Dist, Fwd);
			const float MinDot = Dist < 80.f ? 0.75f : 0.93f;
			if (Dot < MinDot)
			{
				continue;
			}
			const float Score = Dot - Dist / 5000.f;
			if (Score > BestScore)
			{
				FHitResult Hit;
				FCollisionQueryParams P(SCENE_QUERY_STAT(MallFocus), false, this);
				P.AddIgnoredActor(I);
				if (!GetWorld()->LineTraceSingleByObjectType(Hit, Eye, I->FocusPoint(), FCollisionObjectQueryParams(ECC_WorldStatic), P))
				{
					Best = I;
					BestScore = Score;
				}
			}
		}
	}
	if (Best != Focus)
	{
		InteractHold = 0.f;
	}
	Focus = Best;
}

void AMallPlayer::UpdateWeapon(float Dt)
{
	const FMallWeaponDef& D = CurrentDef();
	FireCooldown = FMath::Max(0.f, FireCooldown - Dt);
	Bloom = FMath::FInterpTo(Bloom, 0.f, Dt, 5.f);
	Kick = FMath::FInterpTo(Kick, 0.f, Dt, 14.f);

	// Auto reload after emptying the magazine.
	if (Mag[Slot] == 0 && Reserve[Slot] > 0 && !bReloading && FireCooldown <= 0.f && SwitchTimer <= 0.f && HealTimer <= 0.f && CycleTimer <= 0.f)
	{
		StartReload();
	}

	// Recoil: apply the kick over a few frames, then recover part of it.
	if (RecoilPending > 0.01f || FMath::Abs(RecoilYawPending) > 0.01f)
	{
		const float Step = FMath::Min(1.f, Dt * 28.f);
		const float P = RecoilPending * Step, Y = RecoilYawPending * Step;
		AddControllerPitchInput(-P);
		AddControllerYawInput(Y);
		RecoilPending -= P;
		RecoilYawPending -= Y;
		RecoilRecover += P * 0.55f;
	}
	else if (RecoilRecover > 0.f && !(bTriggerHeld && D.bAutomatic))
	{
		const float R = FMath::Min(RecoilRecover, Dt * 9.f);
		AddControllerPitchInput(R);
		RecoilRecover -= R;
	}

	if (FlashTimer > 0.f)
	{
		FlashTimer -= Dt;
		if (FlashTimer <= 0.f)
		{
			if (Models[Slot].Flash) Models[Slot].Flash->SetVisibility(false, true);
			MuzzleLight->SetIntensity(0.f);
		}
	}

	if (CycleTimer > 0.f)
	{
		const float Before = CycleTimer;
		CycleTimer = FMath::Max(0.f, CycleTimer - Dt);
		if (!bCycleEjected && D.CycleTime > 0.f && Before > D.CycleTime * 0.55f && CycleTimer <= D.CycleTime * 0.55f)
		{
			bCycleEjected = true;
			UMallAudio::Get()->Play2D(GetWorld(), D.CycleSound, 0.65f);
			EjectShell();
		}
	}

	if (bReloading)
	{
		ReloadTimer -= Dt;
		if (!D.bShellReload && ReloadTimer < ReloadTotal * 0.45f && ReloadTimer + Dt >= ReloadTotal * 0.45f)
		{
			UMallAudio::Get()->Play2D(GetWorld(), "mag_in", 0.65f);
		}
		if (ReloadTimer <= 0.f)
		{
			FinishReload();
		}
	}

	if (SwitchTimer > 0.f)
	{
		const float Before = SwitchTimer;
		SwitchTimer = FMath::Max(0.f, SwitchTimer - Dt);
		// Swap models at the bottom of the lower/raise motion.
		if (PendingSlot >= 0 && Before > 0.3f && SwitchTimer <= 0.3f)
		{
			Models[Slot].Root->SetVisibility(false, true);
			Slot = PendingSlot;
			PendingSlot = -1;
			Models[Slot].Root->SetVisibility(true, true);
			if (Models[Slot].Flash) Models[Slot].Flash->SetVisibility(false, true);
			if (Models[Slot].Shell) Models[Slot].Shell->SetVisibility(false, true);
			UMallAudio::Get()->Play2D(GetWorld(), "rack", 0.35f, 1.15f);
		}
	}

	if (HealTimer > 0.f)
	{
		HealTimer -= Dt;
		if (HealTimer <= 0.f)
		{
			HealTimer = 0.f;
			--Meds;
			Heal(60.f);
			if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
			{
				GM->Notify(TEXT("+60 health"), FColor(120, 230, 140));
			}
		}
	}

	// Aim-down-sights.
	const bool bCanAds = bWantsAds && SwitchTimer <= 0.f && HealTimer <= 0.f && !(bReloading && !D.bShellReload);
	AdsAlpha = FMath::FInterpConstantTo(AdsAlpha, bCanAds ? 1.f : 0.f, Dt, 1.f / D.AdsTime);
	const float Ease = AdsAlpha * AdsAlpha * (3.f - 2.f * AdsAlpha);
	const float TargetFov = FMath::Lerp(90.f + SprintAlpha * 4.f, D.AdsFov, Ease);
	FovCurrent = FMath::FInterpTo(FovCurrent, TargetFov, Dt, 20.f);
	Camera->SetFieldOfView(FovCurrent);
}

void AMallPlayer::UpdateViewmodel(float Dt)
{
	FMallWeaponModel& M = Models[Slot];
	if (!M.Root)
	{
		return;
	}
	const FMallWeaponDef& D = CurrentDef();
	const float Ease = AdsAlpha * AdsAlpha * (3.f - 2.f * AdsAlpha);
	const float HSpeed = FMath::Clamp(GetVelocity().Size2D() / 450.f, 0.f, 1.6f);

	FVector Pos = FMath::Lerp(M.HipOffset, FVector(M.AdsDist, 0, 0), Ease);
	// Slight inward cant at the hip so the side of the weapon reads.
	FRotator Rot(0.f, -3.f * (1.f - Ease), -3.f * (1.f - Ease));

	// Bob and sway (much smaller while aiming).
	const float Calm = 1.f - Ease * 0.9f;
	Pos.Y += FMath::Sin(BobPhase) * 0.6f * HSpeed * Calm;
	Pos.Z += FMath::Abs(FMath::Cos(BobPhase)) * -0.7f * HSpeed * Calm;
	Sway = FMath::Vector2DInterpTo(Sway, FVector2D::ZeroVector, Dt, 7.f);
	Pos.Y += Sway.X * 0.6f * Calm;
	Pos.Z += Sway.Y * 0.5f * Calm;
	Rot.Yaw += Sway.X * 1.2f * Calm;
	Rot.Pitch += Sway.Y * 1.0f * Calm;
	Rot.Roll += Sway.X * 1.5f * Calm;

	// Sprint pose.
	Pos += FVector(-4.f, -3.f, -5.f) * SprintAlpha;
	Rot += FRotator(-14.f, 26.f, -12.f) * SprintAlpha;

	// Recoil kick.
	Pos.X -= Kick * (D.Pellets > 1 || D.bScope ? 5.f : 2.5f);
	Rot.Pitch += Kick * (D.Pellets > 1 ? 6.f : 3.f) * (1.f - Ease * 0.6f);

	// Reload pose and magazine motion.
	float ReloadPose = 0.f;
	if (bReloading)
	{
		const float T = ReloadProgress();
		ReloadPose = D.bShellReload ? 1.f : FMath::Clamp(FMath::Min(T / 0.15f, (1.f - T) / 0.15f), 0.f, 1.f);
		if (M.Mag)
		{
			const float Drop = T < 0.2f ? 0.f : (T < 0.45f ? (T - 0.2f) / 0.25f : (T < 0.6f ? 1.f : FMath::Max(0.f, 1.f - (T - 0.6f) / 0.2f)));
			M.Mag->SetRelativeLocation(FVector(-2.f, 0, -26.f) * Drop);
		}
		if (M.Shell)
		{
			M.Shell->SetRelativeLocation(FVector(4.f, 0, -9.f) + FVector(5.f * T, 0, 4.f * T));
		}
	}
	else if (M.Mag)
	{
		M.Mag->SetRelativeLocation(FVector::ZeroVector);
	}
	Rot.Roll += 28.f * ReloadPose;
	Rot.Pitch += (D.bShellReload ? 8.f : -12.f) * ReloadPose;
	Pos.Z -= 3.f * ReloadPose;

	// Slide / pump / bolt.
	if (M.Moving)
	{
		float Travel = 0.f;
		if (Slot == 0 || Slot == 1)
		{
			Travel = Kick;
			if (Mag[Slot] == 0 && Slot == 0)
			{
				Travel = 1.f;   // slide locks back on empty
			}
		}
		else if (CycleTimer > 0.f && D.CycleTime > 0.f)
		{
			const float T = 1.f - CycleTimer / FMath::Max(D.CycleTime, 0.45f);
			Travel = FMath::Sin(FMath::Clamp((T - 0.2f) / 0.8f, 0.f, 1.f) * PI);
		}
		const FVector Base = Slot == 3 ? FVector(-4.f, 0, -5.4f) : FVector::ZeroVector;
		M.Moving->SetRelativeLocation(Base + M.MovingAxis * M.MovingTravel * Travel);
		if (Slot == 3)
		{
			M.Moving->SetRelativeRotation(FRotator(0, 0, -70.f * FMath::Clamp(Travel * 2.f, 0.f, 1.f)));
		}
	}

	// Weapon switch / healing: lower out of view.
	float Lower = 0.f;
	if (SwitchTimer > 0.f)
	{
		Lower = SwitchTimer > 0.3f ? 1.f - (SwitchTimer - 0.3f) / 0.25f : SwitchTimer / 0.3f;
		Lower = FMath::Clamp(Lower, 0.f, 1.f);
	}
	if (HealTimer > 0.f)
	{
		Lower = FMath::Max(Lower, FMath::Clamp(FMath::Min((1.6f - HealTimer) / 0.25f, HealTimer / 0.25f), 0.f, 1.f));
	}
	Pos.Z -= 24.f * Lower;
	Rot.Pitch -= 35.f * Lower;

	M.Root->SetRelativeLocationAndRotation(Pos, Rot);
	const bool bScoped = IsScoped();
	if (bScoped != bHiddenForScope)
	{
		bHiddenForScope = bScoped;
		M.Root->SetVisibility(!bScoped, true);
		if (M.Flash) M.Flash->SetVisibility(false, true);
		if (M.Shell) M.Shell->SetVisibility(bReloading && D.bShellReload && !bScoped, true);
	}
}
