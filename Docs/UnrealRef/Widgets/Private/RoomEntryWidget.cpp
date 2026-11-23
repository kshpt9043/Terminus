// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/RoomEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void URoomEntryWidget::Setup(const FTerminusSessionInfo& InInfo)
{
	Info = InInfo;
	
	RoomNameText->SetText(FText::FromString(Info.RoomName));
	PlayersText->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), Info.CurrentPlayers, Info.MaxPlayers)));
	
	if (PingText)
	{
		PingText->SetText(FText::FromString(FString::Printf(TEXT("%dms"), Info.PingMs)));
	}
	if (LockMark)
	{
		LockMark->SetVisibility(Info.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	
	JoinButton->SetIsEnabled(Info.CurrentPlayers < Info.MaxPlayers);
}

void URoomEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	JoinButton->OnClicked.AddDynamic(this, &URoomEntryWidget::HandleClicked);
}

void URoomEntryWidget::HandleClicked()
{
	OnClicked.ExecuteIfBound(Info);
}
