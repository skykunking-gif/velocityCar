// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "PauseMenuUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

UPauseMenuUI::UPauseMenuUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		TitleFont = FSlateFontInfo(RobotoFontObj.Object, 56, FName("Bold Italic"));
		SmallFont = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Bold"));
	}
}

void UPauseMenuUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CreateLayout();
	}
}

void UPauseMenuUI::CreateLayout()
{
	UVerticalBox* Content = CreatePanelLayout();

	UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(CreateText(TEXT("TitleText"), TitleLabel, TitleFont, FLinearColor::White));
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	auto AddItem = [this, Content](const FText& Label, ERaceMenuAction Action, float Gap)
	{
		UVerticalBoxSlot* ItemSlot = Content->AddChildToVerticalBox(CreateMenuItem(Label, Action));
		ItemSlot->SetHorizontalAlignment(HAlign_Center);
		ItemSlot->SetPadding(FMargin(0.0f, Gap, 0.0f, 0.0f));
	};

	AddItem(ResumeLabel, ERaceMenuAction::Resume, 0.0f);
	AddItem(RetryLabel, ERaceMenuAction::Retry, 12.0f);
	AddItem(ReturnLabel, ERaceMenuAction::ReturnToTitle, 12.0f);

	UVerticalBoxSlot* HintSlot = Content->AddChildToVerticalBox(CreateText(TEXT("HintText"), HintLabel, SmallFont, FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
}

void UPauseMenuUI::OnBack()
{
	ChooseAction(ERaceMenuAction::Resume);
}
