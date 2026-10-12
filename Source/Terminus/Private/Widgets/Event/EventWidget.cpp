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
#include "Widgets/Common/TerminusClickRelay.h"

namespace
{
	bool IsEventSkillOption(EEventOptionType Type)
	{
		return Type == EEventOptionType::ClassSkill || Type == EEventOptionType::OtherClassSkill || Type == EEventOptionType::EventSkill;
	}

	const FLinearColor EventPurple(0.8f, 0.65f, 1.f);
}

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
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("EventDim"), FLinearColor(0.02f, 0.f, 0.04f, 0.7f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("이벤트"), 32, EventPurple);
	TerminusUI::AddRow(Column, TitleText, 0.f);

	// 카드가 접혀도(결과만 보일 때) 글이 좁게 줄바꿈되지 않게 최소 폭을 줌
	MessageText = TerminusUI::MakeWrapText(WidgetTree, TEXT("MessageText"), TEXT(""), 16, TerminusUI::Color::TextBright);
	USizeBox* MessageWidth = WidgetTree->ConstructWidget<USizeBox>();
	MessageWidth->SetMinDesiredWidth(560.f);
	MessageWidth->SetContent(MessageText);
	TerminusUI::AddRow(Column, MessageWidth, 10.f);

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	TerminusUI::AddRow(Column, CardBox, 24.f);

	const TSubclassOf<UItemPickPanelWidget> PickClass = PickPanelClass ? PickPanelClass : TSubclassOf<UItemPickPanelWidget>(UItemPickPanelWidget::StaticClass());
	PickPanel = WidgetTree->ConstructWidget<UItemPickPanelWidget>(PickClass, TEXT("PickPanel"));
	TerminusUI::AddRow(Column, PickPanel, 24.f);
}

void UEventWidget::Setup(const TArray<FEventOption>& InOptions)
{
	Options = InOptions;
	PendingIndex = INDEX_NONE;
	bChosen = false;

	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Options.Num() > 0
			? TEXT("하나를 고르세요.")
			: TEXT("아무 일도 일어나지 않았습니다.")));
	}
	if (PickPanel) PickPanel->Hide();

	RebuildCards();
}

void UEventWidget::RebuildCards()
{
	if (!CardBox) return;

	CardBox->ClearChildren();
	Relays.Reset();
	CardBox->SetVisibility(PendingIndex == INDEX_NONE ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	for (int32 i = 0; i < Options.Num(); ++i)
	{
		const FEventOption& Option = Options[i];

		UButton* Card = WidgetTree->ConstructWidget<UButton>();
		Card->SetBackgroundColor(FLinearColor(0.32f, 0.28f, 0.38f));
		Card->SetIsEnabled(!bChosen);

		UTerminusClickRelay::Bind(Card, this, FSimpleDelegate::CreateUObject(this, &UEventWidget::HandleCardClicked, i), Relays);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y);
		Card->SetContent(Size);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
		Size->SetContent(Inside);
		Inside->AddChildToVerticalBox(TerminusUI::MakeWrapText(WidgetTree, NAME_None, Option.Title.ToString(), 21, FLinearColor::White));

		UTextBlock* Desc = TerminusUI::MakeWrapText(WidgetTree, NAME_None, Option.Description.ToString(), 14, TerminusUI::Color::TextBright);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(Desc))
		{
			S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(CardBox->AddChild(Card)))
		{
			S->SetPadding(FMargin(10.f, 0.f));
		}
	}
}

void UEventWidget::HandleCardClicked(int32 Index)
{
	if (bChosen || !Options.IsValidIndex(Index)) return;

	const FEventOption& Option = Options[Index];
	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;

	// 스킬인데 강화 칸이 꽉 찼으면 바꿀 칸부터 / 확정 유물 변경이면 바꿀 유물부터
	const bool bNeedSkillSlot = IsEventSkillOption(Option.Type) && PS && PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount();
	if (bNeedSkillSlot || Option.Type == EEventOptionType::RelicSwapChosen)
	{
		PendingIndex = Index;
		ShowPick(Option);
		RebuildCards();
		return;
	}

	Send(Index, INDEX_NONE, NAME_None);
}

void UEventWidget::ShowPick(const FEventOption& Option)
{
	if (!PickPanel) return;

	if (Option.Type == EEventOptionType::RelicSwapChosen)
	{
		// 기본 / 업그레이드 유물은 바꿀 수 없음
		PickPanel->OnPicked.BindUObject(this, &UEventWidget::HandleRelicPicked);
		PickPanel->ShowRelics(FText::FromString(TEXT("바꿀 유물을 고르세요. 같은 계층의 다른 유물로 바뀝니다.")),
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
	if (MessageText) MessageText->SetText(FText::FromString(TEXT("...")));
	RebuildCards();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseEvent(Index, ReplaceSlot, RelicRow);
	}
}

void UEventWidget::ShowResult(const FText& Result)
{
	if (CardBox) CardBox->SetVisibility(ESlateVisibility::Collapsed);
	if (MessageText) MessageText->SetText(Result);
}
