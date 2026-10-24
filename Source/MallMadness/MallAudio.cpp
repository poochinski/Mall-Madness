#include "MallAudio.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/World.h"

UMallAudio* UMallAudio::Instance = nullptr;

static constexpr int32 GRate = 44100;

UMallAudio* UMallAudio::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UMallAudio>(GetTransientPackage(), TEXT("MallAudioLib"));
		Instance->AddToRoot();
		Instance->Init();
	}
	return Instance;
}

// ---------------------------------------------------------------------------------------------
// Synthesis helpers
// ---------------------------------------------------------------------------------------------
namespace
{
	struct FRng
	{
		uint32 S = 12345;
		float Next() { S = S * 1664525u + 1013904223u; return ((S >> 8) & 0xFFFF) / 32767.5f - 1.f; }
	};

	struct FBiquad
	{
		float B0 = 1, B1 = 0, B2 = 0, A1 = 0, A2 = 0, X1 = 0, X2 = 0, Y1 = 0, Y2 = 0;
		void BandPass(float F, float Q)
		{
			const float W = 2 * PI * F / GRate, Al = FMath::Sin(W) / (2 * Q), C = FMath::Cos(W), A0 = 1 + Al;
			B0 = Al / A0; B1 = 0; B2 = -Al / A0; A1 = -2 * C / A0; A2 = (1 - Al) / A0;
		}
		void LowPass(float F, float Q = 0.707f)
		{
			const float W = 2 * PI * F / GRate, Al = FMath::Sin(W) / (2 * Q), C = FMath::Cos(W), A0 = 1 + Al;
			B0 = (1 - C) / 2 / A0; B1 = (1 - C) / A0; B2 = B0; A1 = -2 * C / A0; A2 = (1 - Al) / A0;
		}
		void HighPass(float F, float Q = 0.707f)
		{
			const float W = 2 * PI * F / GRate, Al = FMath::Sin(W) / (2 * Q), C = FMath::Cos(W), A0 = 1 + Al;
			B0 = (1 + C) / 2 / A0; B1 = -(1 + C) / A0; B2 = B0; A1 = -2 * C / A0; A2 = (1 - Al) / A0;
		}
		float Run(float X)
		{
			const float Y = B0 * X + B1 * X1 + B2 * X2 - A1 * Y1 - A2 * Y2;
			X2 = X1; X1 = X; Y2 = Y1; Y1 = Y;
			return Y;
		}
	};

	using FBuf = TArray<float>;

	FBuf Make(float Seconds) { FBuf B; B.SetNumZeroed(int32(Seconds * GRate)); return B; }

	void Normalize(FBuf& B, float Peak)
	{
		float Max = 0.0001f;
		for (float V : B) Max = FMath::Max(Max, FMath::Abs(V));
		for (float& V : B) V = V / Max * Peak;
	}

	/** Filtered noise burst with exponential decay, added at Offset seconds. */
	void Noise(FBuf& B, float Offset, float Decay, float Amp, float LowCut, float HighCut, uint32 Seed, float Attack = 0.0005f)
	{
		FRng R; R.S = Seed;
		FBiquad LP, HP; LP.LowPass(HighCut); HP.HighPass(LowCut);
		const int32 Start = int32(Offset * GRate);
		for (int32 I = Start; I < B.Num(); ++I)
		{
			const float T = (I - Start) / float(GRate);
			const float Env = FMath::Min(1.f, T / Attack) * FMath::Exp(-T / Decay);
			if (Env < 0.0005f && T > Attack) break;
			B[I] += HP.Run(LP.Run(R.Next())) * Env * Amp;
		}
	}

	/** Sine with exponential pitch glide and decay. */
	void Tone(FBuf& B, float Offset, float F0, float F1, float Decay, float Amp, float Glide = 0.05f)
	{
		const int32 Start = int32(Offset * GRate);
		float Phase = 0;
		for (int32 I = Start; I < B.Num(); ++I)
		{
			const float T = (I - Start) / float(GRate);
			const float Env = FMath::Min(1.f, T / 0.001f) * FMath::Exp(-T / Decay);
			if (Env < 0.0005f && T > 0.01f) break;
			const float F = F1 + (F0 - F1) * FMath::Exp(-T / Glide);
			Phase += 2 * PI * F / GRate;
			B[I] += FMath::Sin(Phase) * Env * Amp;
		}
	}

	/** Voice-like source (sawtooth + formants) for zombie groans and player grunts. */
	void Voice(FBuf& B, float F0, float F1, float Amp, float Rasp, float F1Formant, float F2Formant, uint32 Seed, float AttackFrac = 0.15f)
	{
		FRng R; R.S = Seed;
		FBiquad Fa, Fb, Lp; Fa.BandPass(F1Formant, 4.f); Fb.BandPass(F2Formant, 6.f); Lp.LowPass(2800.f);
		float Phase = 0;
		const float Len = B.Num() / float(GRate);
		for (int32 I = 0; I < B.Num(); ++I)
		{
			const float T = I / float(GRate);
			const float U = T / Len;
			const float Env = FMath::Min(1.f, U / AttackFrac) * FMath::Min(1.f, (1.f - U) / 0.3f);
			const float F = FMath::Lerp(F0, F1, U) * (1.f + 0.04f * FMath::Sin(T * 2 * PI * 5.3f) + 0.02f * R.Next());
			Phase += F / GRate;
			Phase -= FMath::FloorToFloat(Phase);
			const float Saw = Phase * 2.f - 1.f;
			const float Mod = 1.f - Rasp + Rasp * (0.5f + 0.5f * FMath::Sin(T * 2 * PI * 31.f));
			const float Src = Saw * Mod + R.Next() * 0.35f * Rasp;
			B[I] += Lp.Run(Fa.Run(Src) * 1.2f + Fb.Run(Src) * 0.7f) * Env * Amp;
		}
	}

	/** Cheap room: a few decaying echoes. */
	void Room(FBuf& B, float Delay, float Feedback, int32 Taps)
	{
		const int32 D = int32(Delay * GRate);
		for (int32 Tap = 1; Tap <= Taps; ++Tap)
		{
			const int32 Off = D * Tap + Tap * 37;
			const float G = FMath::Pow(Feedback, float(Tap));
			for (int32 I = B.Num() - 1; I >= Off; --I)
			{
				B[I] += B[I - Off] * G * 0.5f;
			}
		}
	}

	TArray<int16> ToPcm(const FBuf& B)
	{
		TArray<int16> Out;
		Out.SetNumUninitialized(B.Num());
		for (int32 I = 0; I < B.Num(); ++I)
		{
			Out[I] = (int16)FMath::Clamp(int32(B[I] * 32767.f), -32767, 32767);
		}
		return Out;
	}
}

void UMallAudio::Generate()
{
	auto Store = [this](FName Name, FBuf& B, float Peak = 0.9f) { Normalize(B, Peak); Clips.Add(Name, ToPcm(B)); };

	{ // Handgun: sharp crack, short body, small room.
		FBuf B = Make(0.55f);
		Noise(B, 0, 0.010f, 1.0f, 900, 9000, 1);
		Noise(B, 0, 0.045f, 0.7f, 120, 2500, 2);
		Tone(B, 0, 220, 70, 0.05f, 0.9f, 0.02f);
		Noise(B, 0.01f, 0.16f, 0.12f, 150, 1600, 3, 0.01f);
		Room(B, 0.071f, 0.45f, 3);
		Store("shot_pistol", B);
	}
	{ // AR15: tighter, punchier mid.
		FBuf B = Make(0.5f);
		Noise(B, 0, 0.008f, 1.0f, 1200, 10000, 11);
		Noise(B, 0, 0.035f, 0.8f, 180, 3500, 12);
		Tone(B, 0, 180, 80, 0.04f, 0.8f, 0.015f);
		Noise(B, 0.01f, 0.13f, 0.12f, 200, 2000, 13, 0.01f);
		Room(B, 0.063f, 0.4f, 3);
		Store("shot_ar", B);
	}
	{ // Shotgun: heavy boom and long tail.
		FBuf B = Make(0.95f);
		Noise(B, 0, 0.018f, 1.0f, 500, 7000, 21);
		Noise(B, 0, 0.09f, 0.9f, 60, 1800, 22);
		Tone(B, 0, 120, 42, 0.12f, 1.1f, 0.04f);
		Noise(B, 0.02f, 0.32f, 0.18f, 80, 1200, 23, 0.02f);
		Room(B, 0.09f, 0.5f, 4);
		Store("shot_shotgun", B);
	}
	{ // Sniper: supersonic crack plus rolling echo.
		FBuf B = Make(1.6f);
		Noise(B, 0, 0.006f, 1.0f, 1500, 12000, 31);
		Noise(B, 0, 0.06f, 0.9f, 80, 2500, 32);
		Tone(B, 0, 150, 50, 0.10f, 1.0f, 0.03f);
		Noise(B, 0.03f, 0.5f, 0.16f, 60, 900, 33, 0.03f);
		Room(B, 0.17f, 0.5f, 4);
		Store("shot_sniper", B);
	}
	{ FBuf B = Make(0.08f); Noise(B, 0, 0.003f, 1.f, 2000, 9000, 41); Tone(B, 0, 3200, 2800, 0.006f, 0.5f); Store("dry", B, 0.5f); }
	{ FBuf B = Make(0.3f); Noise(B, 0, 0.004f, 1.f, 1500, 8000, 51); Noise(B, 0.02f, 0.05f, 0.4f, 600, 4000, 52, 0.02f); Tone(B, 0, 1900, 1700, 0.02f, 0.4f); Store("mag_out", B, 0.55f); }
	{ FBuf B = Make(0.3f); Noise(B, 0, 0.006f, 1.f, 800, 7000, 61); Tone(B, 0, 1500, 1300, 0.03f, 0.6f); Tone(B, 0.002f, 2600, 2400, 0.02f, 0.3f); Noise(B, 0, 0.03f, 0.4f, 100, 900, 62); Store("mag_in", B, 0.6f); }
	{ FBuf B = Make(0.4f); Noise(B, 0, 0.07f, 0.5f, 900, 5000, 71, 0.03f); Noise(B, 0.16f, 0.006f, 1.f, 1000, 8000, 72); Tone(B, 0.16f, 1700, 1500, 0.03f, 0.5f); Store("rack", B, 0.6f); }
	{ FBuf B = Make(0.5f); Noise(B, 0, 0.008f, 1.f, 600, 6000, 81); Noise(B, 0.02f, 0.06f, 0.4f, 400, 3000, 82, 0.02f); Noise(B, 0.2f, 0.008f, 1.f, 600, 6000, 83); Tone(B, 0.2f, 900, 800, 0.03f, 0.5f); Store("pump", B, 0.7f); }
	{ FBuf B = Make(0.7f); Noise(B, 0, 0.005f, 0.8f, 1000, 7000, 91); Noise(B, 0.1f, 0.08f, 0.5f, 700, 4000, 92, 0.04f); Noise(B, 0.32f, 0.06f, 0.5f, 700, 4000, 93, 0.04f); Noise(B, 0.5f, 0.006f, 1.f, 1000, 8000, 94); Tone(B, 0.5f, 1800, 1600, 0.03f, 0.4f); Store("bolt", B, 0.6f); }
	{ FBuf B = Make(0.18f); Noise(B, 0, 0.006f, 1.f, 700, 6000, 101); Tone(B, 0, 700, 500, 0.03f, 0.5f); Store("shell_in", B, 0.55f); }
	{ FBuf B = Make(0.2f); Noise(B, 0, 0.03f, 1.f, 60, 900, 111, 0.003f); Tone(B, 0, 90, 60, 0.03f, 0.6f); Noise(B, 0.01f, 0.02f, 0.25f, 1500, 5000, 112); Store("step", B, 0.5f); }
	{ FBuf B = Make(0.14f); Noise(B, 0, 0.004f, 1.f, 2000, 9000, 121); Store("shell_drop", B, 0.25f); }

	// Zombie vocals - several takes so a crowd doesn't sound cloned.
	for (int32 V = 0; V < 4; ++V)
	{
		FBuf B = Make(1.4f + V * 0.25f);
		Voice(B, 105 - V * 8, 80 - V * 5, 1.f, 0.35f + V * 0.1f, 450 + V * 60, 1100 + V * 90, 131 + V);
		Room(B, 0.05f, 0.3f, 2);
		Store(*FString::Printf(TEXT("groan%d"), V), B, 0.8f);
	}
	{ FBuf B = Make(0.7f); Voice(B, 160, 120, 1.f, 0.8f, 650, 1500, 141, 0.05f); Store("snarl", B, 0.85f); }
	{ FBuf B = Make(0.55f); Voice(B, 260, 190, 1.f, 0.9f, 800, 1900, 142, 0.04f); Store("shriek", B, 0.8f); }
	{ FBuf B = Make(1.3f); Voice(B, 120, 50, 1.f, 0.5f, 420, 1000, 151, 0.05f); Store("zdeath", B, 0.8f); }
	{ FBuf B = Make(0.15f); Noise(B, 0, 0.03f, 1.f, 80, 1200, 161, 0.002f); Tone(B, 0, 200, 110, 0.04f, 0.6f); Store("hit_flesh", B, 0.7f); }
	{ FBuf B = Make(0.3f); Noise(B, 0, 0.03f, 1.f, 80, 1500, 171, 0.002f); Tone(B, 0, 2400, 2300, 0.06f, 0.35f); Tone(B, 0, 1800, 1750, 0.05f, 0.25f); Store("headshot", B, 0.7f); }
	{ FBuf B = Make(0.05f); Tone(B, 0, 4200, 4000, 0.008f, 1.f); Noise(B, 0, 0.004f, 0.4f, 3000, 9000, 181); Store("hitmark", B, 0.35f); }
	{ FBuf B = Make(0.3f); Tone(B, 0, 880, 880, 0.06f, 0.7f, 1.f); Tone(B, 0.08f, 1320, 1320, 0.09f, 0.7f, 1.f); Store("pickup", B, 0.45f); }
	{ FBuf B = Make(1.0f); Noise(B, 0, 0.25f, 0.6f, 1500, 6000, 191, 0.15f); Noise(B, 0.4f, 0.2f, 0.5f, 1200, 5000, 192, 0.1f); Tone(B, 0.75f, 660, 990, 0.12f, 0.5f, 0.05f); Store("heal", B, 0.5f); }
	{ FBuf B = Make(1.6f); Tone(B, 0, 523, 523, 0.5f, 0.5f, 1.f); Tone(B, 0.12f, 659, 659, 0.5f, 0.5f, 1.f); Tone(B, 0.24f, 784, 784, 0.6f, 0.5f, 1.f); Tone(B, 0.36f, 1046, 1046, 0.7f, 0.4f, 1.f); Store("objective", B, 0.5f); }
	{ // Radio: static bursts with a coded beep.
		FBuf B = Make(2.2f);
		Noise(B, 0, 0.9f, 0.6f, 600, 3500, 201, 0.05f);
		for (int32 I = 0; I < 6; ++I) Tone(B, 0.3f + I * 0.22f, 1000, 1000, 0.05f, 0.6f, 1.f);
		Store("radio", B, 0.55f);
	}
	{ // Alarm: two-tone siren burst.
		FBuf B = Make(1.2f);
		float Phase = 0;
		for (int32 I = 0; I < B.Num(); ++I)
		{
			const float T = I / float(GRate);
			const float F = FMath::Frac(T * 2.f) < 0.5f ? 740.f : 560.f;
			Phase += F / GRate;
			B[I] = (FMath::Frac(Phase) < 0.5f ? 1.f : -1.f) * 0.3f * FMath::Min(1.f, (1.2f - T) * 5.f);
		}
		FBiquad Lp; Lp.LowPass(2500);
		for (float& V : B) V = Lp.Run(V);
		Room(B, 0.12f, 0.5f, 3);
		Store("alarm", B, 0.45f);
	}
	{ FBuf B = Make(0.4f); Voice(B, 170, 120, 1.f, 0.5f, 700, 1600, 211, 0.05f); Noise(B, 0, 0.05f, 0.5f, 60, 600, 212); Store("hurt", B, 0.7f); }
	{ FBuf B = Make(0.18f); Noise(B, 0, 0.012f, 1.f, 600, 8000, 221); Noise(B, 0.005f, 0.05f, 0.3f, 100, 1200, 222); Store("impact", B, 0.5f); }
	{ FBuf B = Make(0.45f); Noise(B, 0, 0.004f, 1.f, 2000, 9000, 231); Tone(B, 0, 3100, 2200, 0.12f, 0.5f, 0.08f); Tone(B, 0, 4500, 3900, 0.06f, 0.3f); Store("impact_metal", B, 0.45f); }
	{ FBuf B = Make(0.3f); Noise(B, 0, 0.02f, 1.f, 2500, 10000, 241); Noise(B, 0.01f, 0.12f, 0.4f, 3000, 9000, 242); Store("glass", B, 0.5f); }
	{ FBuf B = Make(2.5f); Tone(B, 0, 60, 38, 0.35f, 1.f, 0.2f); Noise(B, 0, 0.6f, 0.5f, 30, 400, 251, 0.02f); Room(B, 0.21f, 0.6f, 4); Store("thump", B, 0.7f); }
	{ // Electrical buzz with crackle.
		FBuf B = Make(0.8f);
		FRng R; R.S = 261;
		for (int32 I = 0; I < B.Num(); ++I)
		{
			const float T = I / float(GRate);
			const float Hum = FMath::Sin(T * 2 * PI * 120.f) > 0 ? 0.25f : -0.25f;
			const float Crack = R.Next() > 0.995f ? R.Next() : 0.f;
			B[I] = (Hum + Crack * 1.5f) * FMath::Sin(PI * T / 0.8f);
		}
		FBiquad Lp; Lp.LowPass(3000);
		for (float& V : B) V = Lp.Run(V);
		Store("buzz", B, 0.4f);
	}
	{ // Metal clang (distant debris).
		FBuf B = Make(1.5f); Tone(B, 0, 410, 400, 0.4f, 0.6f, 1.f); Tone(B, 0, 1130, 1100, 0.25f, 0.4f, 1.f); Tone(B, 0, 1730, 1700, 0.15f, 0.3f, 1.f); Noise(B, 0, 0.01f, 0.6f, 500, 6000, 271); Room(B, 0.15f, 0.5f, 3); Store("clang", B, 0.5f);
	}
	{ // Ambience bed: ventilation rush + mains hum, seamless 8 s loop.
		const float Len = 8.f;
		FBuf B = Make(Len);
		FRng R; R.S = 281;
		FBiquad Lp, Lp2; Lp.LowPass(380); Lp2.LowPass(90);
		for (int32 I = 0; I < B.Num(); ++I)
		{
			const float T = I / float(GRate);
			const float N = R.Next();
			const float Rush = Lp.Run(N) * (0.8f + 0.2f * FMath::Sin(T * 2 * PI / Len * 2));
			const float Rumble = Lp2.Run(N) * 2.0f;
			const float Hum = FMath::Sin(T * 2 * PI * 60.f) * 0.05f + FMath::Sin(T * 2 * PI * 120.f) * 0.03f + FMath::Sin(T * 2 * PI * 180.f) * 0.012f;
			B[I] = Rush * 0.9f + Rumble + Hum;
		}
		// Crossfade the ends for a click-free loop.
		const int32 Fade = GRate / 2;
		for (int32 I = 0; I < Fade; ++I)
		{
			const float A = I / float(Fade);
			B[I] = B[I] * A + B[B.Num() - Fade + I] * (1 - A);
		}
		B.SetNum(B.Num() - Fade);
		Store("ambience", B, 0.32f);
	}
}

void UMallAudio::Init()
{
	Generate();
}

USoundWaveProcedural* UMallAudio::MakeWave(const TArray<int16>& Pcm)
{
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(GRate);
	Wave->NumChannels = 1;
	Wave->Duration = Pcm.Num() / float(GRate);
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	return Wave;
}

USoundAttenuation* UMallAudio::GetAttenuation(float Range)
{
	const int32 Key = FMath::RoundToInt(Range / 100.f);
	if (TObjectPtr<USoundAttenuation>* Found = Attenuations.Find(Key))
	{
		return *Found;
	}
	USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(Range * 0.06f, 0, 0);
	Att->Attenuation.FalloffDistance = Range;
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Att->Attenuation.bAttenuateWithLPF = true;
	Att->Attenuation.LPFRadiusMin = Range * 0.2f;
	Att->Attenuation.LPFRadiusMax = Range;
	Attenuations.Add(Key, Att);
	return Att;
}

void UMallAudio::Play2D(UWorld* World, FName Clip, float Volume, float Pitch)
{
	const TArray<int16>* Pcm = Clips.Find(Clip);
	if (!Pcm || !World || MasterVolume <= 0.001f)
	{
		return;
	}
	USoundWaveProcedural* Wave = MakeWave(*Pcm);
	if (UAudioComponent* C = UGameplayStatics::SpawnSound2D(World, Wave, Volume * MasterVolume, Pitch, 0.f, nullptr, false, true))
	{
		Active.Add({C, Now + Wave->Duration / FMath::Max(0.2f, Pitch) + 0.1});
	}
}

void UMallAudio::PlayAt(UWorld* World, FName Clip, const FVector& Loc, float Volume, float Pitch, float Range)
{
	const TArray<int16>* Pcm = Clips.Find(Clip);
	if (!Pcm || !World || MasterVolume <= 0.001f)
	{
		return;
	}
	USoundWaveProcedural* Wave = MakeWave(*Pcm);
	if (UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(World, Wave, Loc, FRotator::ZeroRotator, Volume * MasterVolume, Pitch, 0.f, GetAttenuation(Range), nullptr, true))
	{
		Active.Add({C, Now + Wave->Duration / FMath::Max(0.2f, Pitch) + 0.1});
	}
}

void UMallAudio::OnAmbienceUnderflow(USoundWaveProcedural* Wave, int32 SamplesRequired)
{
	const TArray<int16>* Pcm = Clips.Find("ambience");
	if (!Pcm || !Wave)
	{
		return;
	}
	// Feed the loop in chunks; volume scaling happens on the component.
	const int32 Chunk = FMath::Max(SamplesRequired, 8192);
	TArray<int16> Out;
	Out.SetNumUninitialized(Chunk);
	for (int32 I = 0; I < Chunk; ++I)
	{
		Out[I] = (*Pcm)[AmbienceCursor];
		AmbienceCursor = (AmbienceCursor + 1) % Pcm->Num();
	}
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Out.GetData()), Out.Num() * sizeof(int16));
}

void UMallAudio::StartAmbience(UWorld* World)
{
	if (AmbienceComp && AmbienceComp->IsPlaying())
	{
		AmbienceComp->SetVolumeMultiplier(MasterVolume);
		return;
	}
	AmbienceWave = NewObject<USoundWaveProcedural>(this);
	AmbienceWave->SetSampleRate(GRate);
	AmbienceWave->NumChannels = 1;
	AmbienceWave->Duration = INDEFINITELY_LOOPING_DURATION;
	AmbienceWave->bLooping = false;
	AmbienceWave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateUObject(this, &UMallAudio::OnAmbienceUnderflow);
	OnAmbienceUnderflow(AmbienceWave, GRate);
	AmbienceComp = UGameplayStatics::SpawnSound2D(World, AmbienceWave, MasterVolume, 1.f, 0.f, nullptr, true, false);
}

void UMallAudio::StopAll()
{
	for (FActive& A : Active)
	{
		if (A.Comp.IsValid())
		{
			A.Comp->Stop();
		}
	}
	Active.Reset();
	if (AmbienceComp)
	{
		AmbienceComp->Stop();
		AmbienceComp = nullptr;
	}
}

void UMallAudio::Tick(float DeltaTime)
{
	Now += DeltaTime;
	for (int32 I = Active.Num() - 1; I >= 0; --I)
	{
		if (!Active[I].Comp.IsValid())
		{
			Active.RemoveAtSwap(I);
		}
		else if (Now > Active[I].EndTime)
		{
			Active[I].Comp->Stop();
			Active.RemoveAtSwap(I);
		}
	}
	if (AmbienceComp)
	{
		AmbienceComp->SetVolumeMultiplier(MasterVolume);
	}
}
