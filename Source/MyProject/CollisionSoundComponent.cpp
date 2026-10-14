// Copyright Epic Games, Inc. All Rights Reserved.


#include "CollisionSoundComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundGenerator.h"

namespace
{
	/** How quickly the scraping follows the car, in seconds */
	constexpr float ScrapeSmoothingTime = 0.03f;

	/** Resonances of the scraped metal, in Hz at a slow scrape, and how loud each rings */
	constexpr int32 NumScrapeResonances = 5;
	constexpr float ScrapeFrequencies[NumScrapeResonances] = { 1150.0f, 2630.0f, 4170.0f, 6050.0f, 8300.0f };
	constexpr float ScrapeGains[NumScrapeResonances] = { 1.0f, 0.8f, 0.6f, 0.45f, 0.3f };

	/** Sharpness of the scrape resonances */
	constexpr float ScrapeQ = 12.0f;

	/** Crackles a second at the slowest and fastest scrapes */
	constexpr float MinCrackleRate = 30.0f;
	constexpr float MaxCrackleRate = 350.0f;

	/** How long the scrape keeps one loudness before wavering to the next, in seconds */
	constexpr float FlutterStepTime = 0.015f;

	/** Ringing parts of a knock: frequency in Hz, how long it takes to die down to a third, in seconds, and loudness.
	 *  The two lowest are the thump of the body, the rest the clang of the metal */
	struct FImpactMode
	{
		float Frequency;
		float DecayTime;
		float Gain;
	};

	constexpr int32 NumImpactModes = 8;
	constexpr FImpactMode ImpactModes[NumImpactModes] =
	{
		{ 95.0f, 0.10f, 1.0f },
		{ 170.0f, 0.07f, 0.7f },
		{ 523.0f, 0.30f, 0.35f },
		{ 1187.0f, 0.22f, 0.28f },
		{ 1979.0f, 0.16f, 0.22f },
		{ 2861.0f, 0.11f, 0.18f },
		{ 4013.0f, 0.07f, 0.14f },
		{ 5550.0f, 0.05f, 0.10f },
	};

	/** Knocks that can ring at once. A new one past this replaces the oldest */
	constexpr int32 MaxImpactVoices = 4;

	/** How long the noise burst at the start of a knock lasts, in seconds */
	constexpr float ImpactNoiseTime = 0.008f;

	/** Loudness of the whole sound before the volume setting */
	constexpr float OutputGain = 1.0f;

	/** A band pass filter that rings at one frequency */
	struct FResonator
	{
		float B0 = 0.0f;
		float A1 = 0.0f;
		float A2 = 0.0f;
		float Z1 = 0.0f;
		float Z2 = 0.0f;

		void Set(float Frequency, float Q, float SampleRate)
		{
			const float Omega = 2.0f * PI * FMath::Min(Frequency, SampleRate * 0.45f) / SampleRate;
			const float Alpha = FMath::Sin(Omega) / (2.0f * Q);
			const float A0 = 1.0f + Alpha;

			B0 = Alpha / A0;
			A1 = -2.0f * FMath::Cos(Omega) / A0;
			A2 = (1.0f - Alpha) / A0;
		}

		float Process(float In)
		{
			// transposed direct form II, with the zero coefficients of a band pass left out
			const float Out = B0 * In + Z1;
			Z1 = -A1 * Out + Z2;
			Z2 = -B0 * In - A2 * Out;
			return Out;
		}
	};

	/** One knock ringing out: a decaying oscillator per ringing part, and the noise burst it starts with */
	struct FImpactVoice
	{
		float Re[NumImpactModes] = {};
		float Im[NumImpactModes] = {};
		float Cos[NumImpactModes] = {};
		float Sin[NumImpactModes] = {};
		float Decay[NumImpactModes] = {};
		float Noise = 0.0f;
		float Age = 0.0f;
		bool bActive = false;
	};

	/**
	 *  Renders the collision sound on the audio thread.
	 */
	class FCollisionSoundGenerator : public ISoundGenerator
	{
	public:

		FCollisionSoundGenerator(TSharedRef<FCollisionSoundControl, ESPMode::ThreadSafe> InControl, float InSampleRate, float InVolume)
			: Control(InControl)
			, SampleRate(InSampleRate)
			, Volume(InVolume)
			, Random(static_cast<int32>(FPlatformTime::Cycles()))
		{
			SetScrapeSpeed(0.0f);
			ClickDecay = FMath::Exp(-1.0f / (0.0007f * SampleRate));
			NoiseDecay = FMath::Exp(-1.0f / (ImpactNoiseTime * SampleRate));
			FlutterStep = FMath::Max(1, FMath::RoundToInt(FlutterStepTime * SampleRate));
			NoiseLowPass = 1.0f - FMath::Exp(-2.0f * PI * 3000.0f / SampleRate);
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			// follow the scrape a block at a time, ramping the loudness across the block so it doesn't click
			const float BlockSmoothing = 1.0f - FMath::Exp(-NumSamples / (SampleRate * ScrapeSmoothingTime));
			const float StartLevel = ScrapeLevel;
			ScrapeLevel += (Control->ScrapeLevel.load() - ScrapeLevel) * BlockSmoothing;
			SetScrapeSpeed(ScrapeSpeed + (Control->ScrapeSpeed.load() - ScrapeSpeed) * BlockSmoothing);

			const float Impact = Control->PendingImpact.exchange(0.0f);

			if (Impact > 0.0f)
			{
				StartImpact(Impact);
			}

			const float CrackleChance = FMath::Lerp(MinCrackleRate, MaxCrackleRate, ScrapeSpeed) / SampleRate;
			const bool bScraping = ScrapeLevel > 0.001f || StartLevel > 0.001f;

			for (int32 Frame = 0; Frame < NumSamples; ++Frame)
			{
				float Out = 0.0f;

				if (bScraping)
				{
					const float Noise = Random.FRandRange(-1.0f, 1.0f);

					// crackle: short sharp clicks of grit, more of them the faster the scrape
					if (Random.FRand() < CrackleChance)
					{
						Click = Random.FRandRange(0.5f, 1.0f);
					}

					Click *= ClickDecay;

					const float Excitation = Noise * (0.35f + 3.0f * Click);
					float Ring = 0.0f;

					for (int32 Index = 0; Index < NumScrapeResonances; ++Index)
					{
						Ring += ScrapeResonators[Index].Process(Excitation) * ScrapeGains[Index];
					}

					// the hiss of the rubbing, above the ringing
					const float Hiss = (Noise - LastNoise) * 0.12f;
					LastNoise = Noise;

					// the contact chatters, so the loudness wavers
					if (--FlutterCountdown <= 0)
					{
						FlutterCountdown = FlutterStep;
						FlutterTarget = Random.FRandRange(0.45f, 1.0f);
					}

					Flutter += (FlutterTarget - Flutter) * 0.01f;

					const float Level = FMath::Lerp(StartLevel, ScrapeLevel, static_cast<float>(Frame) / NumSamples);
					Out += (Ring * 8.0f + Hiss) * Level * Flutter;
				}

				for (FImpactVoice& Voice : Voices)
				{
					if (!Voice.bActive)
					{
						continue;
					}

					float Sum = 0.0f;

					for (int32 Mode = 0; Mode < NumImpactModes; ++Mode)
					{
						// turn each oscillator a step and let it die down a little
						const float Re = Voice.Re[Mode];
						const float Im = Voice.Im[Mode];
						Voice.Re[Mode] = (Re * Voice.Cos[Mode] - Im * Voice.Sin[Mode]) * Voice.Decay[Mode];
						Voice.Im[Mode] = (Re * Voice.Sin[Mode] + Im * Voice.Cos[Mode]) * Voice.Decay[Mode];
						Sum += Voice.Im[Mode];
					}

					// the crack of the first contact
					NoiseFilter += (Random.FRandRange(-1.0f, 1.0f) * Voice.Noise - NoiseFilter) * NoiseLowPass;
					Sum += NoiseFilter;
					Voice.Noise *= NoiseDecay;

					Out += Sum;
				}

				// round off the loudest knocks rather than clip them
				Out *= OutputGain * Volume;
				OutAudio[Frame] = Out / (1.0f + FMath::Abs(Out));
			}

			// let rung out knocks go
			const float BlockTime = NumSamples / SampleRate;

			for (FImpactVoice& Voice : Voices)
			{
				if (Voice.bActive)
				{
					Voice.Age += BlockTime;
					Voice.bActive = Voice.Age < 1.0f;
				}
			}

			return NumSamples;
		}

	private:

		/** Tunes the scrape resonances to the scrape speed: faster scrapes ring higher */
		void SetScrapeSpeed(float NewSpeed)
		{
			ScrapeSpeed = FMath::Clamp(NewSpeed, 0.0f, 1.0f);

			const float Tune = 0.85f + 0.3f * ScrapeSpeed;

			for (int32 Index = 0; Index < NumScrapeResonances; ++Index)
			{
				ScrapeResonators[Index].Set(ScrapeFrequencies[Index] * Tune, ScrapeQ, SampleRate);
			}
		}

		/** Starts a knock ringing, in a free voice or in place of the oldest */
		void StartImpact(float Strength)
		{
			Strength = FMath::Clamp(Strength, 0.0f, 1.0f);

			FImpactVoice* Voice = &Voices[0];

			for (FImpactVoice& Candidate : Voices)
			{
				if (!Candidate.bActive || Candidate.Age > Voice->Age)
				{
					Voice = &Candidate;

					if (!Candidate.bActive)
					{
						break;
					}
				}
			}

			*Voice = FImpactVoice();
			Voice->bActive = true;
			Voice->Noise = 0.6f * Strength;

			// harder knocks are louder and brighter. Each one is tuned a little differently so repeats don't sound alike
			const float Loudness = FMath::Pow(Strength, 0.8f);

			for (int32 Mode = 0; Mode < NumImpactModes; ++Mode)
			{
				const FImpactMode& Part = ImpactModes[Mode];
				const float Frequency = Part.Frequency * Random.FRandRange(0.94f, 1.06f);
				const float Omega = 2.0f * PI * Frequency / SampleRate;
				const float Brightness = Mode < 2 ? 1.0f : FMath::Lerp(0.4f, 1.0f, Strength);

				Voice->Cos[Mode] = FMath::Cos(Omega);
				Voice->Sin[Mode] = FMath::Sin(Omega);
				Voice->Decay[Mode] = FMath::Exp(-1.0f / (Part.DecayTime * SampleRate));
				Voice->Re[Mode] = Part.Gain * Loudness * Brightness;
			}
		}

		TSharedRef<FCollisionSoundControl, ESPMode::ThreadSafe> Control;
		float SampleRate;
		float Volume;
		FRandomStream Random;

		float ScrapeLevel = 0.0f;
		float ScrapeSpeed = 0.0f;
		FResonator ScrapeResonators[NumScrapeResonances];

		float Click = 0.0f;
		float ClickDecay = 0.0f;
		float LastNoise = 0.0f;
		float Flutter = 1.0f;
		float FlutterTarget = 1.0f;
		int32 FlutterStep = 1;
		int32 FlutterCountdown = 0;

		FImpactVoice Voices[MaxImpactVoices];
		float NoiseDecay = 0.0f;
		float NoiseFilter = 0.0f;
		float NoiseLowPass = 1.0f;
	};
}

UCollisionSoundComponent::UCollisionSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Control(MakeShared<FCollisionSoundControl, ESPMode::ThreadSafe>())
{
	// started with the car, and silent until something is hit
	bAutoActivate = true;

	// fades with distance like the engine. A synth plays flat in 2D by default, which ignores the attenuation altogether
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	AttenuationOverrides.AttenuationShapeExtents = FVector(1500.0f, 0.0f, 0.0f);
	AttenuationOverrides.FalloffDistance = 8000.0f;
	AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	// the chase camera is close enough that the player's own car would swing from speaker to speaker
	AttenuationOverrides.NonSpatializedRadiusStart = 1200.0f;
	AttenuationOverrides.NonSpatializedRadiusEnd = 800.0f;
}

void UCollisionSoundComponent::SetScrape(float Level, float Speed)
{
	Control->ScrapeLevel.store(FMath::Clamp(Level, 0.0f, 1.0f));
	Control->ScrapeSpeed.store(FMath::Clamp(Speed, 0.0f, 1.0f));
}

void UCollisionSoundComponent::PlayImpact(float Strength)
{
	// two knocks landing before the renderer picks either up play as the harder one
	float Pending = Control->PendingImpact.load();
	const float Clamped = FMath::Clamp(Strength, 0.0f, 1.0f);

	while (Clamped > Pending && !Control->PendingImpact.compare_exchange_weak(Pending, Clamped))
	{
	}
}

void UCollisionSoundComponent::SetDistanceFalloff(float InnerRadius, float FalloffDistance)
{
	AttenuationOverrides.AttenuationShapeExtents = FVector(FMath::Max(InnerRadius, 0.0f), 0.0f, 0.0f);
	AttenuationOverrides.FalloffDistance = FMath::Max(FalloffDistance, 1.0f);

	// the synth hands its settings to its audio component when it starts, so a playing sound needs telling directly
	if (UAudioComponent* Audio = GetAudioComponent())
	{
		Audio->AdjustAttenuation(AttenuationOverrides);
	}
}

bool UCollisionSoundComponent::Init(int32& SampleRate)
{
	NumChannels = 1;

	return true;
}

ISoundGeneratorPtr UCollisionSoundComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FCollisionSoundGenerator, ESPMode::ThreadSafe>(Control, InParams.SampleRate, Volume);
}
