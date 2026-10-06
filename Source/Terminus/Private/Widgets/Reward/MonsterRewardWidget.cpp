#include "Widgets/Reward/MonsterRewardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Skill/SkillCardWidget.h"

namespace
{
	UTextBlock* MakeRewardText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	UButton* MakeRewardButton(UWidgetTree* Tree, const FName& Name, const FString& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeRewardText(Tree, NAME_None, 17, FLinearColor::Black);
		Text->SetText(FText::FromString(Label));
		Button->SetContent(Text);
		return Button;
	}

	void AddCentered(UVerticalBox* Column, UWidget* Widget, float Top)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Widget))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
	}

	// 바꾸기 패널 하나 (안내 + 칸 줄 + 취소)
	UVerticalBox* MakeRewardReplacePanel(UWidgetTree* Tree, const FName& PanelName, const FString& Guide,
		const FName& BoxName, TObjectPtr<UPanelWidget>& OutBox, const FName& CancelName, TObjectPtr<UButton>& OutCancel)
	{
		UVerticalBox* Panel = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), PanelName);

		UTextBlock* GuideText = MakeRewardText(Tree, NAME_None, 16, FLinearColor(0.85f, 0.85f, 0.85f));
		GuideText->SetText(FText::FromString(Guide));
		AddCentered(Panel, GuideText, 0.f);

		UHorizontalBox* Box = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), BoxName);
		OutBox = Box;
		AddCentered(Panel, Box, 12.f);

		OutCancel = MakeRewardButton(Tree, CancelName, TEXT("  취소  "));
		AddCentered(Panel, OutCancel, 12.f);
		return Panel;
	}

	FString RewardRelicName(FName Row)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		return Relic ? Relic->RelicName.ToString() : Row.ToString();
	}

	FString RewardSkillName(FName Row)
	{
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
		return Skill ? Skill->DisplayName_KR.ToString() : Row.ToString();
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UMonsterRewardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	if (NextButton)               NextButton->OnClicked.AddDynamic(this, &UMonsterRewardWidget::HandleNextClicked);
	if (ReplaceCancelButton)      ReplaceCancelButton->OnClicked.AddDynamic(this, &UMonsterRewardWidget::HandleReplaceCancel);
	if (RelicReplaceCancelButton) RelicReplaceCancelButton->OnClicked.AddDynamic(this, &UMonsterRewardWidget::HandleRelicReplaceCancel);

	// 아래 화면(전투 HUD)을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UMonsterRewardWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RewardDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeRewardText(WidgetTree, TEXT("TitleText"), 30, FLinearColor(1.f, 0.85f, 0.35f));
	AddCentered(Column, TitleText, 0.f);

	CurrencyText = MakeRewardText(WidgetTree, TEXT("CurrencyText"), 18, FLinearColor::White);
	AddCentered(Column, CurrencyText, 10.f);

	// 스킬 / 유물 섹션을 나란히
	UHorizontalBox* Sections = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddCentered(Column, Sections, 24.f);

	UVerticalBox* Skill = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SkillSection"));
	SkillSection = Skill;
	if (UHorizontalBoxSlot* S = Sections->AddChildToHorizontalBox(Skill)) S->SetPadding(FMargin(20.f, 0.f));
	UTextBlock* SkillTitle = MakeRewardText(WidgetTree, NAME_None, 17, FLinearColor(0.8f, 0.8f, 0.8f));
	SkillTitle->SetText(FText::FromString(TEXT("픽업 스킬")));
	AddCentered(Skill, SkillTitle, 0.f);
	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	AddCentered(Skill, CardBox, 10.f);

	UVerticalBox* Relic = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RelicSection"));
	RelicSection = Relic;
	if (UHorizontalBoxSlot* S = Sections->AddChildToHorizontalBox(Relic)) S->SetPadding(FMargin(20.f, 0.f));
	UTextBlock* RelicTitle = MakeRewardText(WidgetTree, NAME_None, 17, FLinearColor(0.8f, 0.8f, 0.8f));
	RelicTitle->SetText(FText::FromString(TEXT("유물")));
	AddCentered(Relic, RelicTitle, 0.f);
	RelicBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RelicBox"));
	AddCentered(Relic, RelicBox, 10.f);

	// 바꾸기 패널
	ReplacePanel = MakeRewardReplacePanel(WidgetTree, TEXT("ReplacePanel"), TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요"),
		TEXT("ReplaceBox"), ReplaceBox, TEXT("ReplaceCancelButton"), ReplaceCancelButton);
	AddCentered(Column, ReplacePanel, 24.f);

	RelicReplacePanel = MakeRewardReplacePanel(WidgetTree, TEXT("RelicReplacePanel"), TEXT("유물 칸이 가득 찼습니다. 버릴 유물을 고르세요"),
		TEXT("RelicReplaceBox"), RelicReplaceBox, TEXT("RelicReplaceCancelButton"), RelicReplaceCancelButton);
	AddCentered(Column, RelicReplacePanel, 24.f);

	ResultText = MakeRewardText(WidgetTree, TEXT("ResultText"), 18, FLinearColor::White);
	AddCentered(Column, ResultText, 24.f);

	NextButton = MakeRewardButton(WidgetTree, TEXT("NextButton"), TEXT("   다음으로   "));
	AddCentered(Column, NextButton, 24.f);
}

// =====================================================================
// 내용
// =====================================================================

void UMonsterRewardWidget::Setup(const FRoomRewardOffer& InOffer)
{
	Offer = InOffer;
	ChosenSkill = PendingSkill = NAME_None;
	ChosenRelic = PendingRelic = ReplaceRelic = NAME_None;
	ReplaceSlot = INDEX_NONE;

	if (TitleText)
	{
		const TCHAR* Title = Offer.RoomType == ERoomType::BOSS ? TEXT("보스 처치!")
			: Offer.RoomType == ERoomType::GUARDIAN ? TEXT("가디언 처치!")
			: TEXT("전투 승리!");
		TitleText->SetText(FText::FromString(Title));
	}
	if (CurrencyText)
	{
		CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("던전 재화 +%d"), Offer.Currency)));
		CurrencyText->SetVisibility(Offer.Currency > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	BuildSkillCards();
	BuildRelicCards();
	Refresh();
}

UItemSlotWidget* UMonsterRewardWidget::MakeSlot()
{
	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	return CreateWidget<UItemSlotWidget>(this, Class);
}

void UMonsterRewardWidget::BuildSkillCards()
{
	if (!CardBox) return;
	CardBox->ClearChildren();

	const TSubclassOf<USkillCardWidget> Class = CardClass ? CardClass : TSubclassOf<USkillCardWidget>(USkillCardWidget::StaticClass());
	for (int32 i = 0; i < Offer.SkillOffers.Num(); ++i)
	{
		USkillCardWidget* Card = CreateWidget<USkillCardWidget>(this, Class);
		if (!Card) continue;

		Card->SetSkill(Offer.SkillOffers[i]);
		Card->OnCardClicked.BindUObject(this, &UMonsterRewardWidget::HandleCardClicked);
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(CardBox->AddChild(Card)))
		{
			HSlot->SetPadding(FMargin(CardSpacing * 0.5f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UMonsterRewardWidget::BuildRelicCards()
{
	if (!RelicBox) return;
	RelicBox->ClearChildren();

	for (const FName& Row : Offer.RelicOffers)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		UItemSlotWidget* RelicSlot = MakeSlot();
		if (!Relic || !RelicSlot) continue;

		RelicSlot->SetShowName(true);
		RelicSlot->SetSlotSize(FVector2D(120.f, 120.f));
		RelicSlot->SetRelic(Row);
		RelicSlot->OnSlotClicked.BindUObject(this, &UMonsterRewardWidget::HandleRelicClicked);

		// 칸 + 설명
		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>();
		AddCentered(Card, RelicSlot, 0.f);

		USizeBox* DescSize = WidgetTree->ConstructWidget<USizeBox>();
		DescSize->SetWidthOverride(220.f);
		UTextBlock* Desc = MakeRewardText(WidgetTree, NAME_None, 14, FLinearColor(0.9f, 0.9f, 0.9f));
		Desc->SetText(Relic->RelicDesc);
		Desc->SetAutoWrapText(true);
		DescSize->SetContent(Desc);
		AddCentered(Card, DescSize, 8.f);

		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(RelicBox->AddChild(Card)))
		{
			HSlot->SetPadding(FMargin(CardSpacing * 0.5f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
}

void UMonsterRewardWidget::Refresh()
{
	const bool bSkillReplacing = !PendingSkill.IsNone();
	const bool bRelicReplacing = !PendingRelic.IsNone();
	const bool bReplacing = bSkillReplacing || bRelicReplacing;

	// 후보는 아직 안 골랐고 바꾸기 중이 아닐 때만
	if (SkillSection)
	{
		const bool bShow = !bReplacing && ChosenSkill.IsNone() && Offer.SkillOffers.Num() > 0;
		SkillSection->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (CardBox)
	{
		CardBox->SetVisibility(!bReplacing && ChosenSkill.IsNone() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (RelicSection)
	{
		const bool bShow = !bReplacing && ChosenRelic.IsNone() && Offer.RelicOffers.Num() > 0;
		RelicSection->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (RelicBox)
	{
		RelicBox->SetVisibility(!bReplacing && ChosenRelic.IsNone() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ReplacePanel)      ReplacePanel->SetVisibility(bSkillReplacing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (RelicReplacePanel) RelicReplacePanel->SetVisibility(bRelicReplacing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (!ResultText) return;

	TArray<FString> Lines;
	if (!ChosenSkill.IsNone())
	{
		Lines.Add(FString::Printf(TEXT("획득: %s%s"), *RewardSkillName(ChosenSkill),
			ReplaceSlot != INDEX_NONE ? *FString::Printf(TEXT("  (강화 칸 %d 교체)"), ReplaceSlot + 1) : TEXT("")));
	}
	if (!ChosenRelic.IsNone())
	{
		Lines.Add(FString::Printf(TEXT("획득: %s%s"), *RewardRelicName(ChosenRelic),
			!ReplaceRelic.IsNone() ? *FString::Printf(TEXT("  (%s 버림)"), *RewardRelicName(ReplaceRelic)) : TEXT("")));
	}

	const bool bAnyLeft = (ChosenSkill.IsNone() && Offer.SkillOffers.Num() > 0) || (ChosenRelic.IsNone() && Offer.RelicOffers.Num() > 0);
	if (bReplacing)
	{
		Lines.Reset();
	}
	else if (Lines.Num() == 0)
	{
		Lines.Add(bAnyLeft ? TEXT("받을 보상을 고르세요 (안 받아도 됩니다)") : TEXT("받을 수 있는 보상이 없습니다"));
	}
	ResultText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

// =====================================================================
// 스킬
// =====================================================================

void UMonsterRewardWidget::HandleCardClicked(FName SkillRow)
{
	if (bFinished || !ChosenSkill.IsNone() || !PendingRelic.IsNone()) return;

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 칸이 꽉 찼으면 바꿀 칸부터
	if (PS && PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount())
	{
		PendingSkill = SkillRow;
		ShowSkillReplace();
	}
	else
	{
		ChosenSkill = SkillRow;
	}
	Refresh();
}

void UMonsterRewardWidget::ShowSkillReplace()
{
	if (!ReplaceBox) return;
	ReplaceBox->ClearChildren();

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const TArray<FName> Equipped = PS ? PS->GetRunState().EnhanceSkills : TArray<FName>();

	for (int32 i = 0; i < Equipped.Num(); ++i)
	{
		UItemSlotWidget* EquippedSlot = MakeSlot();
		if (!EquippedSlot) continue;

		EquippedSlot->SetSlotIndex(i);
		EquippedSlot->SetSkill(Equipped[i]);
		EquippedSlot->OnSlotClicked.BindUObject(this, &UMonsterRewardWidget::HandleReplaceSlotClicked);
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(ReplaceBox->AddChild(EquippedSlot)))
		{
			HSlot->SetPadding(FMargin(6.f, 0.f));
		}
	}
}

void UMonsterRewardWidget::HandleReplaceSlotClicked(UItemSlotWidget* ClickedSlot)
{
	if (bFinished || PendingSkill.IsNone() || !ClickedSlot) return;

	ChosenSkill = PendingSkill;
	PendingSkill = NAME_None;
	ReplaceSlot = ClickedSlot->GetSlotIndex();
	Refresh();
}

void UMonsterRewardWidget::HandleReplaceCancel()
{
	PendingSkill = NAME_None;
	Refresh();
}

// =====================================================================
// 유물
// =====================================================================

void UMonsterRewardWidget::HandleRelicClicked(UItemSlotWidget* ClickedSlot)
{
	if (bFinished || !ClickedSlot || !ChosenRelic.IsNone() || !PendingSkill.IsNone()) return;

	const FName Row = ClickedSlot->GetItemRow();
	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 칸이 꽉 찼으면 버릴 유물부터
	if (PS && PS->GetRelics().Num() >= PS->GetRelicCapacity())
	{
		PendingRelic = Row;
		ShowRelicReplace();
	}
	else
	{
		ChosenRelic = Row;
	}
	Refresh();
}

void UMonsterRewardWidget::ShowRelicReplace()
{
	if (!RelicReplaceBox) return;
	RelicReplaceBox->ClearChildren();

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PS) return;

	for (const FName& Owned : PS->GetRelics())
	{
		// 직업 기본 유물(패시브)은 못 버림
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Owned);
		if (!Relic || Relic->RelicTier == ERelicTier::Basic) continue;

		UItemSlotWidget* OwnedSlot = MakeSlot();
		if (!OwnedSlot) continue;

		OwnedSlot->SetShowName(true);
		OwnedSlot->SetRelic(Owned);
		OwnedSlot->OnSlotClicked.BindUObject(this, &UMonsterRewardWidget::HandleRelicReplaceClicked);
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(RelicReplaceBox->AddChild(OwnedSlot)))
		{
			HSlot->SetPadding(FMargin(6.f, 0.f));
		}
	}
}

void UMonsterRewardWidget::HandleRelicReplaceClicked(UItemSlotWidget* ClickedSlot)
{
	if (bFinished || PendingRelic.IsNone() || !ClickedSlot) return;

	ChosenRelic = PendingRelic;
	PendingRelic = NAME_None;
	ReplaceRelic = ClickedSlot->GetItemRow();
	Refresh();
}

void UMonsterRewardWidget::HandleRelicReplaceCancel()
{
	PendingRelic = NAME_None;
	Refresh();
}

// =====================================================================
// 마침
// =====================================================================

void UMonsterRewardWidget::HandleNextClicked()
{
	if (bFinished) return;
	bFinished = true;

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_FinishRoomReward(ChosenSkill, ReplaceSlot, ChosenRelic, ReplaceRelic);
	}

	RemoveFromParent();
}
