#include "Widgets/Reward/MonsterRewardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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

	if (NextButton)          NextButton->OnClicked.AddDynamic(this, &UMonsterRewardWidget::HandleNextClicked);
	if (ReplaceCancelButton) ReplaceCancelButton->OnClicked.AddDynamic(this, &UMonsterRewardWidget::HandleReplaceCancel);

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

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	AddCentered(Column, CardBox, 28.f);

	// 바꿀 칸 고르기
	UVerticalBox* Replace = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ReplacePanel"));
	ReplacePanel = Replace;
	AddCentered(Column, Replace, 28.f);

	UTextBlock* ReplaceGuide = MakeRewardText(WidgetTree, NAME_None, 16, FLinearColor(0.85f, 0.85f, 0.85f));
	ReplaceGuide->SetText(FText::FromString(TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요")));
	AddCentered(Replace, ReplaceGuide, 0.f);

	ReplaceBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ReplaceBox"));
	AddCentered(Replace, ReplaceBox, 12.f);

	ReplaceCancelButton = MakeRewardButton(WidgetTree, TEXT("ReplaceCancelButton"), TEXT("  취소  "));
	AddCentered(Replace, ReplaceCancelButton, 12.f);

	ResultText = MakeRewardText(WidgetTree, TEXT("ResultText"), 18, FLinearColor::White);
	AddCentered(Column, ResultText, 24.f);

	NextButton = MakeRewardButton(WidgetTree, TEXT("NextButton"), TEXT("   다음으로   "));
	AddCentered(Column, NextButton, 24.f);
}

// =====================================================================
// 단계
// =====================================================================

void UMonsterRewardWidget::Setup(int32 InCurrency, const TArray<FName>& InSkillOffers)
{
	Offers = InSkillOffers;
	ChosenSkill = NAME_None;
	PendingSkill = NAME_None;
	ReplaceSlot = INDEX_NONE;

	if (TitleText)    TitleText->SetText(FText::FromString(TEXT("전투 승리!")));
	if (CurrencyText) CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("던전 재화 +%d"), InCurrency)));

	// 카드 만들기 (한 번)
	if (CardBox)
	{
		CardBox->ClearChildren();
		const TSubclassOf<USkillCardWidget> Class = CardClass ? CardClass : TSubclassOf<USkillCardWidget>(USkillCardWidget::StaticClass());
		for (int32 i = 0; i < Offers.Num(); ++i)
		{
			USkillCardWidget* Card = CreateWidget<USkillCardWidget>(this, Class);
			if (!Card) continue;

			Card->SetSkill(Offers[i]);
			Card->OnCardClicked.BindUObject(this, &UMonsterRewardWidget::HandleCardClicked);
			if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(CardBox->AddChild(Card)))
			{
				HSlot->SetPadding(FMargin(i == 0 ? 0.f : CardSpacing * 0.5f, 0.f, i == Offers.Num() - 1 ? 0.f : CardSpacing * 0.5f, 0.f));
				HSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	ShowCards();
}

void UMonsterRewardWidget::ShowCards()
{
	if (CardBox)      CardBox->SetVisibility(Offers.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ReplacePanel) ReplacePanel->SetVisibility(ESlateVisibility::Collapsed);

	if (ResultText)
	{
		ResultText->SetText(FText::FromString(Offers.Num() > 0
			? TEXT("강화 스킬을 하나 고를 수 있습니다 (안 골라도 됩니다)")
			: TEXT("받을 수 있는 강화 스킬이 없습니다")));
	}
}

void UMonsterRewardWidget::ShowReplace()
{
	if (CardBox)      CardBox->SetVisibility(ESlateVisibility::Collapsed);
	if (ReplacePanel) ReplacePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (ResultText)   ResultText->SetText(FText::GetEmpty());

	if (!ReplaceBox) return;
	ReplaceBox->ClearChildren();

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const TArray<FName> Equipped = PS ? PS->GetRunState().EnhanceSkills : TArray<FName>();

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	for (int32 i = 0; i < Equipped.Num(); ++i)
	{
		UItemSlotWidget* EquippedSlot = CreateWidget<UItemSlotWidget>(this, Class);
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

void UMonsterRewardWidget::ShowResult()
{
	if (CardBox)      CardBox->SetVisibility(ESlateVisibility::Collapsed);
	if (ReplacePanel) ReplacePanel->SetVisibility(ESlateVisibility::Collapsed);

	if (ResultText)
	{
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(ChosenSkill);
		FString Label = FString::Printf(TEXT("획득: %s"), Skill ? *Skill->DisplayName_KR.ToString() : *ChosenSkill.ToString());
		if (ReplaceSlot != INDEX_NONE)
		{
			Label += FString::Printf(TEXT("  (강화 칸 %d 교체)"), ReplaceSlot + 1);
		}
		ResultText->SetText(FText::FromString(Label));
	}
}

// =====================================================================
// 입력
// =====================================================================

void UMonsterRewardWidget::HandleCardClicked(FName SkillRow)
{
	if (bFinished || !ChosenSkill.IsNone()) return;

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 칸이 꽉 찼으면 바꿀 칸부터
	if (PS && PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount())
	{
		PendingSkill = SkillRow;
		ShowReplace();
		return;
	}

	ChosenSkill = SkillRow;
	ShowResult();
}

void UMonsterRewardWidget::HandleReplaceSlotClicked(UItemSlotWidget* ClickedSlot)
{
	if (bFinished || PendingSkill.IsNone() || !ClickedSlot) return;

	ChosenSkill = PendingSkill;
	PendingSkill = NAME_None;
	ReplaceSlot = ClickedSlot->GetSlotIndex();
	ShowResult();
}

void UMonsterRewardWidget::HandleReplaceCancel()
{
	PendingSkill = NAME_None;
	ShowCards();
}

void UMonsterRewardWidget::HandleNextClicked()
{
	if (bFinished) return;
	bFinished = true;

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_FinishMonsterReward(ChosenSkill, ReplaceSlot);
	}

	RemoveFromParent();
}
