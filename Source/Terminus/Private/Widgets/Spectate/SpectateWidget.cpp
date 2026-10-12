#include "Widgets/Spectate/SpectateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dungeon/DungeonArea.h"
#include "Dungeon/DungeonAreaSubsystem.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "TimerManager.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/Common/TerminusUIText.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/Common/ChoiceCardWidget.h"
#include "Widgets/TerminusUISettings.h"

namespace
{
	FString SpectateAreaLabel(const ADungeonArea* Area)
	{
		TArray<FString> Names;
		for (const ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS) Names.Add(PS->GetPlayerName());
		}
		return FString::Printf(TEXT("%s (%s)"), *FString::Join(Names, TEXT(", ")), *TerminusUI::RoomTypeLabel(Area->GetRoom().Type));
	}
}

void USpectateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !ButtonBox)
	{
		BuildDefaultLayout();
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void USpectateWidget::BuildDefaultLayout()
{
	// 화면 위쪽 가운데 작은 바. 바깥은 클릭이 통과
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>();
	Bar->SetBrushColor(TerminusUI::Color::Dim);
	Bar->SetPadding(FMargin(12.f, 6.f));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Bar))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 70.f, 0.f, 0.f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Bar->SetContent(Row);

	LabelText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("관전"), 15, TerminusUI::Color::Gold);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->AddChildToHorizontalBox(ButtonBox);
}

void USpectateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshHandle, this, &USpectateWidget::Refresh, 0.3f, true);
	}
}

void USpectateWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshHandle);
	}
	Super::NativeDestruct();
}

void USpectateWidget::Refresh()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const UDungeonAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDungeonAreaSubsystem>() : nullptr;
	if (!PC || !PS || !Areas)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ADungeonArea* MyArea = PS->GetCurrentArea();
	ADungeonArea* Viewed = PC->GetViewedArea();

	// 내가 아직 싸우는 중인가 (살아서 턴이 도는 전투)
	const UDungeonCombatComponent* MyCombat = MyArea ? MyArea->GetCombat() : nullptr;
	const ECombatPhase MyPhase = MyCombat ? MyCombat->GetPhase() : ECombatPhase::None;
	const ATerminusBattler* MyBattler = Cast<ATerminusBattler>(PS->GetPawn());
	const UCombatStatsComponent* MyStats = MyBattler ? MyBattler->GetCombatStats() : nullptr;
	const bool bMeAlive = MyStats && !MyStats->IsDead();
	const bool bFighting = MyArea && !MyArea->IsCleared() && bMeAlive
		&& (MyPhase == ECombatPhase::PlayerTurn || MyPhase == ECombatPhase::MonsterTurn);

	// 볼 수 있는 다른 구역 (진행 중인 구역 중 내 것 빼고)
	TArray<ADungeonArea*> Others;
	for (ADungeonArea* Area : Areas->GetActiveAreas())
	{
		if (Area && Area != MyArea) Others.Add(Area);
	}

	const bool bCanSpectate = !bFighting && Others.Num() > 0;

	// 보던 구역이 끝났거나 내가 다시 싸워야 하면 내 구역으로
	if (Viewed && Viewed != MyArea && (!bCanSpectate || !Others.Contains(Viewed)))
	{
		PC->ViewDungeonArea(MyArea);
		Viewed = MyArea;
	}

	if (!bCanSpectate)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		ShownKey.Reset();
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 구역 구성이나 보는 구역이 바뀌었을 때만 다시 그림 (누르는 도중에 버튼이 바뀌지 않게)
	FString Key = FString::Printf(TEXT("%p|%p|"), MyArea, Viewed);
	for (const ADungeonArea* Area : Others)
	{
		Key += FString::Printf(TEXT("%p:%d;"), Area, Area->GetOccupants().Num());
	}
	if (Key != ShownKey)
	{
		ShownKey = Key;
		RebuildButtons(MyArea, Others, Viewed);
	}
}

void USpectateWidget::RebuildButtons(ADungeonArea* MyArea, const TArray<ADungeonArea*>& Others, ADungeonArea* Viewed)
{
	if (!ButtonBox) return;

	ButtonBox->ClearChildren();

	auto AddButton = [this, Viewed](ADungeonArea* Area, bool bMine, const FString& Label)
	{
		UChoiceCardWidget* Card = CreateWidget<UChoiceCardWidget>(this, UTerminusUISettings::Pick(ButtonClass, &UTerminusUISettings::ChoiceCard));
		if (!Card) return;

		FChoiceCardStyle CardStyle;
		CardStyle.TitleSize = 14;
		CardStyle.Padding = FMargin(8.f, 2.f);
		CardStyle.NormalColor = FLinearColor(0.2f, 0.2f, 0.24f);
		Card->ApplyDefaultStyle(CardStyle);

		Card->SetChoiceId(bMine ? FName(TEXT("Mine")) : FName(TEXT("Other")));
		Card->SetTexts(Label, FString());
		Card->SetState(Viewed == Area ? EChoiceCardState::Selected : EChoiceCardState::Normal);
		Card->OnChosen = FSimpleDelegate::CreateWeakLambda(this, [this, Area = TWeakObjectPtr<ADungeonArea>(Area), bMine]() { HandleAreaClicked(Area.Get(), bMine); });

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(ButtonBox->AddChild(Card)))
		{
			S->SetPadding(FMargin(4.f, 0.f));
		}
	};

	AddButton(MyArea, true, MyArea ? TEXT("  내 방  ") : TEXT("  지도  "));
	for (ADungeonArea* Area : Others)
	{
		AddButton(Area, false, FString::Printf(TEXT("  %s  "), *SpectateAreaLabel(Area)));
	}
}

void USpectateWidget::HandleAreaClicked(ADungeonArea* Area, bool bMine)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PC || !PS) return;

	PC->ViewDungeonArea(bMine ? PS->GetCurrentArea() : Area);
	ShownKey.Reset();
	Refresh();
}
