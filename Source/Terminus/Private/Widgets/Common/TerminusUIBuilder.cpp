#include "Widgets/Common/TerminusUIBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace TerminusUI
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FText& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (!Initial.IsEmpty())
		{
			Text->SetText(Initial);
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
		return Text;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		return MakeText(Tree, Name, FText::FromString(Initial), Size, Color, Justify);
	}

	UTextBlock* MakeWrapText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		UTextBlock* Text = MakeText(Tree, Name, Initial, Size, Color, Justify);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddShadow(UTextBlock* Text, float Opacity)
	{
		if (!Text) return;
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, Opacity));
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName& Name, const FString& Label, int32 Size, const FLinearColor& TextColor, const FLinearColor& Background)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Background);
		Button->SetContent(MakeText(Tree, NAME_None, Label, Size, TextColor));
		return Button;
	}

	UVerticalBoxSlot* AddRow(UVerticalBox* Column, UWidget* Widget, float Top, EHorizontalAlignment Align)
	{
		UVerticalBoxSlot* Slot = Column ? Column->AddChildToVerticalBox(Widget) : nullptr;
		if (Slot)
		{
			Slot->SetHorizontalAlignment(Align);
			Slot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
		return Slot;
	}

	UBorder* MakeDimRoot(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color)
	{
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Dim->SetBrushColor(Color);
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);
		Tree->RootWidget = Dim;
		return Dim;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrushColor(Color);
		Panel->SetPadding(Padding);
		return Panel;
	}

	void SetTextIfChanged(UTextBlock* Text, const FText& Value)
	{
		if (Text && !Text->GetText().EqualTo(Value))
		{
			Text->SetText(Value);
		}
	}

	void SetTextIfChanged(UTextBlock* Text, const FString& Value)
	{
		if (Text && Text->GetText().ToString() != Value)
		{
			Text->SetText(FText::FromString(Value));
		}
	}
}
