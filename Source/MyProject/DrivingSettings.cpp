// Copyright Epic Games, Inc. All Rights Reserved.


#include "DrivingSettings.h"

bool UDrivingSettings::IsAutomaticTransmission(int32 PlayerIndex)
{
	const UDrivingSettings* Settings = GetDefault<UDrivingSettings>();

	return PlayerIndex > 0 ? Settings->bSecondPlayerAutomaticTransmission : Settings->bAutomaticTransmission;
}

void UDrivingSettings::SetAutomaticTransmission(bool bAutomatic, int32 PlayerIndex)
{
	UDrivingSettings* Settings = GetMutableDefault<UDrivingSettings>();
	bool& bSetting = PlayerIndex > 0 ? Settings->bSecondPlayerAutomaticTransmission : Settings->bAutomaticTransmission;

	if (bSetting == bAutomatic)
	{
		return;
	}

	bSetting = bAutomatic;
	Settings->SaveConfig();

	OnChanged().Broadcast();
}

FSimpleMulticastDelegate& UDrivingSettings::OnChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}
