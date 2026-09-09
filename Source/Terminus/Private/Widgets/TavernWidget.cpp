// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/TavernWidget.h"

#include "Widgets/PlayerSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/ClassButton.h"
#include "Data/CharacterTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusUI, Log, All);

void UTavernWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 슬롯과 캐릭터 정보는 서로 무관
	CreateSlots();
	CreateClassButtons();
	ApplySelection(Selected);

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

void UTavernWidget::CreateClassButtons()
{
	if (!ClassButtonClass)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: ClassButtonClass 가 비어 있음. BP 디폴트에서 WBP_ClassButton를 지정할 것"));
		return;
	}
	
	ClassButtons.Reset();
	ClassButtonBox->ClearChildren();
	
	for (int32 i = 0; i < (int32)ECharacterClass::MAX; ++i)
	{
		// 다시 열거형으로 형변환
		const ECharacterClass ThisClass = (ECharacterClass)i;
		
		const FCharacterClassRow* Row = FindClassRow(ThisClass);
		if (!Row)
		{
			UE_LOG(LogTerminusUI, Warning, TEXT("TavernWidget: %s 행이 없어 버튼을 건너뜀"),
				*UEnum::GetValueAsString(ThisClass));
			continue;
		}

		UClassButton* Btn = CreateWidget<UClassButton>(this, ClassButtonClass);
		if (!Btn) { continue; }

		Btn->Setup(ThisClass, Row->DisplayName);
		Btn->OnClicked.BindUObject(this, &UTavernWidget::HandleClassChosen);

		ClassButtonBox->AddChild(Btn);
		ClassButtons.Add(Btn);
	}
}

void UTavernWidget::HandleClassChosen(ECharacterClass InClass)
{
	if (InClass == Selected) { return; }
	ApplySelection(InClass);
}

void UTavernWidget::CreateSlots()
{
	if (!SlotClass)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: SlotClass 가 비어 있음. BP 디폴트에서 WBP_PlayerSlot 을 지정할 것"));
		return;
	}

	// NativeConstruct 는 두 번 불릴 수 있으니 먼저 비우고 시작
	Slots.Reset();
	SlotPanel->ClearChildren();

	for (int32 i = 0; i < MaxSlots; ++i)
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
}

const FCharacterClassRow* UTavernWidget::FindClassRow(ECharacterClass InClass) const
{
	if (!CharacterClassTable)
	{
		return nullptr;
	}

	// Ctx 는 행 구조가 안 맞을 때 로그에 찍히는 이름
	static const FString Ctx(TEXT("FindClassRow"));
	TArray<FCharacterClassRow*> Rows;
	CharacterClassTable->GetAllRows<FCharacterClassRow>(Ctx, Rows);

	for (const FCharacterClassRow* Row : Rows)
	{
		if (Row && Row->Class == InClass)
		{
			return Row;
		}
	}
	return nullptr;
}

void UTavernWidget::ApplySelection(ECharacterClass InClass)
{
	Selected = InClass;

	for (UClassButton* Btn : ClassButtons)
	{
		if (Btn)
		{
			Btn->SetSelected(Btn->GetCharacterClass() == InClass);
		}
	}
	
	const FCharacterClassRow* Row = FindClassRow(InClass);
	if (!Row)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: %s 행이 없음. DT_CharacterClass 와 Character Class Table 지정을 확인할 것"),
			*UEnum::GetValueAsString(InClass));
		return;
	}

	if (ClassTitleText)
	{
		ClassTitleText->SetText(Row->DisplayName);
	}
	DescriptionText->SetText(Row->Description);
	PassiveText->SetText(Row->PassiveText);

	if (UTexture2D* Tex = Row->Illustration.LoadSynchronous())
	{
		Illustration->SetBrushFromTexture(Tex);
		Illustration->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// Hidden 으로 자리를 남겨야 옆의 설명 패널이 안 밀린다
		Illustration->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTavernWidget::RefreshSlots()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		// 접속 직후엔 아직 없음
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

	// 인원이 바뀔 때 알려주는 로그
	if (Players.Num() != LastPlayerCount)
	{
		LastPlayerCount = Players.Num();
		UE_LOG(LogTerminusUI, Log, TEXT("TavernWidget: 인원 %d"), LastPlayerCount);
	}
}
