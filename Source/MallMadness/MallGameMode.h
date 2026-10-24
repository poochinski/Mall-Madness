#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MallZombie.h"
#include "MallGameMode.generated.h"

class AMallBuilder;
class AMallPlayer;
class AMallInteractable;
class AMallFX;

UENUM()
enum class EMallState : uint8 { Title, Playing, Paused, Dead, Won };

enum class EMallButton : int32 { None = 0, Play, Resume, Restart, Quit, SensDown, SensUp, VolDown, VolUp };

struct FMallNote
{
	FString Text;
	FColor Color;
	float Time = 0.f;
	float Duration = 3.f;
};

UCLASS()
class AMallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMallGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- flow ----
	void StartGame();
	void SetPaused(bool bPause);
	void Restart();
	void OnButton(EMallButton Button);
	void OnPlayerDied();
	void OnZombieKilled(AMallZombie* Z);
	void Win();

	// ---- gameplay services ----
	AMallPlayer* GetPlayer() const;
	void ReportNoise(const FVector& Location, float Radius);
	void Notify(const FString& Text, const FColor& Color = FColor::White, float Duration = 3.f);
	bool HandleInteract(AMallPlayer* Player, AMallInteractable* Item);
	AMallZombie* SpawnZombie(const FVector& Location, EZombieKind Kind);

	// ---- HUD queries ----
	FString ObjectiveText() const;
	bool ObjectiveMarker(FVector& OutLocation) const;
	float TransmitProgress() const { return Objective == 2 ? 1.f - TransmitTimer / TransmitTotal : 0.f; }

	UPROPERTY() TObjectPtr<AMallBuilder> Builder;
	UPROPERTY() TArray<TObjectPtr<AMallZombie>> Zombies;
	EMallState State = EMallState::Title;
	int32 Objective = 0;
	float TransmitTimer = 0.f;
	float TransmitTotal = 75.f;
	float PlayTime = 0.f;
	bool bReadClue = false;
	TArray<FMallNote> Notes;
	FString BigMessage;
	float BigMessageTime = 0.f;

	static float Sensitivity;
	static float Volume;

private:
	void SpawnEnvironment();
	void TickDirector(float Dt);
	void TickAutoShots(float Dt);
	void TickTest(float Dt);
	bool FindSpawnPoint(FVector& Out) const;
	EZombieKind RollKind() const;
	void SetEmergencyMode(bool bOn);

	bool bRestarted = false;
	bool bTestMode = false;
	float DirectorTimer = 2.f;
	float AmbientTimer = 8.f;
	float AlarmTimer = 0.f;
	int32 SpawnSeed = 1;

	// Screenshot / test harness
	TArray<FTransform> Shots;
	int32 ShotIndex = -1;
	float ShotTimer = 0.f;
	bool bShotsActive = false;
	bool bShotTaken = false;
	int32 TestStep = 0;
	float TestTimer = 0.f;
	TArray<FVector> TestPath;
	int32 TestPathIndex = 0;
	float TestWalkTime = 0.f;
	TArray<FString> TestLog;
	void TestLine(const FString& S);
	bool TestWalkTo(const FVector& Target, float Dt, float Timeout);
	void TestShot(const FString& Name);
	int32 TestShotCount = 0;
	UPROPERTY() TObjectPtr<AMallZombie> TestZombie;
	int32 TestPhase = 0;
	int32 TestWeapon = 0;
	FVector TestA, TestB;
	float TestNum = 0.f;
};
