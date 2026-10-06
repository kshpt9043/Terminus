#include "Widgets/Map/FloorTitleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "TimerManager.h"

namespace
{
	UTextBlock* MakeFloorTitleText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}
}

void UFloorTitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	// 보여주는 동안 아래 지도를 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UFloorTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { RemoveFromParent(); }), DisplaySeconds, false);
	}
}

void UFloorTitleWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FloorTitleDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeFloorTitleText(WidgetTree, TEXT("TitleText"), 54, FLinearColor::White);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	SubtitleText = MakeFloorTitleText(WidgetTree, TEXT("SubtitleText"), 22, FLinearColor(1.f, 0.85f, 0.35f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(SubtitleText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	NoteText = MakeFloorTitleText(WidgetTree, TEXT("NoteText"), 15, FLinearColor(0.75f, 0.9f, 0.75f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(NoteText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}
}

void UFloorTitleWidget::Setup(const FText& InTitle, const FText& InSubtitle, const FText& InNote)
{
	if (TitleText)    TitleText->SetText(InTitle);
	if (SubtitleText) SubtitleText->SetText(InSubtitle);
	if (NoteText)     NoteText->SetText(InNote);
}
