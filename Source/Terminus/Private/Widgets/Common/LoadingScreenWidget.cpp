#include "Widgets/Common/LoadingScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CircularThrobber.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* MakeLoadingText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}
}

void ULoadingScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	// 아래 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);

	if (!PendingMessage.IsEmpty())
	{
		SetMessage(PendingMessage);
	}
}

void ULoadingScreenWidget::BuildDefaultLayout()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LoadingBackground"));
	Background->SetBrushColor(FLinearColor(0.03f, 0.028f, 0.025f, 1.f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Column);

	TitleText = MakeLoadingText(WidgetTree, TEXT("TitleText"), TEXT("불러오는 중"), 28, FLinearColor(1.f, 0.85f, 0.35f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	UCircularThrobber* Throbber = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("Throbber"));
	Throbber->SetRadius(28.f);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Throbber))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 24.f));
	}

	MessageText = MakeLoadingText(WidgetTree, TEXT("MessageText"), TEXT(""), 16, FLinearColor(0.85f, 0.85f, 0.85f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText)) S->SetHorizontalAlignment(HAlign_Center);
}

void ULoadingScreenWidget::SetMessage(const FText& InMessage)
{
	PendingMessage = InMessage;

	if (MessageText)
	{
		MessageText->SetText(InMessage);
	}
	OnMessageChanged(InMessage);
}

void ULoadingSpinnerWidget::BuildDefaultLayout()
{
	// 화면을 살짝 블러 -> 그 위를 어둡게 반투명 -> 가운데 로딩 아이콘만
	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("LoadingBlur"));
	Blur->SetBlurStrength(BlurStrength);
	Blur->SetPadding(FMargin(0.f));
	WidgetTree->RootWidget = Blur;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LoadingDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, DimOpacity));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	Blur->SetContent(Dim);

	UCircularThrobber* Throbber = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("Throbber"));
	Throbber->SetRadius(28.f);
	Dim->SetContent(Throbber);
}

FReply ULoadingScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}
