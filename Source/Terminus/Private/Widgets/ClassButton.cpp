// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/ClassButton.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

namespace
{
	// 1d 팔레트. 선택되면 금색, 아니면 보조 글자색
	const FLinearColor LabelOn  = FLinearColor(FColor::FromHex(TEXT("E6C47A")));
	const FLinearColor LabelOff = FLinearColor(FColor::FromHex(TEXT("A79F8E")));
}

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
	
	LabelText->SetColorAndOpacity(FSlateColor(bSelected ? LabelOn : LabelOff));
}
