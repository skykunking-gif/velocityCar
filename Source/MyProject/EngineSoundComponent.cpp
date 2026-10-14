// Copyright Epic Games, Inc. All Rights Reserved.


#include "EngineSoundComponent.h"
#include "EngineSoundProfile.h"
#include "GameAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundGenerator.h"

namespace
{
	/** Cutoff high enough to count as no filter */
	constexpr float OpenCutoff = 20000.0f;

	/** How quickly the loudness and the filter follow the throttle */
	constexpr float ControlSmoothingTime = 0.06f;

	/** Samples skipped between those compared when lining up snippets, and between the shifts tried. Lining up
	 *  goes by the low notes of the engine, so this loses little and saves most of the work */
	constexpr int32 AlignmentStride = 8;
	constexpr int32 AlignmentStep = 2;

	/**
	 *  Renders the engine sound on the audio thread. Every half snippet length a new snippet starts at the point
	 *  of the recording asked for, faded in and out with a Hann window so the overlapping pair always adds up to
	 *  full volume. Each new snippet is shifted a little so its waveform lines up with the one it fades into.
	 */
	class FEngineSoundGenerator : public ISoundGenerator
	{
	public:

		FEngineSoundGenerator(TSharedRef<const FEngineSoundSamples> InSamples, TSharedRef<FEngineSoundControl, ESPMode::ThreadSafe> InControl,
			float InOutputSampleRate, float GrainLength, float InGrainJitter, float GrainAlignment)
			: Samples(InSamples)
			, Control(InControl)
			, OutputSampleRate(InOutputSampleRate)
			, ReadStep(InSamples->SampleRate / InOutputSampleRate)
			, GrainJitter(InGrainJitter * InSamples->SampleRate)
			, AlignmentRange(FMath::RoundToInt(GrainAlignment * InSamples->SampleRate))
			, Random(static_cast<int32>(FPlatformTime::Cycles()))
		{
			// an even length so the two halves of the overlap line up exactly
			GrainFrames = FMath::Max(2, FMath::RoundToInt(GrainLength * OutputSampleRate / 2.0f) * 2);
			Gain = Control->Gain.load();
			SetCutoff(Control->Cutoff.load());
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			const float* Source = Samples->Samples.GetData();
			const int32 NumSourceSamples = Samples->Samples.Num();

			// follow the throttle a block at a time, ramping the loudness across the block so it doesn't click
			const float BlockSmoothing = 1.0f - FMath::Exp(-NumSamples / (OutputSampleRate * ControlSmoothingTime));
			const float StartGain = Gain;
			Gain += (Control->Gain.load() - Gain) * BlockSmoothing;
			SetCutoff(Cutoff + (Control->Cutoff.load() - Cutoff) * BlockSmoothing);

			for (int32 Frame = 0; Frame < NumSamples; ++Frame)
			{
				if (FramesToNextGrain <= 0)
				{
					StartGrain(NumSourceSamples);
					FramesToNextGrain = GrainFrames / 2;
				}

				--FramesToNextGrain;

				float Sample = 0.0f;

				for (int32 GrainIndex = Grains.Num() - 1; GrainIndex >= 0; --GrainIndex)
				{
					FGrain& Grain = Grains[GrainIndex];

					const int32 ReadIndex = static_cast<int32>(Grain.ReadPosition);
					const float Alpha = static_cast<float>(Grain.ReadPosition - ReadIndex);
					const float Value = FMath::Lerp(Source[ReadIndex], Source[FMath::Min(ReadIndex + 1, NumSourceSamples - 1)], Alpha);
					const float Window = 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * Grain.Age / GrainFrames);

					Sample += Value * Window;

					Grain.ReadPosition += ReadStep;

					if (++Grain.Age >= GrainFrames)
					{
						Grains.RemoveAtSwap(GrainIndex, EAllowShrinking::No);
					}
				}

				// two one pole low pass filters in a row
				Filter1 += FilterCoefficient * (Sample - Filter1);
				Filter2 += FilterCoefficient * (Filter1 - Filter2);

				OutAudio[Frame] = Filter2 * FMath::Lerp(StartGain, Gain, static_cast<float>(Frame) / NumSamples);
			}

			return NumSamples;
		}

		virtual int32 GetDesiredNumSamplesToRenderPerCallback() const override
		{
			return 256;
		}

	private:

		/** A snippet of the recording being played */
		struct FGrain
		{
			/** Where in the recording the snippet is, in source samples */
			double ReadPosition = 0.0;

			/** Frames played so far */
			int32 Age = 0;
		};

		/** Starts a snippet centred on the time in the recording asked for, moved a little at random,
		 *  then lined up with the snippet already playing */
		void StartGrain(int32 NumSourceSamples)
		{
			const int32 GrainSourceLength = FMath::CeilToInt32(GrainFrames * ReadStep) + 2;
			const double Center = Control->RecordingTime.load() * Samples->SampleRate + Random.FRandRange(-GrainJitter, GrainJitter);
			const int32 MaxStart = FMath::Max(NumSourceSamples - GrainSourceLength, 0);

			int32 Start = FMath::Clamp(FMath::RoundToInt32(Center - 0.5 * GrainSourceLength), 0, MaxStart);

			// the newest snippet is the one the new snippet fades into
			const FGrain* Newest = nullptr;

			for (const FGrain& Grain : Grains)
			{
				if (!Newest || Grain.Age < Newest->Age)
				{
					Newest = &Grain;
				}
			}

			if (Newest)
			{
				Start = FindAlignedStart(Start, static_cast<int32>(Newest->ReadPosition), MaxStart);
			}

			FGrain& Grain = Grains.AddDefaulted_GetRef();
			Grain.ReadPosition = Start;
		}

		/** Finds the start near Start whose waveform best matches what the playing snippet plays next, from Reference on,
		 *  across the half snippet the two overlap. Compares by normalized correlation so a louder spot doesn't win just for being loud */
		int32 FindAlignedStart(int32 Start, int32 Reference, int32 MaxStart) const
		{
			const float* Source = Samples->Samples.GetData();
			const int32 OverlapLength = FMath::FloorToInt32(GrainFrames / 2 * ReadStep);

			if (AlignmentRange <= 0 || Reference + OverlapLength >= Samples->Samples.Num())
			{
				return Start;
			}

			int32 BestStart = Start;
			float BestScore = -UE_MAX_FLT;

			for (int32 Offset = -AlignmentRange; Offset <= AlignmentRange; Offset += AlignmentStep)
			{
				const int32 Candidate = Start + Offset;

				if (Candidate < 0 || Candidate > MaxStart)
				{
					continue;
				}

				float Correlation = 0.0f;
				float Energy = 0.0f;

				for (int32 Index = 0; Index < OverlapLength; Index += AlignmentStride)
				{
					const float Value = Source[Candidate + Index];
					Correlation += Source[Reference + Index] * Value;
					Energy += Value * Value;
				}

				const float Score = Correlation * FMath::InvSqrt(Energy + UE_SMALL_NUMBER);

				if (Score > BestScore)
				{
					BestScore = Score;
					BestStart = Candidate;
				}
			}

			return BestStart;
		}

		void SetCutoff(float NewCutoff)
		{
			Cutoff = NewCutoff;

			// the filter is left out altogether when it's wide open
			FilterCoefficient = Cutoff >= OpenCutoff ? 1.0f : 1.0f - FMath::Exp(-UE_TWO_PI * Cutoff / OutputSampleRate);
		}

		TSharedRef<const FEngineSoundSamples> Samples;
		TSharedRef<FEngineSoundControl, ESPMode::ThreadSafe> Control;

		float OutputSampleRate;

		/** Source samples to step through for each output frame, to play the recording at its own pitch */
		double ReadStep;

		/** Most a snippet's start is moved at random, in source samples */
		float GrainJitter;

		/** Most a snippet's start is shifted to line it up with the one before, in source samples */
		int32 AlignmentRange;

		/** Length of a snippet in output frames */
		int32 GrainFrames = 2;

		int32 FramesToNextGrain = 0;

		TArray<FGrain, TInlineAllocator<4>> Grains;

		float Gain = 0.0f;
		float Cutoff = OpenCutoff;
		float FilterCoefficient = 1.0f;
		float Filter1 = 0.0f;
		float Filter2 = 0.0f;

		FRandomStream Random;
	};
}

UEngineSoundComponent::UEngineSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Control(MakeShared<FEngineSoundControl, ESPMode::ThreadSafe>())
{
	// nothing to play until a profile is set
	bAutoActivate = false;

	// heard all round the car at full volume, and further off fading with distance. NaturalSound loses
	// 60 dB over the falloff distance and keeps on fading at that rate past it.
	// A synth plays flat in 2D by default, which ignores the attenuation altogether
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

void UEngineSoundComponent::SetProfile(UEngineSoundProfile* NewProfile)
{
	if (NewProfile == Profile && IsPlaying())
	{
		return;
	}

	Stop();

	Profile = NewProfile;
	Samples = Profile ? Profile->LoadSamples() : nullptr;

	if (!Samples.IsValid() || Profile->RecordingRPM.IsEmpty())
	{
		return;
	}

	GrainLength = Profile->GrainLength;
	GrainJitter = Profile->GrainJitter;
	GrainAlignment = Profile->GrainAlignment;

	SetEngineState(0.0f, 0.0f);
	Start();
}

void UEngineSoundComponent::SetEngineState(float RPMFraction, float Throttle)
{
	if (!Profile)
	{
		return;
	}

	Throttle = FMath::Clamp(Throttle, 0.0f, 1.0f);

	Control->RecordingTime.store(Profile->GetRecordingTime(RPMFraction));
	Control->Gain.store(Profile->Volume * UGameAudioSettings::GetEngineVolume() * FMath::Lerp(Profile->OffThrottleVolume, 1.0f, Throttle));

	// sweep the cutoff evenly in pitch rather than in Hz
	const float OffCutoff = FMath::Min(Profile->OffThrottleCutoff, OpenCutoff);
	Control->Cutoff.store(FMath::Exp(FMath::Lerp(FMath::Loge(OffCutoff), FMath::Loge(OpenCutoff), Throttle)));
}

void UEngineSoundComponent::SetDistanceFalloff(float InnerRadius, float FalloffDistance)
{
	AttenuationOverrides.AttenuationShapeExtents = FVector(FMath::Max(InnerRadius, 0.0f), 0.0f, 0.0f);
	AttenuationOverrides.FalloffDistance = FMath::Max(FalloffDistance, 1.0f);

	// the synth hands its settings to its audio component when it starts, so a playing sound needs telling directly
	if (UAudioComponent* Audio = GetAudioComponent())
	{
		Audio->AdjustAttenuation(AttenuationOverrides);
	}
}

bool UEngineSoundComponent::Init(int32& SampleRate)
{
	NumChannels = 1;

	return true;
}

ISoundGeneratorPtr UEngineSoundComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	if (!Samples.IsValid())
	{
		return nullptr;
	}

	return MakeShared<FEngineSoundGenerator, ESPMode::ThreadSafe>(Samples.ToSharedRef(), Control, InParams.SampleRate, GrainLength, GrainJitter, GrainAlignment);
}
