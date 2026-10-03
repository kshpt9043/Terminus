#include "Widgets/Skill/StartSkillPickWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/TerminusPlayerController.h"
#include "Widgets/Skill/SkillCardWidget.h"

namespace
{
	UTextBlock* MakeCenteredText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
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

void UStartSkillPickWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	// 아래 지도를 못 누르게 화면 전체가 클릭을 받음. 아무 키 입력을 받으려면 포커스도 받아야 함
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UStartSkillPickWidget::BuildDefaultLayout()
{
	// 화면 전체를 어둡게 덮는 바탕 + 가운데 제목 / 안내 / 카드 줄
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PickRoot"));
	Root->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
	Root->SetHorizontalAlignment(HAlign_Center);
	Root->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Column);

	TitleText = MakeCenteredText(WidgetTree, TEXT("TitleText"), 30, FLinearColor::White);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	MessageText = MakeCenteredText(WidgetTree, TEXT("MessageText"), 18, FLinearColor(0.8f, 0.8f, 0.8f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CardBox))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 36.f, 0.f, 0.f));
	}
}

void UStartSkillPickWidget::SetChoices(const TArray<FName>& SkillRows)
{
	bNoSkills = SkillRows.Num() == 0;
	ShownTime = FPlatformTime::Seconds();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(
			bNoSkills ? TEXT("보유한 스킬이 없습니다")
			: SkillRows.Num() > 1 ? TEXT("강화 스킬을 하나 골라 장착하세요")
			: TEXT("강화 스킬을 장착하세요")));
	}

	if (MessageText)
	{
		MessageText->SetText(FText::FromString(TEXT("아무 키나 누르면 지도로 이동합니다")));
		MessageText->SetVisibility(bNoSkills ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!CardBox) return;
	CardBox->ClearChildren();
	CardBox->SetVisibility(bNoSkills ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	const TSubclassOf<USkillCardWidget> Class = CardClass ? CardClass : TSubclassOf<USkillCardWidget>(USkillCardWidget::StaticClass());

	for (int32 i = 0; i < SkillRows.Num(); ++i)
	{
		USkillCardWidget* Card = CreateWidget<USkillCardWidget>(this, Class);
		if (!Card) continue;

		Card->SetSkill(SkillRows[i]);
		Card->OnCardClicked.BindUObject(this, &UStartSkillPickWidget::HandleCardClicked);

		UPanelSlot* CardSlot = CardBox->AddChild(Card);
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(CardSlot))
		{
			HSlot->SetPadding(FMargin(i == 0 ? 0.f : CardSpacing * 0.5f, 0.f, i == SkillRows.Num() - 1 ? 0.f : CardSpacing * 0.5f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

FReply UStartSkillPickWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return TrySkip() ? FReply::Handled() : Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UStartSkillPickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 카드 화면에선 바탕 클릭을 그냥 먹기만 함 (아래 지도로 안 새게)
	TrySkip();
	return FReply::Handled();
}

bool UStartSkillPickWidget::TrySkip()
{
	if (!bNoSkills || bChosen) return false;
	if (FPlatformTime::Seconds() - ShownTime < AnyKeyDelay) return true;   // 먹기만 하고 아직 안 넘김

	Finish(NAME_None);
	return true;
}

void UStartSkillPickWidget::HandleCardClicked(FName SkillRow)
{
	Finish(SkillRow);
}

void UStartSkillPickWidget::Finish(FName SkillRow)
{
	if (bChosen) return;
	bChosen = true;

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseStartSkill(SkillRow);
	}

	RemoveFromParent();
}
