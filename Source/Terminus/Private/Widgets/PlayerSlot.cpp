// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/PlayerSlot.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UPlayerSlot::NativeConstruct()
{
	Super::NativeConstruct();

	Clear();
}

void UPlayerSlot::Setup(APlayerState* PS, const FText& ClassName, bool bReady)
{
	if (!PS)
	{
		Clear();
		return;
	}

	FString Name = PS->GetPlayerName();
	if (GetOwningPlayerState() == PS)
	{
		// 닉네임이 겹칠 수 있으니 본인 자리는 표시해둔다
		Name += TEXT(" (나)");
	}
	PlayerNameText->SetText(FText::FromString(Name));
	ClassNameText->SetText(ClassName);
	// 준비 상태 -> ReadyMark 켜고 끄기
	ReadyMark->SetVisibility(bReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	EmptyMark->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerSlot::Clear()
{
	PlayerNameText->SetText(FText::GetEmpty());
	ClassNameText->SetText(FText::GetEmpty());
	ReadyMark->SetVisibility(ESlateVisibility::Collapsed);
	EmptyMark->SetVisibility(ESlateVisibility::Visible);
}
