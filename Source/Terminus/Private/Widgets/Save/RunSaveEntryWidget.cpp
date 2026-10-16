#include "Widgets/Save/RunSaveEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void URunSaveEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (LoadButton)   LoadButton->OnClicked.AddDynamic(this, &URunSaveEntryWidget::HandleLoad);
	if (DeleteButton) DeleteButton->OnClicked.AddDynamic(this, &URunSaveEntryWidget::HandleDelete);
}

void URunSaveEntryWidget::BuildDefaultLayout()
{
	UBorder* Row = TerminusUI::MakePanel(WidgetTree, TEXT("EntryRow"), TerminusUI::Color::Card, FMargin(14.f, 10.f));
	WidgetTree->RootWidget = Row;

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->SetContent(Line);

	// 왼쪽: 글
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Info))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	TitleText = TerminusUI::MakeText(WidgetTree, TEXT("TitleText"), TEXT(""), TerminusUI::FontSize::Subtitle, TerminusUI::Color::Gold, ETextJustify::Left);
	TerminusUI::SetBold(TitleText);
	Info->AddChildToVerticalBox(TitleText);

	InfoText = TerminusUI::MakeText(WidgetTree, TEXT("InfoText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextSub, ETextJustify::Left);
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(InfoText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	PlayersText = TerminusUI::MakeText(WidgetTree, TEXT("PlayersText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextBody, ETextJustify::Left);
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(PlayersText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	DateText = TerminusUI::MakeText(WidgetTree, TEXT("DateText"), TEXT(""), TerminusUI::FontSize::Aux, TerminusUI::Color::TextFaint, ETextJustify::Left);
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(DateText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// 오른쪽: 버튼
	// 불러오기 = 보조 행동 육각 버튼, 삭제 = 평평한 버튼 + 경고색 글자
	LoadButton = TerminusUI::MakeHexButton(WidgetTree, TEXT("LoadButton"), TEXT("불러오기"), TerminusUI::EHexButton::Secondary, TerminusUI::FontSize::Body);
	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(LoadButton))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(12.f, 0.f, 6.f, 0.f));
	}

	DeleteButton = TerminusUI::MakeButton(WidgetTree, TEXT("DeleteButton"), TEXT("삭제"), TerminusUI::FontSize::Body, TerminusUI::Color::Alert);
	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(DeleteButton))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
}

void URunSaveEntryWidget::Setup(const FRunSaveSummary& InSummary)
{
	SlotName = InSummary.SlotName;

	if (TitleText)   TitleText->SetText(FText::FromString(InSummary.RoomName.IsEmpty() ? TEXT("이름 없는 던전") : InSummary.RoomName));
	if (InfoText)    InfoText->SetText(UTerminusSaveSubsystem::DescribeTitle(InSummary));
	if (PlayersText) PlayersText->SetText(UTerminusSaveSubsystem::DescribePlayers(InSummary));
	if (DateText)    DateText->SetText(FText::FromString(InSummary.SavedAt.ToString(TEXT("%Y-%m-%d %H:%M"))));
}

void URunSaveEntryWidget::HandleLoad()
{
	OnLoadClicked.ExecuteIfBound(SlotName);
}

void URunSaveEntryWidget::HandleDelete()
{
	OnDeleteClicked.ExecuteIfBound(SlotName);
}
