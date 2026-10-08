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

namespace
{
	UTextBlock* MakeSpectateText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	FString SpectateRoomTypeName(ERoomType Type)
	{
		switch (Type)
		{
		case ERoomType::MONSTER:  return TEXT("몬스터");
		case ERoomType::GUARDIAN: return TEXT("가디언");
		case ERoomType::BOSS:     return TEXT("보스");
		case ERoomType::BREAK:    return TEXT("휴식터");
		case ERoomType::EVENT:    return TEXT("이벤트");
		case ERoomType::STORE:    return TEXT("상점");
		case ERoomType::QUEST:    return TEXT("퀘스트");
		default:                  return TEXT("방");
		}
	}

	FString SpectateAreaLabel(const ADungeonArea* Area)
	{
		TArray<FString> Names;
		for (const ATerminusPlayerState* PS : Area->GetOccupants())
		{
			if (PS) Names.Add(PS->GetPlayerName());
		}
		return FString::Printf(TEXT("%s (%s)"), *FString::Join(Names, TEXT(", ")), *SpectateRoomTypeName(Area->GetRoom().Type));
	}
}

void USpectateClickRelay::HandleClicked()
{
	if (USpectateWidget* Widget = Owner.Get())
	{
		Widget->HandleAreaClicked(Area.Get(), bMine);
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
	Bar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Bar->SetPadding(FMargin(12.f, 6.f));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Bar))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 70.f, 0.f, 0.f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Bar->SetContent(Row);

	LabelText = MakeSpectateText(WidgetTree, TEXT("관전"), 15, FLinearColor(1.f, 0.82f, 0.35f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->AddChildToHorizontalBox(ButtonBox);
}

void USpectateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshTimer += InDeltaTime;
	if (RefreshTimer >= 0.3f)
	{
		RefreshTimer = 0.f;
		Refresh();
	}
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
	Relays.Reset();

	auto AddButton = [this, Viewed](ADungeonArea* Area, bool bMine, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		const bool bViewing = bMine ? (Viewed == Area) : (Viewed == Area);
		Button->SetBackgroundColor(bViewing ? FLinearColor(0.25f, 0.5f, 0.3f) : FLinearColor(0.2f, 0.2f, 0.24f));
		Button->SetContent(MakeSpectateText(WidgetTree, Label, 14, FLinearColor::White));

		USpectateClickRelay* Relay = NewObject<USpectateClickRelay>(this);
		Relay->Owner = this;
		Relay->Area = Area;
		Relay->bMine = bMine;
		Button->OnClicked.AddDynamic(Relay, &USpectateClickRelay::HandleClicked);
		Relays.Add(Relay);

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(ButtonBox->AddChild(Button)))
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
