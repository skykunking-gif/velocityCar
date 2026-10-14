// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyProjectUI.generated.h"

class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class UWidget;
class UTachometerStyle;
struct FTachometerReadout;

/**
 *  Simple Vehicle HUD class
 *  Displays the current speed and gear.
 *  Widget setup is handled in a Blueprint subclass.
 *  If TachometerStyle is set, a tachometer is built in code in the bottom right corner instead: a dial with a
 *  needle or a bar that follows the engine speed, and its own speed and gear readouts. The designer's widgets are hidden
 *  while it's shown, since they would only repeat those readouts.
 */
UCLASS(abstract)
class UMyProjectUI : public UUserWidget
{
	GENERATED_BODY()
	
protected:

	/** Controls the display of speed in Km/h or MPH */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle")
	bool bIsMPH = false;

	/** Design of the generated tachometer. Leave empty to keep the designer's widgets. SetTachometerStyle can swap it at runtime. */
	UPROPERTY(EditAnywhere, Category="Tachometer")
	TObjectPtr<UTachometerStyle> TachometerStyle;

	/** Distance of the tachometer from the bottom right corner of the screen */
	UPROPERTY(EditAnywhere, Category="Tachometer")
	FVector2D TachometerCornerOffset = FVector2D(16.0f, 16.0f);

	/** Design the tachometer on screen was built from. Null if none was built. */
	UPROPERTY(Transient)
	TObjectPtr<UTachometerStyle> ShownTachometerStyle;

	/** Outermost widget of the generated tachometer, removed when the design is swapped */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> TachometerWidget;

	/** Dial of the generated tachometer, which shows the automatic or the manual gearbox's artwork. Null if there's no tachometer */
	UPROPERTY(Transient)
	TObjectPtr<UImage> DialImage;

	/** Needle of the generated tachometer. Null if there's no tachometer or it has no needle. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> NeedleImage;

	/** Listens for the gearbox being changed, to show the dial that goes with it */
	FDelegateHandle DrivingSettingsChangedHandle;

	/** Material drawing the bar of the generated tachometer. Null if there's no tachometer or it has no bar. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarMaterialInstance;

	/** Speed readout of the generated tachometer: a text, or a row of character cells. Null if there's no tachometer or its style hides the speed. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SpeedReadout;

	/** Gear readout of the generated tachometer: a text, or a row of character cells. Null if there's no tachometer or its style hides the gear. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GearReadout;

	/** Speed in the display unit, as of the last update */
	float DisplaySpeed = 0.0f;

	/** Gear as of the last update. Zero is neutral, negative is reverse */
	int32 CurrentGear = 0;

	/** Engine speed in RPM, as of the last update */
	float EngineRPM = 0.0f;

	/** Engine speed a full tachometer reads, in RPM */
	float MaxEngineRPM = 0.0f;

public:

	/** Called to update the speed display */
	void UpdateSpeed(float NewSpeed);

	/** Called to update the gear display */
	void UpdateGear(int32 NewGear);

	/** Called to update the tachometer. NewMaxRPM sets what a full gauge reads */
	void UpdateEngineRPM(float NewRPM, float NewMaxRPM);

	/** Rebuilds the tachometer with another design, e.g. the one that goes with the car being driven.
	 *  Null goes back to TachometerStyle. */
	UFUNCTION(BlueprintCallable, Category="Tachometer")
	void SetTachometerStyle(UTachometerStyle* NewStyle);

protected:

	/** Builds the tachometer if TachometerStyle is set */
	virtual void NativeOnInitialized() override;

	/** Stops listening for the gearbox being changed */
	virtual void NativeDestruct() override;

	/** Shows the dial artwork for the gearbox the player drives with */
	void RefreshDialTexture();

	/** Builds a tachometer of the given design in the bottom right corner and hides the designer's widgets */
	void CreateTachometer(UTachometerStyle* Style);

	/** Adds one of the tachometer's readouts to its canvas: a text, or a row of fixed-width character cells if the
	 *  readout asks for them. Returns null if the style hides it. */
	UWidget* CreateTachometerReadout(UCanvasPanel* Canvas, FName Name, const FTachometerReadout& Readout);

	/** Makes a text widget styled for a readout */
	UTextBlock* CreateReadoutText(FName Name, const FTachometerReadout& Readout);

	/** Shows Text on a readout made by CreateTachometerReadout, one character per cell for a cell row */
	void SetReadoutText(UWidget* ReadoutWidget, const FTachometerReadout& Readout, const FText& Text);

	/** Writes the current speed into the tachometer's speed readout */
	void RefreshSpeedText();

	/** Lights the tachometer's bar up to the current engine speed */
	void RefreshBarFill();

	/** Implemented in Blueprint to display the new speed */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnSpeedUpdate(float NewSpeed);

	/** Implemented in Blueprint to display the new gear */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnGearUpdate(int32 NewGear);

	/** Implemented in Blueprint to display the new engine speed */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnEngineRPMUpdate(float NewRPM);

public:

	/** Speed in whichever unit the HUD is set to display */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetDisplaySpeed() const { return DisplaySpeed; };

	/** Name of the unit the speed readout is in, ready to sit next to the number */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	FText GetSpeedUnitText() const;

	/** Current gear. Zero is neutral, negative is reverse */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	int32 GetCurrentGear() const { return CurrentGear; };

	/** Current gear as it should read on the HUD: R for reverse, N for neutral, otherwise the gear number */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	FText GetGearText() const;

	/** Engine speed in RPM */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetEngineRPM() const { return EngineRPM; };

	/** Engine speed a full tachometer reads, in RPM */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetMaxEngineRPM() const { return MaxEngineRPM; };

	/** Engine speed as a 0-1 fraction of the maximum, for driving a gauge sweep or a bar */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetEngineRPMFraction() const;
};
