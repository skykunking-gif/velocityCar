// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateEnums.h"
#include "TachometerStyle.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 *  One text readout on a tachometer, such as the speed or the gear
 */
USTRUCT(BlueprintType)
struct FTachometerReadout
{
	GENERATED_BODY()

	/** If false, the readout isn't shown. Turn it off when the dial artwork already has it printed on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	bool bVisible = true;

	/** Where the readout goes, in dial texture pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FVector2D Position = FVector2D::ZeroVector;

	/** Which point of the text sits on Position: (0,0) is its top left corner, (1,1) its bottom right */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FVector2D Alignment = FVector2D(0.5f, 0.5f);

	/** Font, sized for the dial texture's pixels. Scale shrinks it along with the artwork. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FSlateFontInfo Font;

	/** Text color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FLinearColor Color = FLinearColor::White;

	/** If above zero, every character gets a cell this wide (dial texture pixels) instead of its own advance, like
	 *  the fixed digit positions of a seven-segment display, so a narrow 1 doesn't pull the other digits closer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout", meta = (ClampMin = "0"))
	float CharacterCellWidth = 0.0f;

	/** Where a character sits in its cell. Right matches a seven-segment 1, which lights the right-hand segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout", meta = (EditCondition = "CharacterCellWidth > 0"))
	TEnumAsByte<EHorizontalAlignment> CellAlignment = HAlign_Right;
};

/**
 *  Everything that makes up one tachometer design: the dial and needle artwork, how the needle
 *  maps engine speed to an angle, an optional bar that lights up from the left with the engine speed,
 *  and where the speed and gear readouts sit on the dial.
 *  Make one asset per design and hand it to the vehicle HUD.
 */
UCLASS(BlueprintType)
class UTachometerStyle : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Dial artwork */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dial")
	TObjectPtr<UTexture2D> DialTexture;

	/** Dial artwork shown while the player drives with the manual gearbox, such as one marked M where DialTexture
	 *  is marked A. The same size as DialTexture. Leave empty to show DialTexture with either gearbox */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dial")
	TObjectPtr<UTexture2D> ManualDialTexture;

	/** Size the tachometer is drawn at relative to its textures. 0.5 draws @2x artwork at its 1080p size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dial", meta = (ClampMin = "0.01"))
	float Scale = 0.5f;

	/** Where the needle turns on the dial, in dial texture pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needle")
	FVector2D DialHub = FVector2D::ZeroVector;

	/** Needle artwork, pointing straight up in the texture and made at the same scale as the dial */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needle")
	TObjectPtr<UTexture2D> NeedleTexture;

	/** Size the needle is drawn at relative to the dial, for a needle drawn at another scale, such as an @2x needle on
	 *  an @1x dial */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needle", meta = (ClampMin = "0.01"))
	float NeedleScale = 1.0f;

	/** The point of the needle that sits on DialHub and turns, in needle texture pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needle")
	FVector2D NeedlePivot = FVector2D::ZeroVector;

	/** Needle angle for engine speeds, clockwise from straight up in degrees. X is the RPM and Y the angle.
	 *  Points must be in ascending RPM order; speeds between them are interpolated and speeds past the ends clamp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Needle")
	TArray<FVector2D> NeedleAngles;

	/** Bar artwork drawn over the dial, fully lit. The part past the engine speed is hidden. Leave empty for no bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	TObjectPtr<UTexture2D> BarTexture;

	/** Draws the bar and hides its unlit part. Gets the bar texture as "Bar", and as fractions of the texture width
	 *  the lit width along its bottom edge as "Fill" and how far the edge leans right from bottom to top as "Slant" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	TObjectPtr<UMaterialInterface> BarMaterial;

	/** Where the bar texture's top left corner goes, in dial texture pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	FVector2D BarPosition = FVector2D::ZeroVector;

	/** Number of segments the bar lights up in, one at a time. Zero lights it smoothly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar", meta = (ClampMin = "0"))
	int32 BarSegments = 0;

	/** Engine speed each segment stands for. The first segment is lit from 0 RPM, the next from this, and so on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar", meta = (ClampMin = "1"))
	float BarRPMPerSegment = 1000.0f;

	/** Where the lit part ends with nothing lit, measured along the bar texture's bottom edge in its pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	float BarStartX = 0.0f;

	/** How far the end of the lit part moves for each segment, in bar texture pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	float BarSegmentWidth = 0.0f;

	/** How many pixels the end of the lit part leans right for each pixel up, so it runs between slanted segments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bar")
	float BarSlant = 0.0f;

	/** Vehicle speed, in the HUD's unit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readouts")
	FTachometerReadout Speed;

	/** The speed is padded with leading zeros to at least this many digits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readouts", meta = (ClampMin = "1"))
	int32 SpeedMinDigits = 3;

	/** Current gear: R, N or the gear number */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readouts")
	FTachometerReadout Gear;

	/** Name of the speed unit, km/h or MPH */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readouts")
	FTachometerReadout Unit;

	/** Dial artwork for the automatic or the manual gearbox */
	UTexture2D* GetDialTexture(bool bAutomatic) const { return !bAutomatic && ManualDialTexture ? ManualDialTexture.Get() : DialTexture.Get(); }

	/** Needle angle for an engine speed, looked up in NeedleAngles */
	float GetNeedleAngle(float RPM) const;

	/** Where the lit part of the bar ends for an engine speed, along the bar texture's bottom edge in its pixels */
	float GetBarFillX(float RPM) const;
};
