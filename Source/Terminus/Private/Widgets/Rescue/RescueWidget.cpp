#include "Widgets/Rescue/RescueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/TerminusPlayerController.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

namespace
{
	// 제목 + 설명 두 줄짜리 선택지 버튼. 설명 글자는 OutDesc 로
	UButton* MakeRescueOption(UWidgetTree* Tree, const FName& Name, const FString& Title, UTextBlock*& OutDesc)
	{
		// 카드처럼 생긴 평평한 버튼 (여백은 안쪽 줄에서)
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = TerminusUI::MakeFlatButtonStyle(TerminusUI::Color::Card);
		Style.NormalPadding = FMargin(4.f, 2.f);
		Style.PressedPadding = FMargin(4.f, 3.f, 4.f, 1.f);
		Button->SetStyle(Style);

		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(520.f);
		Size->SetMinDesiredHeight(74.f);
		Button->SetContent(Size);

		UVerticalBox* Inside = Tree->ConstructWidget<UVerticalBox>();
		Size->SetContent(Inside);
		UTextBlock* TitleLine = TerminusUI::MakeWrapText(Tree, NAME_None, Title, TerminusUI::FontSize::Subtitle, TerminusUI::Color::Gold, ETextJustify::Left);
		TerminusUI::SetBold(TitleLine);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(TitleLine))
		{
			S->SetPadding(FMargin(14.f, 6.f, 14.f, 0.f));
		}
		OutDesc = TerminusUI::MakeWrapText(Tree, NAME_None, TEXT(""), 14, TerminusUI::Color::TextBright, ETextJustify::Left);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(OutDesc))
		{
			S->SetPadding(FMargin(14.f, 2.f, 14.f, 6.f));
		}
		return Button;
	}
}

void URescueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !OptionBox)
	{
		BuildDefaultLayout();
	}

	if (RescueButton) RescueButton->OnClicked.AddDynamic(this, &URescueWidget::HandleRescue);
	if (InterveneButton) InterveneButton->OnClicked.AddDynamic(this, &URescueWidget::HandleIntervene);

	SetVisibility(ESlateVisibility::Visible);
}

void URescueWidget::BuildDefaultLayout()
{
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("RescueDim"), TerminusUI::Color::Dim);

	UBorder* Panel = TerminusUI::MakePanel(WidgetTree, NAME_None, TerminusUI::Color::Board, FMargin(30.f, 24.f));
	Dim->SetContent(Panel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	TitleText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TitleText"), TEXT("동료 전멸"), TerminusUI::FontSize::Section, TerminusUI::Color::Warning);
	TerminusUI::SetBold(TitleText);
	Column->AddChildToVerticalBox(TitleText);

	MessageText = TerminusUI::MakeWrapText(WidgetTree, TEXT("MessageText"), TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextBody);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	TimerText = TerminusUI::MakeWrapText(WidgetTree, TEXT("TimerText"), TEXT(""), TerminusUI::FontSize::Body, TerminusUI::Color::TextMuted);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TimerText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UVerticalBox* Options = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OptionBox"));
	OptionBox = Options;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Options)) S->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));

	UTextBlock* Desc = nullptr;
	RescueButton = MakeRescueOption(WidgetTree, TEXT("RescueButton"), TEXT("구출"), Desc);
	RescueDescText = Desc;
	if (UVerticalBoxSlot* S = Options->AddChildToVerticalBox(RescueButton)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	InterveneButton = MakeRescueOption(WidgetTree, TEXT("InterveneButton"), TEXT("난입"), Desc);
	InterveneDescText = Desc;
	Options->AddChildToVerticalBox(InterveneButton);
}

void URescueWidget::Setup(bool bInChooser, const TArray<FString>& WipedNames, bool bInCanIntervene, float InHealthCost, float Seconds)
{
	bChooser = bInChooser;
	bCanIntervene = bInCanIntervene;
	bChosen = false;
	TimeLeft = Seconds;

	const FString Names = FString::Join(WipedNames, TEXT(", "));
	const int32 CostPercent = FMath::RoundToInt(InHealthCost * 100.f);

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bChooser ? TEXT("동료 전멸") : TEXT("전멸")));
	}

	if (MessageText)
	{
		MessageText->SetText(FText::FromString(bChooser
			? FString::Printf(TEXT("%s 님이 쓰러졌습니다. 어떻게 할까요?"), *Names)
			: FString(TEXT("쓰러졌습니다...\n다른 방의 동료가 구출 / 난입을 고르는 중입니다."))));
	}

	if (RescueDescText)
	{
		RescueDescText->SetText(FText::FromString(FString::Printf(
			TEXT("내 현재 체력의 %d%% 를 잃고, 쓰러진 동료를 체력 1 로 일으킵니다. 다음 방은 동료가 쓰러진 방에서 고릅니다. (보상 없음)"), CostPercent)));
	}
	if (InterveneDescText)
	{
		InterveneDescText->SetText(FText::FromString(bCanIntervene
			? FString(TEXT("전멸한 방에 들어가 남은 몬스터와 싸웁니다. 몬스터 체력은 그대로, 이기면 보상을 받고 동료는 체력 1 로 일어납니다. 다음 방은 그 방에서 고릅니다."))
			: FString(TEXT("전멸한 방이 여러 곳이라 난입할 수 없습니다."))));
	}

	if (OptionBox) OptionBox->SetVisibility(bChooser ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (RescueButton) RescueButton->SetIsEnabled(true);
	if (InterveneButton) InterveneButton->SetIsEnabled(bCanIntervene);
}

void URescueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeLeft = FMath::Max(0.f, TimeLeft - InDeltaTime);
	if (TimerText)
	{
		TerminusUI::SetTextIfChanged(TimerText, FString::Printf(TEXT("남은 시간 %d초"), FMath::CeilToInt(TimeLeft)));   // 초가 바뀔 때만
	}
}

void URescueWidget::Choose(bool bIntervene)
{
	if (!bChooser || bChosen) return;
	if (bIntervene && !bCanIntervene) return;

	bChosen = true;
	if (RescueButton) RescueButton->SetIsEnabled(false);
	if (InterveneButton) InterveneButton->SetIsEnabled(false);
	if (MessageText)
	{
		MessageText->SetText(FText::FromString(FString::Printf(TEXT("%s 을(를) 골랐습니다. 다른 동료를 기다리는 중..."), bIntervene ? TEXT("난입") : TEXT("구출"))));
	}

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseRescue(bIntervene);
	}
}

void URescueWidget::HandleRescue()
{
	Choose(false);
}

void URescueWidget::HandleIntervene()
{
	Choose(true);
}
