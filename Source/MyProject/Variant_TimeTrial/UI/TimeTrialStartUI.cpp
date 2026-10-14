// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialStartUI.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"

void UTimeTrialStartUI::StartCountdown()
{
	// pass control to BP
	BP_StartCountdown();
}

void UTimeTrialStartUI::SetCountdownPaused(bool bPaused)
{
	// the countdown is the Blueprint's animations playing one after another. An animation that isn't playing ignores this
	if (const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
	{
		for (const UWidgetAnimation* Animation : WidgetClass->Animations)
		{
			SetPlaybackSpeed(Animation, bPaused ? 0.0f : 1.0f);
		}
	}
}

void UTimeTrialStartUI::FinishCountdown()
{
	// broadcast the delegate
	OnCountdownFinished.Broadcast();
}
