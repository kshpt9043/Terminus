#include "Widgets/Map/FloorTitleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "TimerManager.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

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
	UBorder* Dim = TerminusUI::MakeDimRoot(WidgetTree, TEXT("FloorTitleDim"), TerminusUI::Color::DimStrong);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = TerminusUI::MakeText(WidgetTree, TEXT("TitleText"), TEXT(""), 54, FLinearColor::White);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	SubtitleText = TerminusUI::MakeText(WidgetTree, TEXT("SubtitleText"), TEXT(""), 22, TerminusUI::Color::Gold);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(SubtitleText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	NoteText = TerminusUI::MakeText(WidgetTree, TEXT("NoteText"), TEXT(""), 15, FLinearColor(0.75f, 0.9f, 0.75f));
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
