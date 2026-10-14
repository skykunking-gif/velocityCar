// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProject.h"
#include "Modules/ModuleManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, MyProject, "MyProject" );

DEFINE_LOG_CATEGORY(LogMyProject)

int32 GetLocalPlayerIndex(const AController* Controller)
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;

	return LocalPlayer ? FMath::Max(LocalPlayer->GetLocalPlayerIndex(), 0) : 0;
}
