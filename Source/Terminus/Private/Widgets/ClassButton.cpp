// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ClassButton.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UClassButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (ClickArea)
	{
		ClickArea->OnClicked.AddDynamic(this, &UClassButton::HandleClicked);
	}
}

void UClassButton::HandleClicked()
{
	OnClicked.ExecuteIfBound(MyClass);
}

void UClassButton::Setup(ECharacterClass InClass, const FText& Label)
{
	MyClass = InClass;
	LabelText->SetText(Label);
	SetSelected(false);
}

void UClassButton::SetSelected(bool bSelected)
{
	if (SelectedMark)
	{
		SelectedMark->SetVisibility(
			bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
