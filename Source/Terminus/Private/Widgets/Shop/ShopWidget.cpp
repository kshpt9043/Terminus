#include "Widgets/Shop/ShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/ItemPickPanelWidget.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/Common/TerminusClickRelay.h"

namespace
{
	// 제목 + 칸 상자 한 묶음
	UPanelWidget* AddShopSection(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Title, const FName& BoxName, bool bWrap)
	{
		UVerticalBox* Section = Tree->ConstructWidget<UVerticalBox>();
		Section->AddChildToVerticalBox(TerminusUI::MakeText(Tree, NAME_None, Title, 16, TerminusUI::Color::Gold, ETextJustify::Left));

		UPanelWidget* Box = nullptr;
		if (bWrap)
		{
			UWrapBox* Wrap = Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), BoxName);
			Wrap->SetInnerSlotPadding(FVector2D(8.f, 8.f));
			Box = Wrap;
		}
		else
		{
			Box = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), BoxName);
		}
		if (UVerticalBoxSlot* S = Section->AddChildToVerticalBox(Box)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Parent->AddChild(Section)))
		{
			HS->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));
			HS->SetVerticalAlignment(VAlign_Top);
		}
		else if (UVerticalBoxSlot* VS = Cast<UVerticalBoxSlot>(Section->Slot))
		{
			VS->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}
		return Box;
	}

	const FLinearColor ShopBuyColor(0.2f, 0.42f, 0.28f);
}

void UShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !RelicBuyBox)
	{
		BuildDefaultLayout();
	}

	if (ReplacePanel)
	{
		ReplacePanel->SetSlotClass(SlotClass);
		ReplacePanel->SetSlotSize(SlotSize);
		ReplacePanel->OnPicked.BindUObject(this, &UShopWidget::HandleReplacePicked);
		ReplacePanel->OnCancelled.BindUObject(this, &UShopWidget::HandleReplaceCancel);
	}
	if (LeaveButton) LeaveButton->OnClicked.AddDynamic(this, &UShopWidget::HandleLeave);

	SetVisibility(ESlateVisibility::Visible);
}

void UShopWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("ShopDim"), TerminusUI::Color::Dim);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(TerminusUI::Color::Panel);
	Panel->SetPadding(FMargin(28.f, 22.f));
	Dim->SetContent(Panel);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Root);

	// 제목 + 던전 재화
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChildToVerticalBox(Header);
	TitleText = TerminusUI::MakeText(WidgetTree, TEXT("TitleText"), TEXT("상점"), 28, TerminusUI::Color::Gold, ETextJustify::Left);
	if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(TitleText)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CurrencyText = TerminusUI::MakeText(WidgetTree, TEXT("CurrencyText"), TEXT(""), 18, FLinearColor::White, ETextJustify::Right);
	if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(CurrencyText)) S->SetVerticalAlignment(VAlign_Center);

	MessageText = TerminusUI::MakeText(WidgetTree, TEXT("MessageText"), TEXT(""), 14, FLinearColor(0.85f, 0.95f, 0.85f), ETextJustify::Left);
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(MessageText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));

	// 구매 줄
	UHorizontalBox* BuyRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChildToVerticalBox(BuyRow);
	RelicBuyBox = AddShopSection(WidgetTree, BuyRow, TEXT("유물 구매"), TEXT("RelicBuyBox"), false);
	SkillBuyBox = AddShopSection(WidgetTree, BuyRow, TEXT("픽업 스킬 구매"), TEXT("SkillBuyBox"), false);

	// 강화 / 판매
	UHorizontalBox* UpgradeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(UpgradeRow)) S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	UpgradeBox = AddShopSection(WidgetTree, UpgradeRow, TEXT("픽업 스킬 강화"), TEXT("UpgradeBox"), false);

	UHorizontalBox* SellRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(SellRow)) S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	SellBox = AddShopSection(WidgetTree, SellRow, TEXT("유물 판매"), TEXT("SellBox"), true);

	// 바꿀 칸 고르기
	const TSubclassOf<UItemPickPanelWidget> PickClass = PickPanelClass ? PickPanelClass : TSubclassOf<UItemPickPanelWidget>(UItemPickPanelWidget::StaticClass());
	ReplacePanel = WidgetTree->ConstructWidget<UItemPickPanelWidget>(PickClass, TEXT("ReplacePanel"));
	TerminusUI::AddRow(Root, ReplacePanel, 16.f);

	LeaveButton = TerminusUI::MakeButton(WidgetTree, TEXT("LeaveButton"), TEXT("  나가기  "), 13, FLinearColor::White, FLinearColor(0.45f, 0.2f, 0.2f));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(LeaveButton))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}
}

void UShopWidget::SetShopState(const FShopState& InState)
{
	State = InState;
	PendingSkillIndex = INDEX_NONE;
	if (ReplacePanel) ReplacePanel->Hide();

	// 내 상태(재화 / 유물 / 스킬 / 강화)가 복제로 바뀌면 다시 그림
	ATerminusPlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (PS && WatchedPS.Get() != PS)
	{
		if (ATerminusPlayerState* Old = WatchedPS.Get()) Old->OnRunStateChanged.RemoveDynamic(this, &UShopWidget::HandleRunStateChanged);
		PS->OnRunStateChanged.AddUniqueDynamic(this, &UShopWidget::HandleRunStateChanged);
		WatchedPS = PS;
	}

	Rebuild();
}

void UShopWidget::NativeDestruct()
{
	if (ATerminusPlayerState* PS = WatchedPS.Get())
	{
		PS->OnRunStateChanged.RemoveDynamic(this, &UShopWidget::HandleRunStateChanged);
	}
	WatchedPS = nullptr;
	Super::NativeDestruct();
}

void UShopWidget::HandleRunStateChanged(const FRunState& NewRunState)
{
	// 방문 기록처럼 상점과 상관없는 변화면 그대로
	if (MakeStateKey() != ShownKey) Rebuild();
}

FString UShopWidget::MakeStateKey() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PS) return FString();

	const FRunState Run = PS->GetRunState();
	FString Key = FString::Printf(TEXT("%d|"), Run.Currency);
	for (const FName& Row : Run.Relics) Key += Row.ToString() + TEXT(",");
	Key += TEXT("|");
	for (const FName& Row : Run.EnhanceSkills) Key += FString::Printf(TEXT("%s:%d,"), *Row.ToString(), PS->GetPickupSkillLevel(Row));
	return Key;
}

void UShopWidget::AddCard(UPanelWidget* Box, bool bRelic, FName Row, const FString& Caption, const FString& ButtonLabel, bool bEnabled, EShopAction Action, int32 Index)
{
	if (!Box) return;

	UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>();

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	if (UItemSlotWidget* ItemSlot = CreateWidget<UItemSlotWidget>(this, Class))
	{
		ItemSlot->SetSlotSize(SlotSize);
		ItemSlot->SetShowName(true);
		if (bRelic) ItemSlot->SetRelic(Row);
		else        ItemSlot->SetSkill(Row);
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(ItemSlot)) S->SetHorizontalAlignment(HAlign_Center);
	}

	if (!Caption.IsEmpty())
	{
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(TerminusUI::MakeText(WidgetTree, NAME_None, Caption, 13, TerminusUI::Color::TextMuted)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
	}

	UButton* Button = TerminusUI::MakeButton(WidgetTree, NAME_None, ButtonLabel, 13, FLinearColor::White, bEnabled ? ShopBuyColor : TerminusUI::Color::ButtonIdle);
	Button->SetIsEnabled(bEnabled);
	UTerminusClickRelay::Bind(Button, this, FSimpleDelegate::CreateUObject(this, &UShopWidget::HandleAction, Action, Index, Row), Relays);
	if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(Button))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	UPanelSlot* PanelSlot = Box->AddChild(Card);
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(PanelSlot)) HS->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
}

void UShopWidget::Rebuild()
{
	ShownKey = MakeStateKey();
	Relays.Reset();

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const FRunState Run = PS ? PS->GetRunState() : FRunState();
	const int32 Currency = Run.Currency;

	if (CurrencyText) CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("던전 재화 %d"), Currency)));
	if (MessageText) MessageText->SetText(State.Message);

	// 유물 구매
	if (RelicBuyBox)
	{
		RelicBuyBox->ClearChildren();
		for (int32 i = 0; i < State.RelicOffers.Num(); ++i)
		{
			const bool bBought = State.RelicBought.IsValidIndex(i) && State.RelicBought[i];
			const int32 Price = State.RelicPrices.IsValidIndex(i) ? State.RelicPrices[i] : 0;
			AddCard(RelicBuyBox, true, State.RelicOffers[i], FString(),
				bBought ? FString(TEXT(" 구매함 ")) : FString::Printf(TEXT(" 구매 %d "), Price),
				!bBought && Currency >= Price, EShopAction::BuyRelic, i);
		}
		if (State.RelicOffers.Num() == 0)
		{
			RelicBuyBox->AddChild(TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("살 수 있는 유물이 없습니다."), 13, TerminusUI::Color::TextDim));
		}
	}

	// 픽업 스킬 구매
	if (SkillBuyBox)
	{
		SkillBuyBox->ClearChildren();
		for (int32 i = 0; i < State.SkillOffers.Num(); ++i)
		{
			const bool bBought = State.SkillBought.IsValidIndex(i) && State.SkillBought[i];
			const int32 Price = State.SkillPrices.IsValidIndex(i) ? State.SkillPrices[i] : 0;
			AddCard(SkillBuyBox, false, State.SkillOffers[i], FString(),
				bBought ? FString(TEXT(" 구매함 ")) : FString::Printf(TEXT(" 구매 %d "), Price),
				!bBought && Currency >= Price, EShopAction::BuySkill, i);
		}
		if (State.SkillOffers.Num() == 0)
		{
			SkillBuyBox->AddChild(TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("살 수 있는 스킬이 없습니다."), 13, TerminusUI::Color::TextDim));
		}
	}

	// 장착한 픽업 스킬 강화
	if (UpgradeBox)
	{
		UpgradeBox->ClearChildren();
		for (const FName& Skill : Run.EnhanceSkills)
		{
			const int32 Level = PS ? PS->GetPickupSkillLevel(Skill) : 0;
			const bool bMax = Level >= ATerminusPlayerState::MaxPickupSkillLevel;
			const int32 Cost = State.UpgradeCostPerLevel * (Level + 1);
			AddCard(UpgradeBox, false, Skill, FString::Printf(TEXT("+%d"), Level),
				bMax ? FString(TEXT(" 최대 ")) : FString::Printf(TEXT(" 강화 %d "), Cost),
				!bMax && Currency >= Cost, EShopAction::Upgrade, INDEX_NONE);
		}
		if (Run.EnhanceSkills.Num() == 0)
		{
			UpgradeBox->AddChild(TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("장착한 픽업 스킬이 없습니다."), 13, TerminusUI::Color::TextDim));
		}
	}

	// 보유 유물 판매
	if (SellBox)
	{
		SellBox->ClearChildren();
		for (const FName& Row : Run.Relics)
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			const bool bCanSell = Relic && Relic->RelicTier != ERelicTier::Basic && Relic->CanSellInShop();
			AddCard(SellBox, true, Row, FString(),
				bCanSell ? FString::Printf(TEXT(" 판매 %d "), Relic->SellPrice_Dungeon) : FString(TEXT(" 판매 불가 ")),
				bCanSell, EShopAction::Sell, INDEX_NONE);
		}
	}

	if (PendingSkillIndex != INDEX_NONE) ShowReplace();
}

void UShopWidget::HandleAction(EShopAction Action, int32 Index, FName Row)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PC || !PS) return;

	switch (Action)
	{
	case EShopAction::BuyRelic:
		PC->Server_ShopBuyRelic(Index);
		break;

	case EShopAction::BuySkill:
		// 강화 칸이 꽉 찼으면 바꿀 칸부터
		if (PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount())
		{
			PendingSkillIndex = Index;
			ShowReplace();
			return;
		}
		PC->Server_ShopBuySkill(Index, INDEX_NONE);
		break;

	case EShopAction::Upgrade:
		PC->Server_ShopUpgradeSkill(Row);
		break;

	case EShopAction::Sell:
		PC->Server_ShopSellRelic(Row);
		break;
	}
}

void UShopWidget::ShowReplace()
{
	if (ReplacePanel)
	{
		ReplacePanel->ShowEnhanceSkills(FText::FromString(TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요.")));
	}
}

void UShopWidget::HandleReplacePicked(UItemSlotWidget* ClickedSlot)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC || !ClickedSlot || PendingSkillIndex == INDEX_NONE) return;

	PC->Server_ShopBuySkill(PendingSkillIndex, ClickedSlot->GetSlotIndex());
	HandleReplaceCancel();
}

void UShopWidget::HandleReplaceCancel()
{
	PendingSkillIndex = INDEX_NONE;
	if (ReplacePanel) ReplacePanel->Hide();
}

void UShopWidget::HandleLeave()
{
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ShopLeave();
	}
}
