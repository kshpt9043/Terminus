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
#include "Widgets/Common/ItemPickPanelWidget.h"
#include "Widgets/Common/ItemSlotWidget.h"
#include "Widgets/Skill/SkillCardWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/Common/ItemCardWidget.h"

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
	if (ReplacePanel)
	{
		ReplacePanel->SetSlotClass(SlotClass);
		ReplacePanel->OnPicked.BindUObject(this, &UMonsterRewardWidget::HandleReplaceSlotClicked);
		ReplacePanel->OnCancelled.BindUObject(this, &UMonsterRewardWidget::HandleReplaceCancel);
	}
	if (RelicReplacePanel)
	{
		RelicReplacePanel->SetSlotClass(SlotClass);
		RelicReplacePanel->OnPicked.BindUObject(this, &UMonsterRewardWidget::HandleRelicReplaceClicked);
		RelicReplacePanel->OnCancelled.BindUObject(this, &UMonsterRewardWidget::HandleRelicReplaceCancel);
	}

	// 아래 화면(전투 HUD)을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UMonsterRewardWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("RewardDim"), TerminusUI::Color::DimStrong);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = TerminusUI::MakeText(WidgetTree, TEXT("TitleText"), TEXT(""), 30, TerminusUI::Color::Gold);
	TerminusUI::AddRow(Column, TitleText, 0.f);

	CurrencyText = TerminusUI::MakeText(WidgetTree, TEXT("CurrencyText"), TEXT(""), 18, FLinearColor::White);
	TerminusUI::AddRow(Column, CurrencyText, 10.f);

	// 스킬 / 유물 섹션을 나란히
	UHorizontalBox* Sections = WidgetTree->ConstructWidget<UHorizontalBox>();
	TerminusUI::AddRow(Column, Sections, 24.f);

	UVerticalBox* Skill = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SkillSection"));
	SkillSection = Skill;
	if (UHorizontalBoxSlot* S = Sections->AddChildToHorizontalBox(Skill)) S->SetPadding(FMargin(20.f, 0.f));
	UTextBlock* SkillTitle = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 17, TerminusUI::Color::TextMuted);
	SkillTitle->SetText(FText::FromString(TEXT("픽업 스킬")));
	TerminusUI::AddRow(Skill, SkillTitle, 0.f);
	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	TerminusUI::AddRow(Skill, CardBox, 10.f);

	UVerticalBox* Relic = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RelicSection"));
	RelicSection = Relic;
	if (UHorizontalBoxSlot* S = Sections->AddChildToHorizontalBox(Relic)) S->SetPadding(FMargin(20.f, 0.f));
	UTextBlock* RelicTitle = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 17, TerminusUI::Color::TextMuted);
	RelicTitle->SetText(FText::FromString(TEXT("유물")));
	TerminusUI::AddRow(Relic, RelicTitle, 0.f);
	RelicBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RelicBox"));
	TerminusUI::AddRow(Relic, RelicBox, 10.f);

	// 바꾸기 패널
	const TSubclassOf<UItemPickPanelWidget> PickClass = PickPanelClass ? PickPanelClass : TSubclassOf<UItemPickPanelWidget>(UItemPickPanelWidget::StaticClass());
	ReplacePanel = WidgetTree->ConstructWidget<UItemPickPanelWidget>(PickClass, TEXT("ReplacePanel"));
	TerminusUI::AddRow(Column, ReplacePanel, 24.f);

	RelicReplacePanel = WidgetTree->ConstructWidget<UItemPickPanelWidget>(PickClass, TEXT("RelicReplacePanel"));
	TerminusUI::AddRow(Column, RelicReplacePanel, 24.f);

	ResultText = TerminusUI::MakeText(WidgetTree, TEXT("ResultText"), TEXT(""), 18, FLinearColor::White);
	TerminusUI::AddRow(Column, ResultText, 24.f);

	NextButton = TerminusUI::MakeButton(WidgetTree, TEXT("NextButton"), TEXT("   다음으로   "), 17);
	TerminusUI::AddRow(Column, NextButton, 24.f);
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
		UItemCardWidget* Card = Relic ? CreateWidget<UItemCardWidget>(this, RelicCardClass ? RelicCardClass : TSubclassOf<UItemCardWidget>(UItemCardWidget::StaticClass())) : nullptr;
		if (!Card) continue;

		Card->SetSlotClass(SlotClass);
		Card->SetCaptionStyle(220.f, 14, TerminusUI::Color::TextBright);
		Card->SetItem(true, Row, FVector2D(120.f, 120.f));
		Card->SetCaption(Relic->RelicDesc.ToString());
		Card->SetAction(FString(), false);
		if (UItemSlotWidget* RelicSlot = Card->GetItemSlot())
		{
			RelicSlot->OnSlotClicked.BindUObject(this, &UMonsterRewardWidget::HandleRelicClicked);
		}

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
		Lines.Add(FString::Printf(TEXT("획득: %s%s"), *UTerminusDataSettings::GetSkillName(ChosenSkill),
			ReplaceSlot != INDEX_NONE ? *FString::Printf(TEXT("  (강화 칸 %d 교체)"), ReplaceSlot + 1) : TEXT("")));
	}
	if (!ChosenRelic.IsNone())
	{
		Lines.Add(FString::Printf(TEXT("획득: %s%s"), *UTerminusDataSettings::GetRelicName(ChosenRelic),
			!ReplaceRelic.IsNone() ? *FString::Printf(TEXT("  (%s 버림)"), *UTerminusDataSettings::GetRelicName(ReplaceRelic)) : TEXT("")));
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
	if (ReplacePanel)
	{
		ReplacePanel->ShowEnhanceSkills(FText::FromString(TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요")));
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
	if (RelicReplacePanel)
	{
		// 직업 기본 유물(패시브)은 못 버림
		RelicReplacePanel->ShowRelics(FText::FromString(TEXT("유물 칸이 가득 찼습니다. 버릴 유물을 고르세요")),
			[](const FRelicRow& Relic) { return Relic.RelicTier != ERelicTier::Basic; });
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
