// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TwoPlayerInput.h"
#include "LetterboxGameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Widgets/SViewport.h"
#include "MyProject.h"

FTwoPlayerInput::FTwoPlayerInput(UGameViewportClient* InViewportClient)
	: ViewportClient(InViewportClient)
{
}

FTwoPlayerInput* FTwoPlayerInput::Find(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const ULetterboxGameViewportClient* Viewport = World ? Cast<ULetterboxGameViewportClient>(World->GetGameViewport()) : nullptr;

	return Viewport ? Viewport->GetTwoPlayerInput() : nullptr;
}

const TArray<FKey>& FTwoPlayerInput::GetSecondPlayerKeys()
{
	// driving on the arrows, with the keys around them for everything else. The controls list on the title screen
	// and the second player's own mapping context (ATimeTrialPlayerController) follow this
	static const TArray<FKey> Keys = {
		EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right,
		EKeys::Enter, EKeys::RightShift, EKeys::RightControl, EKeys::Slash, EKeys::Delete
	};

	return Keys;
}

void FTwoPlayerInput::SetSecondPlayerDevice(ESecondPlayerDevice Device, FInputDeviceId Gamepad)
{
	SecondPlayerDevice = Device;
	SecondPlayerGamepad = Device == ESecondPlayerDevice::Gamepad ? Gamepad : FInputDeviceId();
	bWasSharedMenuUp = false;

	UE_LOG(LogMyProject, Log, TEXT("[2P] Second player plays with %s (device %d)"),
		Device == ESecondPlayerDevice::Gamepad ? TEXT("a gamepad") : (Device == ESecondPlayerDevice::Keyboard ? TEXT("the keyboard") : TEXT("nothing")),
		SecondPlayerGamepad.GetId());
}

void FTwoPlayerInput::BeginJoin(FOnJoin InOnJoin)
{
	OnJoin = MoveTemp(InOnJoin);
}

void FTwoPlayerInput::EndJoin()
{
	OnJoin = nullptr;
}

const TArray<ULocalPlayer*>* FTwoPlayerInput::GetPlayers() const
{
	const UGameViewportClient* Viewport = ViewportClient.Get();

	return Viewport && GEngine ? &GEngine->GetGamePlayers(Viewport) : nullptr;
}

bool FTwoPlayerInput::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (bDispatching)
	{
		return false;
	}

	// the first press of a gamepad's accept button or of Enter decides what the second player plays with
	if (OnJoin && !InKeyEvent.IsRepeat())
	{
		const FKey Key = InKeyEvent.GetKey();

		if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter)
		{
			const bool bGamepad = Key.IsGamepadKey();

			// the callback may well end the wait, so it's taken out first
			FOnJoin Joined = MoveTemp(OnJoin);
			OnJoin = nullptr;

			Joined(bGamepad ? ESecondPlayerDevice::Gamepad : ESecondPlayerDevice::Keyboard, bGamepad ? InKeyEvent.GetInputDeviceId() : FInputDeviceId());
			return true;
		}
	}

	const int32 Owner = FindOwner(SlateApp, InKeyEvent);
	int32 UserIndex = 0;
	FInputDeviceId Device;

	if (Owner == INDEX_NONE || !GetPlayerIdentity(SlateApp, Owner, UserIndex, Device) || static_cast<int32>(InKeyEvent.GetUserIndex()) == UserIndex)
	{
		return false;
	}

	TGuardValue<bool> Guard(bDispatching, true);

	SlateApp.ProcessKeyDownEvent(FKeyEvent(InKeyEvent.GetKey(), InKeyEvent.GetModifierKeys(), Device, InKeyEvent.IsRepeat(),
		InKeyEvent.GetCharacter(), InKeyEvent.GetKeyCode(), UserIndex));

	return true;
}

bool FTwoPlayerInput::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (bDispatching)
	{
		return false;
	}

	const int32 Owner = FindOwner(SlateApp, InKeyEvent);
	int32 UserIndex = 0;
	FInputDeviceId Device;

	if (Owner == INDEX_NONE || !GetPlayerIdentity(SlateApp, Owner, UserIndex, Device) || static_cast<int32>(InKeyEvent.GetUserIndex()) == UserIndex)
	{
		return false;
	}

	TGuardValue<bool> Guard(bDispatching, true);

	SlateApp.ProcessKeyUpEvent(FKeyEvent(InKeyEvent.GetKey(), InKeyEvent.GetModifierKeys(), Device, InKeyEvent.IsRepeat(),
		InKeyEvent.GetCharacter(), InKeyEvent.GetKeyCode(), UserIndex));

	return true;
}

bool FTwoPlayerInput::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (bDispatching)
	{
		return false;
	}

	const int32 Owner = FindOwner(SlateApp, InAnalogInputEvent);
	int32 UserIndex = 0;
	FInputDeviceId Device;

	if (Owner == INDEX_NONE || !GetPlayerIdentity(SlateApp, Owner, UserIndex, Device) || static_cast<int32>(InAnalogInputEvent.GetUserIndex()) == UserIndex)
	{
		return false;
	}

	TGuardValue<bool> Guard(bDispatching, true);

	SlateApp.ProcessAnalogInputEvent(FAnalogInputEvent(InAnalogInputEvent.GetKey(), InAnalogInputEvent.GetModifierKeys(), Device, InAnalogInputEvent.IsRepeat(),
		InAnalogInputEvent.GetCharacter(), InAnalogInputEvent.GetKeyCode(), InAnalogInputEvent.GetAnalogValue(), UserIndex));

	return true;
}

int32 FTwoPlayerInput::FindOwner(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TArray<ULocalPlayer*>* Players = GetPlayers();

	// nothing to share out with one player
	if (SecondPlayerDevice == ESecondPlayerDevice::None || !Players || Players->Num() < 2)
	{
		return INDEX_NONE;
	}

	const FKey Key = InKeyEvent.GetKey();

	// the mouse stays with the first player
	if (Key.IsMouseButton())
	{
		return INDEX_NONE;
	}

	// a menu the two share takes everybody's input. Keys held when it comes or goes are let go of, as the key's
	// release would otherwise reach a different player from its press
	const bool bSharedMenuUp = IsSharedMenuUp(SlateApp);

	if (bSharedMenuUp != bWasSharedMenuUp)
	{
		bWasSharedMenuUp = bSharedMenuUp;
		FlushPressedKeys();
	}

	if (bSharedMenuUp)
	{
		return 0;
	}

	if (SecondPlayerDevice == ESecondPlayerDevice::Gamepad)
	{
		return Key.IsGamepadKey() && InKeyEvent.GetInputDeviceId() == SecondPlayerGamepad ? 1 : 0;
	}

	return !Key.IsGamepadKey() && GetSecondPlayerKeys().Contains(Key) ? 1 : 0;
}

bool FTwoPlayerInput::IsSharedMenuUp(FSlateApplication& SlateApp) const
{
	const UGameViewportClient* Viewport = ViewportClient.Get();
	const TArray<ULocalPlayer*>* Players = GetPlayers();

	if (!Viewport || !Players || Players->Num() < 2 || !(*Players)[0] || !(*Players)[1])
	{
		return false;
	}

	const TSharedPtr<SViewport> GameViewportWidget = Viewport->GetGameViewportWidget();

	// a player has a menu up when their focus is on some widget other than the game itself
	auto HasMenuFocused = [&SlateApp, &GameViewportWidget](const ULocalPlayer* Player)
	{
		const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(Player->GetControllerId());

		return Focused.IsValid() && Focused != GameViewportWidget;
	};

	return HasMenuFocused((*Players)[0]) && !HasMenuFocused((*Players)[1]);
}

bool FTwoPlayerInput::GetPlayerIdentity(FSlateApplication& SlateApp, int32 PlayerIndex, int32& OutUserIndex, FInputDeviceId& OutDevice)
{
	const TArray<ULocalPlayer*>* Players = GetPlayers();
	const ULocalPlayer* Player = Players && Players->IsValidIndex(PlayerIndex) ? (*Players)[PlayerIndex] : nullptr;

	if (!Player)
	{
		return false;
	}

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId PlatformUser = Player->GetPlatformUserId();

	OutDevice = DeviceMapper.GetPrimaryInputDeviceForUser(PlatformUser);

	if (!OutDevice.IsValid())
	{
		if (PlayerIndex == 0)
		{
			OutDevice = DeviceMapper.GetDefaultInputDevice();

		} else {

			// a second player without a gamepad of their own, playing on the keyboard: give them a device of their
			// own so the game viewport knows their input from the first player's
			OutDevice = DeviceMapper.AllocateNewInputDeviceId();
			DeviceMapper.Internal_MapInputDeviceToUser(OutDevice, PlatformUser, EInputDeviceConnectionState::Connected);
		}
	}

	// the player's menus are focused for the Slate user of their controller id
	OutUserIndex = Player->GetControllerId();
	EnsureSlateUser(SlateApp, OutUserIndex, OutDevice);

	return true;
}

void FTwoPlayerInput::EnsureSlateUser(FSlateApplication& SlateApp, int32 UserIndex, FInputDeviceId Device)
{
	if (SlateApp.GetUser(UserIndex))
	{
		return;
	}

	// an input event is the only way in from outside: a stick at rest says nothing, but makes the user
	TGuardValue<bool> Guard(bDispatching, true);

	SlateApp.ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftX, FModifierKeysState(), Device, false, 0, 0, 0.0f, UserIndex));
}

void FTwoPlayerInput::PrepareSecondPlayer()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	int32 UserIndex = 0;
	FInputDeviceId Device;

	if (GetPlayerIdentity(SlateApp, 1, UserIndex, Device))
	{
		SlateApp.SetUserFocusToGameViewport(UserIndex);
	}
}

void FTwoPlayerInput::FlushPressedKeys() const
{
	if (const TArray<ULocalPlayer*>* Players = GetPlayers())
	{
		for (const ULocalPlayer* Player : *Players)
		{
			if (Player && Player->PlayerController)
			{
				Player->PlayerController->FlushPressedKeys();
			}
		}
	}
}
