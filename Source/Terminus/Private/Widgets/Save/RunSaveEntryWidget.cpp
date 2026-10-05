#include "Widgets/Save/RunSaveEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* MakeSaveEntryText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UButton* MakeSaveEntryButton(UWidgetTree* Tree, const FName& Name, const FString& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeSaveEntryText(Tree, NAME_None, 15, FLinearColor::Black);
		Text->SetText(FText::FromString(Label));
		Button->SetContent(Text);
		return Button;
	}
}

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
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EntryRow"));
	Row->SetBrushColor(FLinearColor(0.16f, 0.15f, 0.13f, 1.f));
	Row->SetPadding(FMargin(14.f, 10.f));
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

	TitleText = MakeSaveEntryText(WidgetTree, TEXT("TitleText"), 18, FLinearColor(1.f, 0.85f, 0.35f));
	Info->AddChildToVerticalBox(TitleText);

	InfoText = MakeSaveEntryText(WidgetTree, TEXT("InfoText"), 14, FLinearColor(0.85f, 0.85f, 0.85f));
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(InfoText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	PlayersText = MakeSaveEntryText(WidgetTree, TEXT("PlayersText"), 14, FLinearColor::White);
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(PlayersText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	DateText = MakeSaveEntryText(WidgetTree, TEXT("DateText"), 12, FLinearColor(0.6f, 0.6f, 0.6f));
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(DateText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// 오른쪽: 버튼
	LoadButton = MakeSaveEntryButton(WidgetTree, TEXT("LoadButton"), TEXT("  불러오기  "));
	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(LoadButton))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(12.f, 0.f, 6.f, 0.f));
	}

	DeleteButton = MakeSaveEntryButton(WidgetTree, TEXT("DeleteButton"), TEXT("  삭제  "));
	DeleteButton->SetBackgroundColor(FLinearColor(0.75f, 0.4f, 0.35f));
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
