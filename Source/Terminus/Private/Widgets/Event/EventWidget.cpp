#include "Widgets/Event/EventWidget.h"

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
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/Common/ChoiceCardWidget.h"
#include "Widgets/TerminusUISettings.h"

void UEventWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	if (PickPanel)
	{
		PickPanel->SetSlotClass(SlotClass);
		PickPanel->OnCancelled.BindUObject(this, &UEventWidget::HandlePickCancel);
	}

	// 아래 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UEventWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("EventDim"), TerminusUI::Color::Dim);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("이벤트"), TerminusUI::FontSize::ScreenTitle, TerminusUI::Color::Gold);
	TerminusUI::SetBold(TitleText);
	TerminusUI::SetLetterSpacing(TitleText, 80);
	TerminusUI::AddRow(Column, TitleText, 0.f);

	// 상황 설명은 길어서 폭을 정해 둠
	SituationText = TerminusUI::MakeWrapText(WidgetTree, TEXT("SituationText"), TEXT(""), 15, TerminusUI::Color::TextBright);
	USizeBox* SituationWidth = WidgetTree->ConstructWidget<USizeBox>();
	SituationWidth->SetWidthOverride(760.f);
	SituationWidth->SetContent(SituationText);
	TerminusUI::AddRow(Column, SituationWidth, 12.f);

	// 카드가 접혀도(결과만 보일 때) 글이 좁게 줄바꿈되지 않게 최소 폭을 줌
	MessageText = TerminusUI::MakeWrapText(WidgetTree, TEXT("MessageText"), TEXT(""), 16, TerminusUI::Color::TextBright);
	USizeBox* MessageWidth = WidgetTree->ConstructWidget<USizeBox>();
	MessageWidth->SetMinDesiredWidth(560.f);
	MessageWidth->SetContent(MessageText);
	TerminusUI::AddRow(Column, MessageWidth, 10.f);

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	TerminusUI::AddRow(Column, CardBox, 24.f);

	const TSubclassOf<UItemPickPanelWidget> PickClass = UTerminusUISettings::Pick(PickPanelClass, &UTerminusUISettings::ItemPickPanel);
	PickPanel = WidgetTree->ConstructWidget<UItemPickPanelWidget>(PickClass, TEXT("PickPanel"));
	TerminusUI::AddRow(Column, PickPanel, 24.f);
}

void UEventWidget::Setup(const FText& InTitle, const FText& InSituation, const TArray<FEventOption>& InOptions)
{
	Options = InOptions;
	Situation = InSituation;
	PendingIndex = INDEX_NONE;
	bChosen = false;

	if (TitleText) TitleText->SetText(InTitle.IsEmpty() ? FText::FromString(TEXT("이벤트")) : InTitle);
	if (SituationText)
	{
		SituationText->SetText(InSituation);
		SituationText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	const bool bAny = Options.ContainsByPredicate([](const FEventOption& O) { return O.bAvailable; });
	SetMessage(bAny ? TEXT("하나를 고르세요.") : TEXT("아무 일도 일어나지 않았습니다."));
	if (PickPanel) PickPanel->Hide();

	RebuildCards();
}

void UEventWidget::SetMessage(const FString& Message)
{
	if (!MessageText) return;

	// WBP 에 SituationText 가 없으면 상황 설명을 안내 위에 같이
	if (!SituationText && !Situation.IsEmpty())
	{
		MessageText->SetText(FText::FromString(Situation.ToString() + TEXT("\n\n") + Message));
		return;
	}
	MessageText->SetText(FText::FromString(Message));
}

void UEventWidget::RebuildCards()
{
	if (!CardBox) return;

	CardBox->ClearChildren();
	CardBox->SetVisibility(PendingIndex == INDEX_NONE ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	const TSubclassOf<UChoiceCardWidget> Class = UTerminusUISettings::Pick(CardClass, &UTerminusUISettings::ChoiceCard);
	const UEnum* TypeEnum = StaticEnum<EEventOutcomeType>();

	for (int32 i = 0; i < Options.Num(); ++i)
	{
		const FEventOption& Option = Options[i];

		UChoiceCardWidget* Card = CreateWidget<UChoiceCardWidget>(this, Class);
		if (!Card) continue;

		FChoiceCardStyle CardStyle;
		CardStyle.Size = CardSize;
		CardStyle.TitleSize = 20;
		CardStyle.Padding = FMargin(20.f);
		Card->ApplyDefaultStyle(CardStyle);

		Card->SetChoiceId(TypeEnum ? FName(*TypeEnum->GetNameStringByValue(static_cast<int64>(Option.Outcome))) : NAME_None);
		// 못 고르는 이유는 설명 아래에 (WBP 카드엔 아래 줄이 없을 수 있어서 설명에 붙임)
		Card->SetTexts(Option.Title.ToString(), Option.bAvailable
			? Option.Description.ToString()
			: FString::Printf(TEXT("%s\n\n(%s)"), *Option.Description.ToString(), *Option.UnavailableReason.ToString()));
		if (Option.GivesSkill()) Card->SetItem(false, Option.SkillRow);
		else Card->SetItem(true, Option.Outcome == EEventOutcomeType::SwapRelicSameTierRandom || Option.Outcome == EEventOutcomeType::SwapRelicHigherTierRandom ? NAME_None : Option.RelicRow);
		Card->SetChoiceEnabled(!bChosen && Option.bAvailable);
		Card->OnChosen = FSimpleDelegate::CreateUObject(this, &UEventWidget::HandleCardClicked, i);

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(CardBox->AddChild(Card)))
		{
			S->SetPadding(FMargin(10.f, 0.f));
		}
	}
}

void UEventWidget::HandleCardClicked(int32 Index)
{
	if (bChosen || !Options.IsValidIndex(Index) || !Options[Index].bAvailable) return;

	const FEventOption& Option = Options[Index];
	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 스킬인데 강화 칸이 꽉 찼으면 바꿀 칸부터 / 유물인데 유물 칸이 꽉 찼으면 버릴 유물부터
	const bool bNeedSkillSlot = Option.GivesSkill() && PS && PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount();
	const bool bNeedRelicSlot = Option.GivesRelic() && PS && PS->GetRelics().Num() >= PS->GetRelicCapacity();
	if (bNeedSkillSlot || bNeedRelicSlot)
	{
		PendingIndex = Index;
		ShowPick(Option, bNeedSkillSlot);
		RebuildCards();
		return;
	}

	Send(Index, INDEX_NONE, NAME_None);
}

void UEventWidget::ShowPick(const FEventOption& Option, bool bSkillSlot)
{
	if (!PickPanel) return;

	if (!bSkillSlot)
	{
		// 기본 / 업그레이드 유물은 버릴 수 없음
		PickPanel->OnPicked.BindUObject(this, &UEventWidget::HandleRelicPicked);
		PickPanel->ShowRelics(FText::FromString(FString::Printf(TEXT("유물 칸이 가득 찼습니다. [%s] 대신 버릴 유물을 고르세요."),
				*UTerminusDataSettings::GetRelicName(Option.RelicRow))),
			[](const FRelicRow& Relic) { return Relic.RelicTier != ERelicTier::Basic && Relic.RelicTier != ERelicTier::Upgrade; });
		return;
	}

	PickPanel->OnPicked.BindUObject(this, &UEventWidget::HandleSkillSlotPicked);
	PickPanel->ShowEnhanceSkills(FText::FromString(TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요.")));
}

void UEventWidget::HandleSkillSlotPicked(UItemSlotWidget* ClickedSlot)
{
	if (bChosen || PendingIndex == INDEX_NONE || !ClickedSlot) return;
	Send(PendingIndex, ClickedSlot->GetSlotIndex(), NAME_None);
}

void UEventWidget::HandleRelicPicked(UItemSlotWidget* ClickedSlot)
{
	if (bChosen || PendingIndex == INDEX_NONE || !ClickedSlot) return;
	Send(PendingIndex, INDEX_NONE, ClickedSlot->GetItemRow());
}

void UEventWidget::HandlePickCancel()
{
	PendingIndex = INDEX_NONE;
	if (PickPanel) PickPanel->Hide();
	RebuildCards();
}

void UEventWidget::Send(int32 Index, int32 ReplaceSlot, FName RelicRow)
{
	bChosen = true;
	PendingIndex = INDEX_NONE;
	if (PickPanel) PickPanel->Hide();
	SetMessage(TEXT("..."));
	RebuildCards();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseEvent(Index, ReplaceSlot, RelicRow);
	}
}

void UEventWidget::ShowResult(const FText& Result)
{
	if (CardBox) CardBox->SetVisibility(ESlateVisibility::Collapsed);
	if (SituationText) SituationText->SetVisibility(ESlateVisibility::Collapsed);   // 결과 글만 보이게
	if (MessageText) MessageText->SetText(Result);
}
