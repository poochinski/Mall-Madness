#include "MallGameMode.h"

#include "MallAudio.h"
#include "MallBuilder.h"
#include "MallFX.h"
#include "MallHUD.h"
#include "MallInteractable.h"
#include "MallMaterials.h"
#include "MallPlayer.h"
#include "MallPlayerController.h"
#include "MallZombie.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

float AMallGameMode::Sensitivity = 0.12f;
float AMallGameMode::Volume = 0.8f;

AMallGameMode::AMallGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	DefaultPawnClass = AMallPlayer::StaticClass();
	PlayerControllerClass = AMallPlayerController::StaticClass();
	HUDClass = AMallHUD::StaticClass();
}

void AMallGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bRestarted = UGameplayStatics::HasOption(Options, TEXT("restart"));
	bTestMode = FParse::Param(FCommandLine::Get(), TEXT("MallTest"));
	if (UGameplayStatics::HasOption(Options, TEXT("mtest")))
	{
		TestPhase = FCString::Atoi(*UGameplayStatics::ParseOption(Options, TEXT("mtest")));
	}
}

AMallPlayer* AMallGameMode::GetPlayer() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? Cast<AMallPlayer>(PC->GetPawn()) : nullptr;
}

void AMallGameMode::SpawnEnvironment()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;

	// Afternoon sun angled so shafts fall through the atrium skylight onto the concourse.
	ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 3000), FRotator(-48, 28, 0), P);
	if (UDirectionalLightComponent* SunC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		SunC->SetMobility(EComponentMobility::Movable);
		SunC->SetIntensity(6.5f);
		SunC->SetLightColor(FLinearColor(1.0f, 0.92f, 0.82f));
		SunC->SetAtmosphereSunLight(true);
		SunC->SetCastShadows(true);
		SunC->SetLightSourceAngle(0.8f);
	}

	AActor* SkyActor = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	USkyAtmosphereComponent* Atmos = NewObject<USkyAtmosphereComponent>(SkyActor);
	SkyActor->SetRootComponent(Atmos);
	Atmos->RegisterComponent();

	ASkyLight* Sky = W->SpawnActor<ASkyLight>(FVector(0, 0, 2000), FRotator::ZeroRotator, P);
	if (USkyLightComponent* SkyC = Sky->GetLightComponent())
	{
		SkyC->SetMobility(EComponentMobility::Movable);
		SkyC->bRealTimeCapture = true;
		SkyC->SetIntensity(1.0f);
		SkyC->SetLightColor(FLinearColor(0.80f, 0.88f, 1.0f));
		SkyC->RecaptureSky();
	}

	AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -200), FRotator::ZeroRotator, P);
	if (UExponentialHeightFogComponent* FogC = Fog->GetComponent())
	{
		FogC->SetFogDensity(0.012f);
		FogC->SetFogHeightFalloff(0.12f);
		FogC->SetFogInscatteringColor(FLinearColor(0.16f, 0.18f, 0.21f));
		FogC->SetVolumetricFog(true);
		FogC->SetVolumetricFogScatteringDistribution(0.6f);
		FogC->SetVolumetricFogExtinctionScale(0.6f);
	}

	APostProcessVolume* PPV = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	PPV->bUnbound = true;
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = -1.0f;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = 4.0f;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = -0.4f;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 2.0f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 1.2f;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.3f;
	S.bOverride_BloomThreshold = true;
	S.BloomThreshold = 1.2f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.4f;
	S.bOverride_AmbientOcclusionIntensity = true;
	S.AmbientOcclusionIntensity = 0.65f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.1f;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.9f, 0.9f, 0.9f, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.f);
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.15f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.12f;
	S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureHighlightContrastScale = 0.75f;
	S.bOverride_LocalExposureShadowContrastScale = true;
	S.LocalExposureShadowContrastScale = 0.8f;
}

void AMallGameMode::StartPlay()
{
	UMallMaterials::Get();
	UMallAudio::Get()->MasterVolume = Volume;
	SpawnEnvironment();

	Builder = GetWorld()->SpawnActor<AMallBuilder>(AMallBuilder::StaticClass(), FTransform::Identity);
	Builder->Build();
	GetWorld()->SpawnActor<AMallFX>(AMallFX::StaticClass(), FTransform::Identity);

	Super::StartPlay();

	if (AMallPlayer* P = GetPlayer())
	{
		P->SetActorLocation(Builder->PlayerStart + FVector(0, 0, 92.f), false, nullptr, ETeleportType::TeleportPhysics);
		P->LookSensitivity = Sensitivity;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->SetControlRotation(Builder->PlayerStartRot);
		}
	}

	// Opening population: mostly walkers, away from the player start.
	FRandomStream R(1337);
	int32 Spawned = 0;
	for (const FVector& L : Builder->StartZombies)
	{
		if (FVector::Dist(L, Builder->PlayerStart) < 1400.f)
		{
			continue;
		}
		const EZombieKind K = (Spawned % 6 == 5) ? EZombieKind::Runner : EZombieKind::Walker;
		SpawnZombie(L, K);
		++Spawned;
	}
	SetEmergencyMode(false);
	UE_LOG(LogTemp, Display, TEXT("MALL: built. nav nodes=%d links=%d interactables=%d zombies=%d"),
		Builder->Nav.Nodes.Num(), Builder->Nav.NumLinks(), Builder->Interactables.Num(), Zombies.Num());

	// -MallShots="x,y,z,pitch,yaw;..." (metres/degrees) captures screenshots then quits.
	FString ShotStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("MallShots="), ShotStr, false))
	{
		TArray<FString> Entries;
		ShotStr.ParseIntoArray(Entries, TEXT(";"));
		for (const FString& E : Entries)
		{
			TArray<FString> V;
			E.ParseIntoArray(V, TEXT(","));
			if (V.Num() >= 5)
			{
				Shots.Add(FTransform(FRotator(FCString::Atof(*V[3]), FCString::Atof(*V[4]), 0), M(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]))));
			}
		}
		bShotsActive = Shots.Num() > 0;
		ShotTimer = 5.f;
		for (AMallZombie* Z : Zombies)
		{
			if (Z) Z->SetActorTickEnabled(false);
		}
	}

	if (bRestarted || bShotsActive || bTestMode)
	{
		StartGame();
	}
	else
	{
		State = EMallState::Title;
		if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			PC->SetMenuMode(true);
		}
	}
}

void AMallGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	// Drop references into this world so level reloads (restart) can collect it.
	UMallAudio::Get()->StopAll();
	Super::EndPlay(Reason);
}

void AMallGameMode::StartGame()
{
	State = EMallState::Playing;
	UGameplayStatics::SetGamePaused(this, false);
	if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->SetMenuMode(false);
	}
	UMallAudio::Get()->StartAmbience(GetWorld());
	if (!bRestarted)
	{
		Notify(TEXT("Find the fuse. Restore the radio. Get out."), FColor(235, 225, 200), 5.f);
	}
}

void AMallGameMode::SetPaused(bool bPause)
{
	if (bPause && State == EMallState::Playing)
	{
		State = EMallState::Paused;
		UGameplayStatics::SetGamePaused(this, true);
		if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			PC->SetMenuMode(true);
		}
	}
	else if (!bPause && State == EMallState::Paused)
	{
		State = EMallState::Playing;
		UGameplayStatics::SetGamePaused(this, false);
		if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			PC->SetMenuMode(false);
		}
	}
}

void AMallGameMode::Restart()
{
	UMallAudio::Get()->StopAll();
	UGameplayStatics::SetGamePaused(this, false);
	FString Opt = TEXT("restart");
	if (bTestMode)
	{
		Opt += TEXT("?mtest=2");
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, Opt);
}

void AMallGameMode::OnButton(EMallButton Button)
{
	UMallAudio::Get()->Play2D(GetWorld(), "hitmark", 0.5f, 0.7f);
	switch (Button)
	{
	case EMallButton::Play: StartGame(); break;
	case EMallButton::Resume: SetPaused(false); break;
	case EMallButton::Restart: Restart(); break;
	case EMallButton::Quit: FPlatformMisc::RequestExit(false); break;
	case EMallButton::SensDown: Sensitivity = FMath::Max(0.02f, Sensitivity - 0.02f); break;
	case EMallButton::SensUp: Sensitivity = FMath::Min(0.5f, Sensitivity + 0.02f); break;
	case EMallButton::VolDown: Volume = FMath::Max(0.f, Volume - 0.1f); break;
	case EMallButton::VolUp: Volume = FMath::Min(1.f, Volume + 0.1f); break;
	default: break;
	}
	UMallAudio::Get()->MasterVolume = Volume;
	if (AMallPlayer* P = GetPlayer())
	{
		P->LookSensitivity = Sensitivity;
	}
}

void AMallGameMode::OnPlayerDied()
{
	if (State != EMallState::Playing)
	{
		return;
	}
	State = EMallState::Dead;
	UMallAudio::Get()->Play2D(GetWorld(), "thump", 0.8f, 0.7f);
	if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->SetMenuMode(true);
	}
}

void AMallGameMode::Win()
{
	if (State != EMallState::Playing)
	{
		return;
	}
	State = EMallState::Won;
	Objective = 4;
	UMallAudio::Get()->Play2D(GetWorld(), "objective", 0.8f, 0.8f);
	if (AMallPlayerController* PC = Cast<AMallPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->SetMenuMode(true);
	}
	UGameplayStatics::SetGamePaused(this, true);
}

void AMallGameMode::OnZombieKilled(AMallZombie* Z)
{
	// Keep corpses bounded: remove the oldest once too many are lying around.
	int32 Dead = 0;
	for (AMallZombie* Other : Zombies)
	{
		if (Other && Other->IsDead()) ++Dead;
	}
	while (Dead > 12)
	{
		AMallZombie* Oldest = nullptr;
		for (AMallZombie* Other : Zombies)
		{
			if (Other && Other->IsDead() && Other != Z && (!Oldest || Other->DeathTime < Oldest->DeathTime))
			{
				Oldest = Other;
			}
		}
		if (!Oldest)
		{
			break;
		}
		Zombies.Remove(Oldest);
		Oldest->Destroy();
		--Dead;
	}
}

void AMallGameMode::ReportNoise(const FVector& Location, float Radius)
{
	for (AMallZombie* Z : Zombies)
	{
		if (Z && !Z->IsDead())
		{
			Z->HearNoise(Location, Radius);
		}
	}
}

void AMallGameMode::Notify(const FString& Text, const FColor& Color, float Duration)
{
	FMallNote N;
	N.Text = Text;
	N.Color = Color;
	N.Duration = Duration;
	Notes.Add(N);
	if (Notes.Num() > 5)
	{
		Notes.RemoveAt(0);
	}
}

AMallZombie* AMallGameMode::SpawnZombie(const FVector& Location, EZombieKind Kind)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AMallZombie* Z = GetWorld()->SpawnActor<AMallZombie>(AMallZombie::StaticClass(), Location + FVector(0, 0, 95.f), FRotator(0, FMath::FRandRange(0.f, 360.f), 0), P);
	if (Z)
	{
		Z->Setup(Kind, ++SpawnSeed * 7919);
		Zombies.Add(Z);
	}
	return Z;
}

EZombieKind AMallGameMode::RollKind() const
{
	const float Runner[] = {0.14f, 0.22f, 0.3f, 0.25f, 0.25f};
	const float Brute[] = {0.03f, 0.08f, 0.12f, 0.1f, 0.1f};
	const float R = FMath::FRand();
	const int32 O = FMath::Clamp(Objective, 0, 4);
	if (R < Brute[O]) return EZombieKind::Brute;
	if (R < Brute[O] + Runner[O]) return EZombieKind::Runner;
	return EZombieKind::Walker;
}

bool AMallGameMode::FindSpawnPoint(FVector& Out) const
{
	AMallPlayer* P = GetPlayer();
	if (!P || !Builder || Builder->ZombieSpawns.Num() == 0)
	{
		return false;
	}
	const FVector Eye = P->GetEyeLocation();
	const FVector Fwd = P->Camera->GetForwardVector();
	TArray<FVector> Valid;
	for (const FVector& S : Builder->ZombieSpawns)
	{
		const float D = FVector::Dist(S, P->GetActorLocation());
		if (D < 1800.f || D > 7000.f)
		{
			continue;
		}
		const FVector Head = S + FVector(0, 0, 160.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpawnVis), false, P);
		const bool bOccluded = GetWorld()->LineTraceSingleByObjectType(Hit, Eye, Head, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		const bool bInView = FVector::DotProduct((Head - Eye).GetSafeNormal(), Fwd) > 0.45f;
		if (bOccluded || (!bInView && D > 2600.f))
		{
			Valid.Add(S);
		}
	}
	if (Valid.Num() == 0)
	{
		return false;
	}
	Out = Valid[FMath::RandRange(0, Valid.Num() - 1)];
	return true;
}

void AMallGameMode::TickDirector(float Dt)
{
	DirectorTimer -= Dt;
	if (DirectorTimer > 0.f)
	{
		return;
	}
	const int32 Desired[] = {14, 18, 26, 22, 0};
	const float Interval[] = {6.f, 4.5f, 1.4f, 3.f, 99.f};
	const int32 O = FMath::Clamp(Objective, 0, 4);
	DirectorTimer = Interval[O];
	int32 Alive = 0;
	for (AMallZombie* Z : Zombies)
	{
		if (Z && !Z->IsDead()) ++Alive;
	}
	if (Alive >= FMath::Min(Desired[O], 30))
	{
		return;
	}
	FVector Loc;
	if (FindSpawnPoint(Loc))
	{
		if (AMallZombie* Z = SpawnZombie(Loc, RollKind()))
		{
			if (Objective >= 2)
			{
				// The transmission draws them in.
				if (AMallPlayer* P = GetPlayer())
				{
					Z->HearNoise(P->GetActorLocation() + FMath::VRand() * 400.f, 1e6f);
				}
			}
		}
	}
}

void AMallGameMode::SetEmergencyMode(bool bOn)
{
	if (!Builder)
	{
		return;
	}
	for (UPointLightComponent* L : Builder->EmergencyLights)
	{
		if (L) L->SetIntensity(bOn ? 1600.f : 70.f);
	}
}

FString AMallGameMode::ObjectiveText() const
{
	switch (Objective)
	{
	case 0: return TEXT("Find a replacement fuse  -  Voltline Electronics, Level 2");
	case 1: return TEXT("Restore the emergency radio  -  Maintenance, Level 2 east service hall");
	case 2: return FString::Printf(TEXT("Survive while the distress call transmits  -  %d:%02d"), int32(TransmitTimer) / 60, int32(TransmitTimer) % 60);
	case 3: return TEXT("Extraction is here. Reach the main entrance  -  Level 1 west");
	default: return TEXT("You made it out.");
	}
}

bool AMallGameMode::ObjectiveMarker(FVector& OutLocation) const
{
	if (!Builder)
	{
		return false;
	}
	switch (Objective)
	{
	case 0:
		if (!bReadClue && PlayTime < 240.f)
		{
			return false;
		}
		OutLocation = Builder->FuseLocation;
		return true;
	case 1:
		OutLocation = Builder->RadioLocation;
		return true;
	case 3:
		OutLocation = Builder->ExitLocation;
		return true;
	default:
		return false;
	}
}

bool AMallGameMode::HandleInteract(AMallPlayer* P, AMallInteractable* I)
{
	UWorld* W = GetWorld();
	UMallAudio* A = UMallAudio::Get();
	const FColor Good(150, 225, 160), Info(230, 220, 190), Bad(235, 110, 90);

	auto AmmoSlot = [](EMallItem Item) -> int32
	{
		switch (Item)
		{
		case EMallItem::AmmoPistol: return 0;
		case EMallItem::AmmoRifle: return 1;
		case EMallItem::AmmoShells: return 2;
		case EMallItem::AmmoSniper: return 3;
		default: return -1;
		}
	};
	auto Apply = [&](EMallItem Item, int32 Amount) -> FString
	{
		const int32 S = AmmoSlot(Item);
		if (S >= 0)
		{
			P->AddAmmo(S, Amount);
			return FString::Printf(TEXT("+%d %s"), Amount, *MallWeapons::Def(S).AmmoName);
		}
		if (Item == EMallItem::Medkit)
		{
			P->Meds += Amount;
			return FString::Printf(TEXT("+%d medkit"), Amount);
		}
		if (Item == EMallItem::Food)
		{
			P->Heal(Amount);
			return FString::Printf(TEXT("+%d health"), Amount);
		}
		return FString();
	};

	switch (I->Item)
	{
	case EMallItem::AmmoPistol:
	case EMallItem::AmmoRifle:
	case EMallItem::AmmoShells:
	case EMallItem::AmmoSniper:
	case EMallItem::Medkit:
	case EMallItem::Food:
	{
		const FString Got = Apply(I->Item, I->Amount);
		Notify(I->Label.IsEmpty() ? Got : FString::Printf(TEXT("%s  (%s)"), *I->Label, *Got), Good);
		A->Play2D(W, "pickup", 0.6f);
		I->SetUsed();
		return true;
	}
	case EMallItem::Container:
	{
		TArray<FString> Parts;
		for (const FMallLoot& L : I->Contents)
		{
			const FString S = Apply(L.Item, L.Amount);
			if (!S.IsEmpty()) Parts.Add(S);
		}
		Notify(Parts.Num() ? FString::Printf(TEXT("Searched %s:  %s"), *I->Label, *FString::Join(Parts, TEXT(",  "))) : FString::Printf(TEXT("Searched %s: nothing useful"), *I->Label), Parts.Num() ? Good : Info);
		A->Play2D(W, Parts.Num() ? "pickup" : "dry", 0.6f);
		I->SetUsed();
		return true;
	}
	case EMallItem::WeaponAR:
	case EMallItem::WeaponShotgun:
	case EMallItem::WeaponSniper:
	{
		const int32 S = I->Item == EMallItem::WeaponAR ? 1 : (I->Item == EMallItem::WeaponShotgun ? 2 : 3);
		P->GiveWeapon(S, I->Amount);
		Notify(FString::Printf(TEXT("%s acquired  [%d]"), *MallWeapons::Def(S).Name, S + 1), FColor(255, 210, 120), 4.f);
		A->Play2D(W, "rack", 0.8f);
		A->Play2D(W, "pickup", 0.5f, 0.8f);
		I->SetUsed();
		return true;
	}
	case EMallItem::Fuse:
		P->bHasFuse = true;
		Objective = FMath::Max(Objective, 1);
		Notify(TEXT("Took the 30A replacement fuse"), FColor(255, 210, 120), 4.f);
		BigMessage = TEXT("FUSE ACQUIRED");
		BigMessageTime = 3.f;
		A->Play2D(W, "objective", 0.7f);
		I->SetUsed();
		return true;
	case EMallItem::Radio:
		if (!P->bHasFuse)
		{
			Notify(TEXT("The radio's power fuse is blown. You need a replacement."), Bad, 4.f);
			A->Play2D(W, "buzz", 0.6f);
			return false;
		}
		P->bHasFuse = false;
		Objective = 2;
		TransmitTimer = TransmitTotal;
		BigMessage = TEXT("DISTRESS CALL TRANSMITTING");
		BigMessageTime = 3.5f;
		Notify(TEXT("\"...Larkspur Galleria, we read you. Hold for seventy-five seconds.\""), Info, 6.f);
		A->Play2D(W, "radio", 0.9f);
		A->PlayAt(W, "alarm", I->GetActorLocation(), 1.f, 1.f, 6000.f);
		ReportNoise(I->GetActorLocation(), 9000.f);
		SetEmergencyMode(true);
		if (I->StatusLight)
		{
			I->StatusLight->SetLightColor(FLinearColor(0.1f, 1.f, 0.2f));
		}
		I->SetUsed();
		return true;
	case EMallItem::Note:
		bReadClue = true;
		Notify(TEXT("Incident board: \"Spare 30A fuses moved to Voltline stockroom (L2). Radio is in Maintenance, L2 east service hall.\""), Info, 8.f);
		A->Play2D(W, "pickup", 0.4f, 0.7f);
		return true;
	case EMallItem::Exit:
		if (Objective == 3)
		{
			Win();
			return true;
		}
		Notify(TEXT("The entrance shutter is locked down. Call for extraction first."), Bad, 4.f);
		A->Play2D(W, "clang", 0.5f);
		return false;
	default:
		return false;
	}
}

void AMallGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);

	for (int32 I = Notes.Num() - 1; I >= 0; --I)
	{
		Notes[I].Time += Dt;
		if (Notes[I].Time > Notes[I].Duration)
		{
			Notes.RemoveAt(I);
		}
	}
	BigMessageTime = FMath::Max(0.f, BigMessageTime - Dt);

	if (bShotsActive)
	{
		TickAutoShots(Dt);
	}
	if (bTestMode)
	{
		TickTest(Dt);
	}

	if (State != EMallState::Playing)
	{
		return;
	}
	PlayTime += Dt;
	TickDirector(Dt);

	if (Objective == 2)
	{
		TransmitTimer -= Dt;
		AlarmTimer -= Dt;
		if (AlarmTimer <= 0.f && Builder && Builder->RadioItem)
		{
			AlarmTimer = 6.f;
			UMallAudio::Get()->PlayAt(GetWorld(), "alarm", Builder->RadioItem->GetActorLocation(), 0.8f, 1.f, 6000.f);
			ReportNoise(Builder->RadioItem->GetActorLocation(), 5000.f);
		}
		if (TransmitTimer <= 0.f)
		{
			Objective = 3;
			BigMessage = TEXT("EXTRACTION INBOUND");
			BigMessageTime = 3.5f;
			Notify(TEXT("\"Chopper's on the roof lot - the west entrance shutter is opening. Move!\""), FColor(230, 220, 190), 6.f);
			UMallAudio::Get()->Play2D(GetWorld(), "objective", 0.8f);
			if (Builder)
			{
				if (Builder->ExitItem) Builder->ExitItem->bEnabled = true;
				if (Builder->ExitShutter)
				{
					Builder->ExitShutter->SetWorldLocation(Builder->ExitShutter->GetComponentLocation() + FVector(0, 0, 330.f));
					Builder->ExitShutter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}

	// Reaching the open entrance also counts as escaping.
	if (Objective == 3 && Builder)
	{
		if (AMallPlayer* P = GetPlayer())
		{
			if (FVector::Dist2D(P->GetActorLocation(), Builder->ExitLocation) < 260.f)
			{
				Win();
			}
		}
	}

	// Ambient life: distant impacts, electrical buzzes and far-off groans around the player.
	AmbientTimer -= Dt;
	if (AmbientTimer <= 0.f)
	{
		AmbientTimer = FMath::FRandRange(7.f, 16.f);
		if (AMallPlayer* P = GetPlayer())
		{
			const FVector Loc = P->GetActorLocation() + FVector(FMath::FRandRange(-2500.f, 2500.f), FMath::FRandRange(-2500.f, 2500.f), FMath::FRandRange(0.f, 600.f));
			static const FName Pool[] = {"thump", "buzz", "clang", "groan1", "groan3", "glass"};
			const FName Clip = Pool[FMath::RandRange(0, 5)];
			UMallAudio::Get()->PlayAt(GetWorld(), Clip, Loc, Clip == "buzz" ? 0.35f : 0.5f, FMath::FRandRange(0.7f, 1.0f), 6000.f);
		}
	}
}

void AMallGameMode::TickAutoShots(float Dt)
{
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		ShotTimer = FMath::Max(ShotTimer, 2.f);
		return;
	}
	ShotTimer -= Dt;
	if (ShotTimer > 0.f)
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (ShotIndex >= 0 && ShotIndex < Shots.Num() && !bShotTaken)
	{
		// Capture this frame, and only move the camera once the capture has happened.
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("AutoShots/shot_%02d.png"), ShotIndex));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogTemp, Display, TEXT("MALLSHOT %s"), *File);
		bShotTaken = true;
		ShotTimer = 0.4f;
		return;
	}
	bShotTaken = false;
	if (AMallPlayer* SP = GetPlayer())
	{
		SP->Health = SP->MaxHealth;
	}
	++ShotIndex;
	if (ShotIndex >= Shots.Num())
	{
		if (ShotIndex > Shots.Num())
		{
			FPlatformMisc::RequestExit(false);
		}
		ShotTimer = 1.0f;
		return;
	}
	if (PC && PC->GetPawn())
	{
		PC->GetPawn()->SetActorLocation(Shots[ShotIndex].GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(Shots[ShotIndex].Rotator());
		if (ACharacter* C = Cast<ACharacter>(PC->GetPawn()))
		{
			C->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			C->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	ShotTimer = 2.5f;
}

// ---------------------------------------------------------------------------------------------
// Automated gameplay test (-MallTest). Drives the real systems and writes Saved/MallTest.txt.
// ---------------------------------------------------------------------------------------------

void AMallGameMode::TestLine(const FString& S)
{
	UE_LOG(LogTemp, Display, TEXT("MALLTEST %s"), *S);
	TestLog.Add(S);
	FFileHelper::SaveStringArrayToFile(TestLog, *(FPaths::ProjectSavedDir() / FString::Printf(TEXT("MallTest_%d.txt"), TestPhase)));
}

void AMallGameMode::TestShot(const FString& Name)
{
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("AutoShots/test%d_%02d_%s.png"), TestPhase, TestShotCount++, *Name));
	FScreenshotRequest::RequestScreenshot(File, true, false);
}

bool AMallGameMode::TestWalkTo(const FVector& Target, float Dt, float Timeout)
{
	AMallPlayer* P = GetPlayer();
	if (!P)
	{
		return true;
	}
	if (TestPath.Num() == 0)
	{
		const int32 A = Builder->Nav.Nearest(GetWorld(), P->GetActorLocation());
		const int32 B = Builder->Nav.Nearest(GetWorld(), Target);
		Builder->Nav.FindPath(A, B, TestPath);
		TestPath.Add(Target);
		TestPathIndex = 0;
		TestWalkTime = 0.f;
	}
	TestWalkTime += Dt;
	const FVector Loc = P->GetActorLocation();
	FVector To = TestPath[TestPathIndex] - Loc;
	const float DZ = FMath::Abs(To.Z);
	To.Z = 0;
	const float ArriveRadius = TestPathIndex == TestPath.Num() - 1 ? 130.f : 60.f;
	if (To.Size() < ArriveRadius && DZ < 170.f)
	{
		++TestPathIndex;
		if (TestPathIndex >= TestPath.Num())
		{
			P->bScriptedMove = false;
			TestPath.Reset();
			return true;
		}
	}
	P->bScriptedMove = true;
	P->ScriptedMoveDir = To.GetSafeNormal();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(0, To.Rotation().Yaw, 0), Dt, 6.f));
	}
	if (TestWalkTime > Timeout)
	{
		TestLine(FString::Printf(TEXT("WALK TIMEOUT at %s heading for %s (node %d/%d)"), *Loc.ToCompactString(), *Target.ToCompactString(), TestPathIndex, TestPath.Num()));
		P->bScriptedMove = false;
		TestPath.Reset();
		return true;
	}
	return false;
}

void AMallGameMode::TickTest(float Dt)
{
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		return;
	}
	AMallPlayer* P = GetPlayer();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!P || !PC || !Builder)
	{
		return;
	}
	TestTimer += Dt;

	auto Next = [this]() { ++TestStep; TestTimer = 0.f; };
	auto Face = [&](const FVector& At) { PC->SetControlRotation((At - P->GetEyeLocation()).Rotation()); };
	auto Teleport = [&](const FVector& Floor, float Yaw)
	{
		P->SetActorLocation(Floor + FVector(0, 0, 92.f), false, nullptr, ETeleportType::TeleportPhysics);
		P->GetCharacterMovement()->StopMovementImmediately();
		PC->SetControlRotation(FRotator(0, Yaw, 0));
	};
	auto Inv = [&]() { return FString::Printf(TEXT("HP=%.0f meds=%d mags=[%d %d %d %d] res=[%d %d %d %d] owned=[%d%d%d%d] fuse=%d obj=%d"),
		P->Health, P->Meds, P->Mag[0], P->Mag[1], P->Mag[2], P->Mag[3], P->Reserve[0], P->Reserve[1], P->Reserve[2], P->Reserve[3],
		P->bOwned[0], P->bOwned[1], P->bOwned[2], P->bOwned[3], P->bHasFuse, Objective); };

	if (TestPhase == 2)
	{
		// After restart: verify a clean reset, then die.
		switch (TestStep)
		{
		case 0:
			if (TestTimer < 3.f) return;
			TestLine(FString::Printf(TEXT("RESTART state=%d %s zombies=%d interactables used=%d"), int32(State), *Inv(), Zombies.Num(),
				Builder->Interactables.FilterByPredicate([](const AMallInteractable* I) { return I && I->bUsed; }).Num()));
			Next();
			return;
		case 1:
			P->TakeHit(500.f, P->GetActorLocation() + FVector(100, 0, 0));
			Next();
			return;
		case 2:
			if (TestTimer < 1.5f) return;
			TestLine(FString::Printf(TEXT("DEATH state=%d (3=Dead) health=%.0f"), int32(State), P->Health));
			TestShot(TEXT("death"));
			Next();
			return;
		case 3:
			if (TestTimer < 1.5f) return;
			TestLine(TEXT("DONE"));
			FPlatformMisc::RequestExit(false);
			Next();
			return;
		default:
			return;
		}
	}

	switch (TestStep)
	{
	case 0:
		if (TestTimer < 4.f) return;
		TestLine(FString::Printf(TEXT("START nav=%d links=%d interactables=%d zombies=%d %s"), Builder->Nav.Nodes.Num(), Builder->Nav.NumLinks(), Builder->Interactables.Num(), Zombies.Num(), *Inv()));
		{
			// Every node should be reachable from the player start.
			const int32 S = Builder->Nav.Nearest(GetWorld(), Builder->PlayerStart + FVector(0, 0, 90));
			int32 Unreachable = 0;
			TArray<FVector> Tmp;
			for (int32 I = 0; I < Builder->Nav.Nodes.Num(); ++I)
			{
				if (!Builder->Nav.FindPath(S, I, Tmp))
				{
					++Unreachable;
					if (Unreachable < 12) TestLine(FString::Printf(TEXT("  unreachable node %d at %s"), I, *(Builder->Nav.Nodes[I].Pos / 100.f).ToCompactString()));
				}
			}
			TestLine(FString::Printf(TEXT("NAV unreachable=%d"), Unreachable));
		}
		// Freeze zombies during the traversal test so it measures collision, not combat.
		for (AMallZombie* Z : Zombies) if (Z) Z->SetActorTickEnabled(false);
		TestShot(TEXT("start"));
		Next();
		return;
	case 1: // walk to the bottom of the west staircase
		if (TestWalkTo(Builder->StairBottom + FVector(0, 0, 90), Dt, 60.f)) { TestLine(FString::Printf(TEXT("REACHED stair bottom t=%.1f z=%.0f"), TestWalkTime, P->GetActorLocation().Z)); Next(); }
		return;
	case 2: // climb it
		if (TestWalkTo(Builder->StairTop + FVector(0, 0, 90), Dt, 30.f)) { TestLine(FString::Printf(TEXT("REACHED stair top z=%.0f (expect ~690)"), P->GetActorLocation().Z)); TestShot(TEXT("stairtop")); Next(); }
		return;
	case 3: // electronics fuse
		if (TestWalkTo(Builder->FuseLocation, Dt, 60.f)) { TestLine(FString::Printf(TEXT("REACHED fuse area loc=%s"), *(P->GetActorLocation() / 100.f).ToCompactString())); Next(); }
		return;
	case 4: // over to the escalator and down
		if (TestWalkTo(Builder->EscTop + FVector(0, 0, 90), Dt, 60.f)) { TestLine(FString::Printf(TEXT("REACHED escalator top z=%.0f"), P->GetActorLocation().Z)); Next(); }
		return;
	case 5:
		if (TestWalkTo(Builder->EscBottom + FVector(0, 0, 90), Dt, 30.f)) { TestLine(FString::Printf(TEXT("REACHED escalator bottom z=%.0f (expect ~90)"), P->GetActorLocation().Z)); Next(); }
		return;
	case 6:
	{
		// Pick up every weapon / supply interactable by standing in front of it.
		static int32 Idx = 0;
		if (TestTimer < 0.3f) return;
		if (Idx >= Builder->Interactables.Num())
		{
			TestLine(FString::Printf(TEXT("PICKUPS done %s"), *Inv()));
			Next();
			return;
		}
		AMallInteractable* I = Builder->Interactables[Idx];
		if (TestNum == 0.f)
		{
			if (I && !I->bUsed && I->Item != EMallItem::Fuse && I->Item != EMallItem::Radio && I->Item != EMallItem::Exit && I->Item != EMallItem::Note)
			{
				const FVector F = I->FocusPoint();
				FVector Back = (Builder->Nav.Nodes[Builder->Nav.Nearest(GetWorld(), F)].Pos - F).GetSafeNormal2D();
				if (Back.IsNearlyZero()) Back = FVector(1, 0, 0);
				P->SetActorLocation(FVector(F.X, F.Y, F.Z) + Back * 110.f + FVector(0, 0, 40.f), false, nullptr, ETeleportType::TeleportPhysics);
				TestNum = 1.f;
				TestTimer = 0.f;
				return;
			}
			++Idx;
			return;
		}
		Face(I->FocusPoint());
		if (TestTimer < 0.4f) return;
		const bool bOk = P->Interact();
		TestLine(FString::Printf(TEXT("  interact '%s' item=%d focus=%s ok=%d"), *I->Prompt, int32(I->Item), P->Focus ? *P->Focus->Prompt : TEXT("none"), bOk));
		if (!bOk)
		{
			TestLine(FString::Printf(TEXT("    (at %s)"), *(I->FocusPoint() / 100.f).ToCompactString()));
		}
		TestNum = 0.f;
		++Idx;
		return;
	}
	case 7:
	{
		// Weapon test: each weapon vs a fresh walker 7 m away on the concourse.
		const int32 W = TestWeapon;
		if (TestTimer < 0.1f && !TestZombie)
		{
			Teleport(M(-10, 0, 0), 0.f);
			P->SwitchTo(W);
			TestZombie = SpawnZombie(M(-3, 0, 0), EZombieKind::Walker);
			TestZombie->SetActorTickEnabled(false);
			TestNum = float(P->Mag[W] + P->Reserve[W]);
			return;
		}
		if (TestTimer < 1.0f) return;
		if (TestZombie && !TestZombie->IsDead() && TestTimer < 8.f)
		{
			Face(TestZombie->GetActorLocation() + FVector(0, 0, 30));
			P->TryFire(true);
			return;
		}
		TestLine(FString::Printf(TEXT("WEAPON %d %s: zombie dead=%d hp=%.0f shots=%d mag=%d res=%d total %d->%d"), W, *MallWeapons::Def(W).Name,
			TestZombie ? TestZombie->IsDead() : -1, TestZombie ? TestZombie->Health : 0.f, P->ShotsFired, P->Mag[W], P->Reserve[W], int32(TestNum), P->Mag[W] + P->Reserve[W]));
		TestShot(FString::Printf(TEXT("weapon%d"), W));
		TestZombie = nullptr;
		TestWeapon = W + 1;
		TestTimer = 0.f;
		if (TestWeapon >= 4)
		{
			Next();
		}
		return;
	}
	case 8:
	{
		// Reload conservation: fire a few, reload, compare totals.
		if (TestTimer < 0.1f)
		{
			P->SwitchTo(1);
			return;
		}
		if (TestTimer < 1.0f) return;
		if (TestNum >= 0.f && TestTimer < 1.1f)
		{
			for (int32 I = 0; I < 3; ++I) { P->TryFire(true); }
			TestNum = float(P->Mag[1] + P->Reserve[1]);
			return;
		}
		if (TestTimer < 1.5f) { P->StartReload(); return; }
		if (TestTimer < 1.8f) { P->SwitchTo(0); return; }   // cancel mid-reload
		if (TestTimer < 2.6f) return;
		TestLine(FString::Printf(TEXT("RELOAD-CANCEL rifle total before=%d after=%d (must match) reloading=%d"), int32(TestNum), P->Mag[1] + P->Reserve[1], P->IsReloading()));
		Next();
		return;
	}
	case 9:
	{
		// Melee test: a walker next to the player must deal damage...
		if (!TestZombie)
		{
			Teleport(M(-10, 0, 0), 0.f);
			TestNum = P->Health;
			TestZombie = SpawnZombie(M(-8.6f, 0, 0), EZombieKind::Walker);
			TestTimer = 0.f;
			return;
		}
		if (TestTimer < 5.f) return;
		TestLine(FString::Printf(TEXT("MELEE open: health %.0f -> %.0f (expect lower)"), TestNum, P->Health));
		TestZombie->Destroy();
		Zombies.Remove(TestZombie);
		TestZombie = nullptr;
		P->Health = 100.f;
		Next();
		return;
	}
	case 10:
	{
		// ...but never through the floor: zombie directly below an upstairs player.
		if (!TestZombie)
		{
			Teleport(Builder->StairTop + FVector(300, 0, 0), 0.f);
			TestA = P->GetActorLocation();
			TestNum = P->Health;
			TestZombie = SpawnZombie(FVector(TestA.X, TestA.Y - 800.f, 0.f), EZombieKind::Runner);
			TestTimer = 0.f;
			return;
		}
		if (TestTimer < 1.f) return;
		if (TestTimer < 1.1f)
		{
			TestZombie->HearNoise(P->GetActorLocation(), 1e5f);
			return;
		}
		if (TestTimer < 40.f && TestZombie->GetActorLocation().Z < 500.f) return;
		TestLine(FString::Printf(TEXT("FLOOR NAV: runner reached z=%.0f after %.1fs dist=%.0f health %.0f->%.0f"),
			TestZombie->GetActorLocation().Z, TestTimer, FVector::Dist(TestZombie->GetActorLocation(), P->GetActorLocation()), TestNum, P->Health));
		TestShot(TEXT("floornav"));
		TestZombie->Destroy();
		Zombies.Remove(TestZombie);
		TestZombie = nullptr;
		P->Health = 100.f;
		Next();
		return;
	}
	case 11:
	{
		// Objective chain in order.
		if (TestTimer < 0.1f)
		{
			// Radio before fuse must fail.
			TestLine(FString::Printf(TEXT("OBJ radio-before-fuse ok=%d (expect 0)"), HandleInteract(P, Builder->RadioItem)));
			TestLine(FString::Printf(TEXT("OBJ exit-early ok=%d (expect 0)"), HandleInteract(P, Builder->ExitItem)));
			HandleInteract(P, Builder->FuseItem);
			TestLine(FString::Printf(TEXT("OBJ fuse -> objective=%d fuse=%d"), Objective, P->bHasFuse));
			const bool bRadio = HandleInteract(P, Builder->RadioItem);
			TestLine(FString::Printf(TEXT("OBJ radio ok=%d -> objective=%d"), bRadio, Objective));
			Teleport(Builder->RadioLocation + FVector(-150, 0, 0), 0.f);
			return;
		}
		if (TestTimer < 3.f) return;
		TestShot(TEXT("radio"));
		TransmitTimer = 0.5f;
		Next();
		return;
	}
	case 12:
		if (TestTimer < 2.f) return;
		TestLine(FString::Printf(TEXT("OBJ after transmit objective=%d exitEnabled=%d"), Objective, Builder->ExitItem ? Builder->ExitItem->bEnabled : -1));
		Teleport(Builder->ExitLocation + FVector(600, 0, 0), 180.f);
		Next();
		return;
	case 13:
		if (TestTimer < 1.f) return;
		TestShot(TEXT("exit"));
		if (TestWalkTo(Builder->ExitLocation, Dt, 15.f) || State == EMallState::Won)
		{
			TestLine(FString::Printf(TEXT("WIN state=%d (4=Won) time=%.0f kills=%d"), int32(State), PlayTime, P->Kills));
			Next();
		}
		return;
	case 14:
		if (TestTimer < 1.5f) return;
		TestShot(TEXT("victory"));
		Next();
		return;
	case 15:
		if (TestTimer < 1.5f) return;
		TestLine(TEXT("RESTARTING"));
		Restart();
		Next();
		return;
	default:
		return;
	}
}
