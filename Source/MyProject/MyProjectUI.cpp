// Copyright Epic Games, Inc. All Rights Reserved.


#include "MyProjectUI.h"
#include "TachometerStyle.h"
#include "DrivingSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MyProject.h"

namespace
{
	/** Constructs a widget whose name can't clash with the parts of a tachometer built earlier */
	template <typename WidgetT>
	WidgetT* ConstructUniqueWidget(UWidgetTree* WidgetTree, FName BaseName)
	{
		return WidgetTree->ConstructWidget<WidgetT>(WidgetT::StaticClass(), MakeUniqueObjectName(WidgetTree, WidgetT::StaticClass(), BaseName));
	}

	/** Size of a texture in pixels */
	FVector2D GetTextureSize(const UTexture2D* Texture)
	{
		return FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
	}

	/** Makes an image widget that draws a texture at its own size */
	UImage* CreateTextureImage(UWidgetTree* WidgetTree, FName Name, UTexture2D* Texture)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = GetTextureSize(Texture);

		UImage* Image = ConstructUniqueWidget<UImage>(WidgetTree, Name);
		Image->SetBrush(Brush);

		return Image;
	}
}

void UMyProjectUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CreateTachometer(TachometerStyle);

	DrivingSettingsChangedHandle = UDrivingSettings::OnChanged().AddUObject(this, &UMyProjectUI::RefreshDialTexture);
}

void UMyProjectUI::NativeDestruct()
{
	UDrivingSettings::OnChanged().Remove(DrivingSettingsChangedHandle);

	Super::NativeDestruct();
}

void UMyProjectUI::RefreshDialTexture()
{
	if (DialImage && ShownTachometerStyle)
	{
		DialImage->SetBrushFromTexture(ShownTachometerStyle->GetDialTexture(UDrivingSettings::IsAutomaticTransmission(GetLocalPlayerIndex(GetOwningPlayer()))), true);
	}
}

void UMyProjectUI::SetTachometerStyle(UTachometerStyle* NewStyle)
{
	UTachometerStyle* Style = NewStyle ? NewStyle : TachometerStyle.Get();

	if (Style == ShownTachometerStyle)
	{
		return;
	}

	// throw the current tachometer away and build the new design in its place
	if (TachometerWidget)
	{
		TachometerWidget->RemoveFromParent();
	}

	TachometerWidget = nullptr;
	ShownTachometerStyle = nullptr;
	DialImage = nullptr;
	NeedleImage = nullptr;
	BarMaterialInstance = nullptr;
	SpeedReadout = nullptr;
	GearReadout = nullptr;

	CreateTachometer(Style);
}

void UMyProjectUI::CreateTachometer(UTachometerStyle* Style)
{
	if (!Style || !Style->DialTexture || !WidgetTree)
	{
		return;
	}

	UPanelWidget* Root = Cast<UPanelWidget>(WidgetTree->RootWidget);

	if (!Root)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[UI] %s: the root widget isn't a panel, so the tachometer can't be added."), *GetName());
		return;
	}

	// the tachometer replaces the designer's readouts
	for (UWidget* Child : Root->GetAllChildren())
	{
		Child->SetVisibility(ESlateVisibility::Collapsed);
	}

	// everything inside is laid out in dial texture pixels, and the scale box shrinks it to its on-screen size
	UScaleBox* Tachometer = ConstructUniqueWidget<UScaleBox>(WidgetTree, TEXT("Tachometer"));
	Tachometer->SetStretch(EStretch::UserSpecified);
	Tachometer->SetUserSpecifiedScale(Style->Scale);
	Tachometer->SetVisibility(ESlateVisibility::HitTestInvisible);

	const FVector2D DialSize = GetTextureSize(Style->DialTexture);

	USizeBox* DialBox = ConstructUniqueWidget<USizeBox>(WidgetTree, TEXT("TachometerSize"));
	DialBox->SetWidthOverride(static_cast<float>(DialSize.X));
	DialBox->SetHeightOverride(static_cast<float>(DialSize.Y));
	Tachometer->AddChild(DialBox);

	UCanvasPanel* Canvas = ConstructUniqueWidget<UCanvasPanel>(WidgetTree, TEXT("TachometerCanvas"));
	DialBox->AddChild(Canvas);

	DialImage = CreateTextureImage(WidgetTree, TEXT("TachometerDial"), Style->GetDialTexture(UDrivingSettings::IsAutomaticTransmission(GetLocalPlayerIndex(GetOwningPlayer()))));
	Canvas->AddChildToCanvas(DialImage)->SetAutoSize(true);

	if (Style->BarTexture && Style->BarMaterial)
	{
		// the material hides the bar past the engine speed. It takes the slant in texture widths per texture height
		const FVector2D BarSize = GetTextureSize(Style->BarTexture);

		BarMaterialInstance = UMaterialInstanceDynamic::Create(Style->BarMaterial, this);
		BarMaterialInstance->SetTextureParameterValue(TEXT("Bar"), Style->BarTexture);
		BarMaterialInstance->SetScalarParameterValue(TEXT("Slant"), static_cast<float>(Style->BarSlant * BarSize.Y / BarSize.X));

		FSlateBrush Brush;
		Brush.SetResourceObject(BarMaterialInstance);
		Brush.ImageSize = BarSize;

		UImage* BarImage = ConstructUniqueWidget<UImage>(WidgetTree, TEXT("TachometerBar"));
		BarImage->SetBrush(Brush);

		UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(BarImage);
		BarSlot->SetAutoSize(true);
		BarSlot->SetPosition(Style->BarPosition);
	}

	if (Style->NeedleTexture)
	{
		// the needle's pivot sits on the hub, and the needle turns around that same point
		const FVector2D PivotFraction = Style->NeedlePivot / GetTextureSize(Style->NeedleTexture);

		NeedleImage = CreateTextureImage(WidgetTree, TEXT("TachometerNeedle"), Style->NeedleTexture);

		// drawn at the needle's own scale on the dial
		FSlateBrush NeedleBrush = NeedleImage->GetBrush();
		NeedleBrush.ImageSize *= Style->NeedleScale;
		NeedleImage->SetBrush(NeedleBrush);

		NeedleImage->SetRenderTransformPivot(PivotFraction);
		NeedleImage->SetRenderTransformAngle(Style->GetNeedleAngle(EngineRPM));

		UCanvasPanelSlot* NeedleSlot = Canvas->AddChildToCanvas(NeedleImage);
		NeedleSlot->SetAutoSize(true);
		NeedleSlot->SetPosition(Style->DialHub);
		NeedleSlot->SetAlignment(PivotFraction);
	}

	SpeedReadout = CreateTachometerReadout(Canvas, TEXT("TachometerSpeed"), Style->Speed);
	GearReadout = CreateTachometerReadout(Canvas, TEXT("TachometerGear"), Style->Gear);

	if (UWidget* UnitReadout = CreateTachometerReadout(Canvas, TEXT("TachometerUnit"), Style->Unit))
	{
		SetReadoutText(UnitReadout, Style->Unit, GetSpeedUnitText());
	}

	ShownTachometerStyle = Style;
	TachometerWidget = Tachometer;

	RefreshSpeedText();
	RefreshBarFill();

	if (GearReadout)
	{
		SetReadoutText(GearReadout, Style->Gear, GetGearText());
	}

	UPanelSlot* PanelSlot = Root->AddChild(Tachometer);

	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Right);
		OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		OverlaySlot->SetPadding(FMargin(0.0f, 0.0f, TachometerCornerOffset.X, TachometerCornerOffset.Y));

	} else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot)) {

		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		CanvasSlot->SetPosition(-TachometerCornerOffset);

	} else {

		UE_LOG(LogMyProject, Warning, TEXT("[UI] %s: the root widget is a %s, which can't pin the tachometer to a corner. Use a Canvas Panel or Overlay as the root."),
			*GetName(), *Root->GetClass()->GetName());
	}
}

UWidget* UMyProjectUI::CreateTachometerReadout(UCanvasPanel* Canvas, FName Name, const FTachometerReadout& Readout)
{
	if (!Readout.bVisible)
	{
		return nullptr;
	}

	// a cell row starts empty; SetReadoutText adds a cell per character
	UWidget* Widget = Readout.CharacterCellWidth > 0.0f
		? static_cast<UWidget*>(ConstructUniqueWidget<UHorizontalBox>(WidgetTree, Name))
		: CreateReadoutText(Name, Readout);

	UCanvasPanelSlot* ReadoutSlot = Canvas->AddChildToCanvas(Widget);
	ReadoutSlot->SetAutoSize(true);
	ReadoutSlot->SetPosition(Readout.Position);
	ReadoutSlot->SetAlignment(Readout.Alignment);

	return Widget;
}

UTextBlock* UMyProjectUI::CreateReadoutText(FName Name, const FTachometerReadout& Readout)
{
	UTextBlock* Text = ConstructUniqueWidget<UTextBlock>(WidgetTree, Name);
	Text->SetColorAndOpacity(Readout.Color);

	if (Readout.Font.HasValidFont())
	{
		Text->SetFont(Readout.Font);
	}

	return Text;
}

void UMyProjectUI::SetReadoutText(UWidget* ReadoutWidget, const FTachometerReadout& Readout, const FText& Text)
{
	if (UTextBlock* TextBlock = Cast<UTextBlock>(ReadoutWidget))
	{
		TextBlock->SetText(Text);
		return;
	}

	UHorizontalBox* Cells = Cast<UHorizontalBox>(ReadoutWidget);

	if (!Cells)
	{
		return;
	}

	const FString Characters = Text.ToString();

	// one fixed-width cell per character
	while (Cells->GetChildrenCount() < Characters.Len())
	{
		USizeBox* Cell = ConstructUniqueWidget<USizeBox>(WidgetTree, TEXT("ReadoutCell"));
		Cell->SetWidthOverride(Readout.CharacterCellWidth);

		USizeBoxSlot* CharacterSlot = Cast<USizeBoxSlot>(Cell->AddChild(CreateReadoutText(TEXT("ReadoutCharacter"), Readout)));
		CharacterSlot->SetHorizontalAlignment(Readout.CellAlignment);
		CharacterSlot->SetVerticalAlignment(VAlign_Center);

		Cells->AddChildToHorizontalBox(Cell);
	}

	while (Cells->GetChildrenCount() > Characters.Len())
	{
		Cells->RemoveChildAt(Cells->GetChildrenCount() - 1);
	}

	for (int32 Index = 0; Index < Characters.Len(); ++Index)
	{
		const USizeBox* Cell = CastChecked<USizeBox>(Cells->GetChildAt(Index));
		CastChecked<UTextBlock>(Cell->GetContent())->SetText(FText::FromString(Characters.Mid(Index, 1)));
	}
}

void UMyProjectUI::RefreshSpeedText()
{
	if (!SpeedReadout)
	{
		return;
	}

	const FNumberFormattingOptions Options = FNumberFormattingOptions()
		.SetUseGrouping(false)
		.SetMinimumIntegralDigits(ShownTachometerStyle->SpeedMinDigits);

	SetReadoutText(SpeedReadout, ShownTachometerStyle->Speed, FText::AsNumber(FMath::RoundToInt(DisplaySpeed), &Options));
}

void UMyProjectUI::UpdateSpeed(float NewSpeed)
{
	// format the speed to KPH or MPH
	DisplaySpeed = FMath::Abs(NewSpeed) * (bIsMPH ? 0.022f : 0.036f);

	RefreshSpeedText();

	// call the Blueprint handler
	OnSpeedUpdate(DisplaySpeed);
}

void UMyProjectUI::UpdateGear(int32 NewGear)
{
	CurrentGear = NewGear;

	if (GearReadout)
	{
		SetReadoutText(GearReadout, ShownTachometerStyle->Gear, GetGearText());
	}

	// call the Blueprint handler
	OnGearUpdate(NewGear);
}

void UMyProjectUI::UpdateEngineRPM(float NewRPM, float NewMaxRPM)
{
	EngineRPM = FMath::Abs(NewRPM);
	MaxEngineRPM = NewMaxRPM;

	if (NeedleImage)
	{
		NeedleImage->SetRenderTransformAngle(ShownTachometerStyle->GetNeedleAngle(EngineRPM));
	}

	RefreshBarFill();

	// call the Blueprint handler
	OnEngineRPMUpdate(EngineRPM);
}

void UMyProjectUI::RefreshBarFill()
{
	if (!BarMaterialInstance || !ShownTachometerStyle || !ShownTachometerStyle->BarTexture)
	{
		return;
	}

	// the material takes the lit width as a fraction of the texture width
	const float BarWidth = FMath::Max(static_cast<float>(ShownTachometerStyle->BarTexture->GetSurfaceWidth()), 1.0f);

	BarMaterialInstance->SetScalarParameterValue(TEXT("Fill"), ShownTachometerStyle->GetBarFillX(EngineRPM) / BarWidth);
}

FText UMyProjectUI::GetSpeedUnitText() const
{
	return bIsMPH ? NSLOCTEXT("VehicleUI", "SpeedUnitMPH", "MPH") : NSLOCTEXT("VehicleUI", "SpeedUnitKPH", "km/h");
}

FText UMyProjectUI::GetGearText() const
{
	if (CurrentGear < 0)
	{
		return NSLOCTEXT("VehicleUI", "GearReverse", "R");
	}

	if (CurrentGear == 0)
	{
		return NSLOCTEXT("VehicleUI", "GearNeutral", "N");
	}

	return FText::AsNumber(CurrentGear);
}

float UMyProjectUI::GetEngineRPMFraction() const
{
	if (MaxEngineRPM <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(EngineRPM / MaxEngineRPM, 0.0f, 1.0f);
}
