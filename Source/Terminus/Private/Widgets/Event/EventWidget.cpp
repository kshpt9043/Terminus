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
#include "Widgets/Common/ItemSlotWidget.h"

namespace
{
	UTextBlock* MakeEventText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddEventCentered(UVerticalBox* Column, UWidget* Widget, float Top)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Widget))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
	}

	bool IsEventSkillOption(EEventOptionType Type)
	{
		return Type == EEventOptionType::ClassSkill || Type == EEventOptionType::OtherClassSkill || Type == EEventOptionType::EventSkill;
	}

	const FLinearColor EventPurple(0.8f, 0.65f, 1.f);
}

void UEventClickRelay::HandleClicked()
{
	if (UEventWidget* Widget = Owner.Get())
	{
		Widget->HandleCardClicked(Index);
	}
}

void UEventWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	if (PickCancelButton) PickCancelButton->OnClicked.AddDynamic(this, &UEventWidget::HandlePickCancel);

	// 아래 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UEventWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EventDim"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.f, 0.04f, 0.7f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeEventText(WidgetTree, TEXT("TitleText"), TEXT("이벤트"), 32, EventPurple);
	AddEventCentered(Column, TitleText, 0.f);

	// 카드가 접혀도(결과만 보일 때) 글이 좁게 줄바꿈되지 않게 최소 폭을 줌
	MessageText = MakeEventText(WidgetTree, TEXT("MessageText"), TEXT(""), 16, FLinearColor(0.9f, 0.9f, 0.9f));
	USizeBox* MessageWidth = WidgetTree->ConstructWidget<USizeBox>();
	MessageWidth->SetMinDesiredWidth(560.f);
	MessageWidth->SetContent(MessageText);
	AddEventCentered(Column, MessageWidth, 10.f);

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	AddEventCentered(Column, CardBox, 24.f);

	UVerticalBox* Pick = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PickPanel"));
	PickPanel = Pick;
	AddEventCentered(Column, Pick, 24.f);

	PickGuideText = MakeEventText(WidgetTree, TEXT("PickGuideText"), TEXT(""), 16, FLinearColor(0.85f, 0.85f, 0.85f));
	AddEventCentered(Pick, PickGuideText, 0.f);

	PickBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PickBox"));
	AddEventCentered(Pick, PickBox, 12.f);

	PickCancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PickCancelButton"));
	PickCancelButton->SetContent(MakeEventText(WidgetTree, NAME_None, TEXT("  취소  "), 16, FLinearColor::Black));
	AddEventCentered(Pick, PickCancelButton, 12.f);
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
	if (PickPanel) PickPanel->SetVisibility(ESlateVisibility::Collapsed);

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

		UEventClickRelay* Relay = NewObject<UEventClickRelay>(this);
		Relay->Owner = this;
		Relay->Index = i;
		Card->OnClicked.AddDynamic(Relay, &UEventClickRelay::HandleClicked);
		Relays.Add(Relay);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y);
		Card->SetContent(Size);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
		Size->SetContent(Inside);
		Inside->AddChildToVerticalBox(MakeEventText(WidgetTree, NAME_None, Option.Title.ToString(), 21, FLinearColor::White));

		UTextBlock* Desc = MakeEventText(WidgetTree, NAME_None, Option.Description.ToString(), 14, FLinearColor(0.9f, 0.9f, 0.9f));
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
	if (!PickPanel || !PickBox) return;

	PickPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PickBox->ClearChildren();

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	if (!PS) return;

	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());

	if (Option.Type == EEventOptionType::RelicSwapChosen)
	{
		if (PickGuideText) PickGuideText->SetText(FText::FromString(TEXT("바꿀 유물을 고르세요. 같은 계층의 다른 유물로 바뀝니다.")));

		for (const FName& Owned : PS->GetRelics())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Owned);
			if (!Relic || Relic->RelicTier == ERelicTier::Basic || Relic->RelicTier == ERelicTier::Upgrade) continue;

			if (UItemSlotWidget* RelicSlot = CreateWidget<UItemSlotWidget>(this, Class))
			{
				RelicSlot->SetShowName(true);
				RelicSlot->SetRelic(Owned);
				RelicSlot->OnSlotClicked.BindUObject(this, &UEventWidget::HandleRelicPicked);
				if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(PickBox->AddChild(RelicSlot))) S->SetPadding(FMargin(6.f, 0.f));
			}
		}
		return;
	}

	if (PickGuideText) PickGuideText->SetText(FText::FromString(TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 스킬을 고르세요.")));

	const TArray<FName>& Equipped = PS->GetRunState().EnhanceSkills;
	for (int32 i = 0; i < Equipped.Num(); ++i)
	{
		if (UItemSlotWidget* SkillSlot = CreateWidget<UItemSlotWidget>(this, Class))
		{
			SkillSlot->SetSlotIndex(i);
			SkillSlot->SetSkill(Equipped[i]);
			SkillSlot->OnSlotClicked.BindUObject(this, &UEventWidget::HandleSkillSlotPicked);
			if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(PickBox->AddChild(SkillSlot))) S->SetPadding(FMargin(6.f, 0.f));
		}
	}
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
	if (PickPanel) PickPanel->SetVisibility(ESlateVisibility::Collapsed);
	RebuildCards();
}

void UEventWidget::Send(int32 Index, int32 ReplaceSlot, FName RelicRow)
{
	bChosen = true;
	PendingIndex = INDEX_NONE;
	if (PickPanel) PickPanel->SetVisibility(ESlateVisibility::Collapsed);
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
