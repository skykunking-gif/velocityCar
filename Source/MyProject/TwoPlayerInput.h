// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"

class UGameViewportClient;
class ULocalPlayer;

/** What the second player plays with in a two player game */
enum class ESecondPlayerDevice : uint8
{
	/** No second player */
	None,

	/** A gamepad of their own. The first player keeps the keyboard and any other gamepad */
	Gamepad,

	/** The right hand side of the keyboard (the arrow keys and the keys around them). The first player keeps the rest */
	Keyboard
};

/**
 *  Shares out the keyboard and the gamepads between the two players of a split screen game.
 *
 *  Sits in front of Slate as an input pre-processor. Every key, button and stick event is given to the player it
 *  belongs to by sending it on again as coming from that player: their Slate user (so it reaches the menu that player
 *  has focused) and an input device of theirs (so the game viewport hands it to their player controller).
 *
 *  While only the first player has a menu up (race select, pause, results), everybody's input goes to it, so either
 *  player can work the menus both of them share.
 *
 *  Also listens for the second player joining: the first gamepad button press or Enter decides what they play with.
 */
class FTwoPlayerInput : public IInputProcessor
{
public:

	explicit FTwoPlayerInput(UGameViewportClient* InViewportClient);

	/** Returns the input router of the game viewport the object is shown in, or null */
	static FTwoPlayerInput* Find(const UObject* WorldContextObject);

	/** Keys of the right hand side of the keyboard, which go to the second player when they play on the keyboard */
	static const TArray<FKey>& GetSecondPlayerKeys();

	/** Sets what the second player plays with. Input is only shared out while there are two local players */
	void SetSecondPlayerDevice(ESecondPlayerDevice Device, FInputDeviceId Gamepad = FInputDeviceId());

	/** Returns what the second player plays with */
	ESecondPlayerDevice GetSecondPlayerDevice() const { return SecondPlayerDevice; };

	/** Returns true when the second player plays on the keyboard */
	bool IsSecondPlayerOnKeyboard() const { return SecondPlayerDevice == ESecondPlayerDevice::Keyboard; };

	/** Called with what the second player chose to play with */
	using FOnJoin = TFunction<void(ESecondPlayerDevice Device, FInputDeviceId Gamepad)>;

	/** Waits for the second player: the next gamepad accept button picks that gamepad, the next Enter the keyboard */
	void BeginJoin(FOnJoin InOnJoin);

	/** Stops waiting for the second player */
	void EndJoin();

	/** Gets the second local player's Slate user ready, with the game viewport focused, so their menus can take focus
	 *  and their input reaches the game. Call once they've been added */
	void PrepareSecondPlayer();

	//~ Begin IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("TwoPlayerInput"); }
	//~ End IInputProcessor

private:

	/** Returns the local player the event belongs to (0 or 1), or INDEX_NONE to leave it as it is */
	int32 FindOwner(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/** Returns true while the first player has a menu focused and the second doesn't */
	bool IsSharedMenuUp(FSlateApplication& SlateApp) const;

	/** Finds the Slate user and an input device of a local player. Gives them an input device if they have none */
	bool GetPlayerIdentity(FSlateApplication& SlateApp, int32 PlayerIndex, int32& OutUserIndex, FInputDeviceId& OutDevice);

	/** Makes sure a Slate user exists. Slate only makes one when that user's first input comes in, and until then it
	 *  can't be given focus */
	void EnsureSlateUser(FSlateApplication& SlateApp, int32 UserIndex, FInputDeviceId Device);

	/** Returns the local players of the viewport, or null */
	const TArray<ULocalPlayer*>* GetPlayers() const;

	/** Lets go of every key the players hold, so nothing stays held when input changes hands */
	void FlushPressedKeys() const;

	/** The viewport whose players the input is shared between */
	TWeakObjectPtr<UGameViewportClient> ViewportClient;

	/** What the second player plays with */
	ESecondPlayerDevice SecondPlayerDevice = ESecondPlayerDevice::None;

	/** The second player's gamepad, when they play with one */
	FInputDeviceId SecondPlayerGamepad;

	/** Called when the second player joins, while waiting for them */
	FOnJoin OnJoin;

	/** True while an event is being sent on again, so it isn't handled twice */
	bool bDispatching = false;

	/** Whether a shared menu was up at the last event, to notice it coming and going */
	bool bWasSharedMenuUp = false;
};
