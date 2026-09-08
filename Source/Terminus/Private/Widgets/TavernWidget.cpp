// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/TavernWidget.h"

#include "Widgets/PlayerSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusUI, Log, All);

void UTavernWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (!SlotClass)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: SlotClass 가 비어 있음. BP 디폴트에서 WBP_PlayerSlot 을 지정할 것"));
		return;
	}
	
	Slots.Reset();
	SlotPanel->ClearChildren();
	
	for (int i = 0; i < MaxSlots; i++)
	{
		UPlayerSlot* SlotWidget = CreateWidget<UPlayerSlot>(this, SlotClass);
		if (!SlotWidget)
		{
			UE_LOG(LogTerminusUI, Warning, TEXT("TavernWidget: 슬롯 %d 생성 실패"), i);
			continue;
		}
		
		SlotPanel->AddChild(SlotWidget);
		Slots.Add(SlotWidget);
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimer,
			this,
			&UTavernWidget::RefreshSlots,
			0.5f,     // 주기
			true,     // 반복
			0.f);     // 첫 호출은 바로
	}
}

void UTavernWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}

	Super::NativeDestruct();
}

void UTavernWidget::RefreshSlots()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return;
	}

	const TArray<TObjectPtr<APlayerState>>& Players = GS->PlayerArray;

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		UPlayerSlot* SlotWidget = Slots[i];
		if (!SlotWidget) { continue; }

		APlayerState* PS = Players.IsValidIndex(i) ? Players[i].Get() : nullptr;
		SlotWidget->Setup(PS);
	}

	// 인원이 바뀔 때만 찍는다. 0.5초마다 찍으면 로그가 쏟아진다
	if (Players.Num() != LastPlayerCount)
	{
		LastPlayerCount = Players.Num();
		UE_LOG(LogTerminusUI, Log, TEXT("TavernWidget: 인원 %d"), LastPlayerCount);
	}
}
