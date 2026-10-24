#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Tickable.h"
#include "MallAudio.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundWaveProcedural;

/**
 * Synthesised audio: every sound effect is generated as PCM at startup (no external assets),
 * then played through procedural sound waves, either 2D or spatialised with attenuation.
 * Procedural sources never end on their own, so each play is stopped after its clip length.
 */
UCLASS()
class UMallAudio : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UMallAudio* Get();

	void Init();
	void Play2D(UWorld* World, FName Clip, float Volume = 1.f, float Pitch = 1.f);
	void PlayAt(UWorld* World, FName Clip, const FVector& Loc, float Volume = 1.f, float Pitch = 1.f, float Range = 3000.f);
	void StartAmbience(UWorld* World);
	void StopAll();

	float MasterVolume = 0.8f;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMallAudio, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !HasAnyFlags(RF_ClassDefaultObject); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

private:
	USoundWaveProcedural* MakeWave(const TArray<int16>& Pcm);
	USoundAttenuation* GetAttenuation(float Range);
	void Generate();
	void OnAmbienceUnderflow(USoundWaveProcedural* Wave, int32 SamplesRequired);

	TMap<FName, TArray<int16>> Clips;
	struct FActive
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		double EndTime;
	};
	TArray<FActive> Active;
	UPROPERTY() TMap<int32, TObjectPtr<USoundAttenuation>> Attenuations;
	UPROPERTY() TObjectPtr<UAudioComponent> AmbienceComp;
	UPROPERTY() TObjectPtr<USoundWaveProcedural> AmbienceWave;
	int32 AmbienceCursor = 0;
	double Now = 0.0;

	static UMallAudio* Instance;
};
