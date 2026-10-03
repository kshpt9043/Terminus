#include "Widgets/Combat/SkillSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"
#include "Widgets/Skill/SkillTooltipWidget.h"

void USkillSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (SlotButton)
	{
		SlotButton->OnClicked.AddDynamic(this, &USkillSlotWidget::HandleClicked);
	}

	// 툴팁은 칸마다 하나 만들어 두고 스킬이 바뀌면 내용만 갈아끼움
	const TSubclassOf<USkillTooltipWidget> Class = TooltipClass ? TooltipClass : TSubclassOf<USkillTooltipWidget>(USkillTooltipWidget::StaticClass());
	Tooltip = CreateWidget<USkillTooltipWidget>(this, Class);
}

void USkillSlotWidget::BuildDefaultLayout()
{
	SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	WidgetTree->RootWidget = SlotButton;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotButton->SetContent(Column);

	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
	IconSize->SetWidthOverride(48.f);
	IconSize->SetHeightOverride(48.f);
	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	IconSize->SetContent(Icon);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(IconSize))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	FSlateFontInfo Font = NameText->GetFont();
	Font.Size = 14;
	NameText->SetFont(Font);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	NameText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(NameText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
}

void USkillSlotWidget::SetSkill(const FSkillRow* InSkill)
{
	if (bShownOnce && InSkill == ShownSkill) return;
	bShownOnce = true;
	ShownSkill = InSkill;

	if (NameText)
	{
		NameText->SetText(InSkill ? InSkill->DisplayName_KR : EmptyLabel);
	}

	if (Icon)
	{
		UTexture2D* Tex = InSkill ? UTerminusDataSettings::FindSkillIcon(*InSkill) : nullptr;
		if (Tex)
		{
			Icon->SetBrushFromTexture(Tex);
		}
		// Hidden 으로 자리는 남김 (아이콘 유무로 칸 크기가 달라지지 않게)
		Icon->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 툴팁은 스킬이 있을 때만
	if (Tooltip && InSkill)
	{
		Tooltip->SetSkill(InSkill);
	}
	SetToolTip(InSkill ? Tooltip.Get() : nullptr);

	// 빈 칸은 못 누름
	if (!InSkill)
	{
		SetUsable(false);
	}
}

void USkillSlotWidget::SetUsable(bool bInUsable)
{
	const bool bUsable = bInUsable && ShownSkill != nullptr;
	if (bShownUsable.IsSet() && bShownUsable.GetValue() == bUsable) return;
	bShownUsable = bUsable;

	if (SlotButton)
	{
		SlotButton->SetIsEnabled(bUsable);
	}
}

void USkillSlotWidget::SetPending(bool bInPending)
{
	if (bShownPending.IsSet() && bShownPending.GetValue() == bInPending) return;
	bShownPending = bInPending;

	if (SlotButton)
	{
		SlotButton->SetBackgroundColor(bInPending ? PendingColor : NormalColor);
	}
}

void USkillSlotWidget::HandleClicked()
{
	OnSlotClicked.ExecuteIfBound(SlotIndex);
}
