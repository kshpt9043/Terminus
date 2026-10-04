#include "Widgets/Common/ItemSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Engine/Texture2D.h"
#include "Widgets/Skill/SkillTooltipWidget.h"

namespace
{
	FLinearColor ItemSlotRelicColor(ERelicTier Tier)
	{
		switch (Tier)
		{
		case ERelicTier::Basic:   return FLinearColor(0.55f, 0.55f, 0.55f);
		case ERelicTier::Upgrade: return FLinearColor(1.f, 0.8f, 0.3f);
		case ERelicTier::Mid:     return FLinearColor(0.35f, 0.6f, 1.f);
		case ERelicTier::Deep:    return FLinearColor(0.95f, 0.3f, 0.3f);
		default:                  return FLinearColor(0.85f, 0.85f, 0.85f);
		}
	}

	FLinearColor ItemSlotSkillColor(ESkillType Type)
	{
		switch (Type)
		{
		case ESkillType::Attack:  return FLinearColor(0.9f, 0.35f, 0.3f);
		case ESkillType::Defense: return FLinearColor(0.35f, 0.6f, 0.95f);
		default:                  return FLinearColor(0.7f, 0.45f, 0.95f);
		}
	}

	FString ItemSlotTierLabel(ERelicTier Tier)
	{
		switch (Tier)
		{
		case ERelicTier::Basic:   return TEXT("기본");
		case ERelicTier::Upgrade: return TEXT("업그레이드");
		case ERelicTier::Mid:     return TEXT("중층");
		case ERelicTier::Deep:    return TEXT("심층");
		default:                  return TEXT("표층");
		}
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UItemSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (SlotButton)
	{
		SlotButton->OnClicked.AddDynamic(this, &UItemSlotWidget::HandleClicked);
	}

	if (!bShownOnce)
	{
		SetEmpty();
	}
}

void UItemSlotWidget::BuildDefaultLayout()
{
	SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSizeBox"));
	SlotSizeBox->SetWidthOverride(DefaultSlotSize.X);
	SlotSizeBox->SetHeightOverride(DefaultSlotSize.Y);
	WidgetTree->RootWidget = SlotSizeBox;

	SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	SlotButton->SetBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	SlotSizeBox->SetContent(SlotButton);

	Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetPadding(FMargin(2.f));
	SlotButton->SetContent(Frame);

	UBorder* Inner = WidgetTree->ConstructWidget<UBorder>();
	Inner->SetBrushColor(FLinearColor(0.07f, 0.06f, 0.06f, 0.95f));
	Inner->SetPadding(FMargin(4.f));
	Inner->SetHorizontalAlignment(HAlign_Fill);
	Inner->SetVerticalAlignment(VAlign_Center);
	Frame->SetContent(Inner);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Inner->SetContent(Column);

	// 아이콘 자리: 이미지 위에 대체 글자
	UOverlay* IconArea = WidgetTree->ConstructWidget<UOverlay>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(IconArea))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	if (UOverlaySlot* S = IconArea->AddChildToOverlay(Icon))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	IconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IconText"));
	FSlateFontInfo IconFont = IconText->GetFont();
	IconFont.Size = 15;
	IconText->SetFont(IconFont);
	if (UOverlaySlot* S = IconArea->AddChildToOverlay(IconText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 10;
	NameText->SetFont(NameFont);
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(NameText))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
}

void UItemSlotWidget::SetShowName(bool bShow)
{
	if (NameText)
	{
		NameText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UItemSlotWidget::SetSlotSize(FVector2D Size)
{
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(Size.X);
		SlotSizeBox->SetHeightOverride(Size.Y);
	}
}

// =====================================================================
// 내용
// =====================================================================

void UItemSlotWidget::ShowIcon(UTexture2D* Texture, const FText& Name)
{
	if (Icon)
	{
		if (Texture) Icon->SetBrushFromTexture(Texture);
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IconText)
	{
		IconText->SetText(FText::FromString(Name.ToString().Left(2)));
		IconText->SetVisibility(!Texture && !Name.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (NameText)
	{
		NameText->SetText(Name);
	}
}

void UItemSlotWidget::SetEmpty()
{
	if (bShownOnce && Kind == EItemSlotKind::Empty) return;
	bShownOnce = true;

	Kind = EItemSlotKind::Empty;
	ItemRow = NAME_None;
	ItemColor = EmptyColor;
	ShowIcon(nullptr, FText::GetEmpty());
	SetToolTip(nullptr);
	ApplyFrameColor();
	ApplyButtonEnabled();
}

void UItemSlotWidget::SetSkill(FName SkillRow)
{
	if (bShownOnce && Kind == EItemSlotKind::Skill && ItemRow == SkillRow) return;

	const FSkillRow* Skill = SkillRow.IsNone() ? nullptr : UTerminusDataSettings::FindSkillRow(SkillRow);
	if (!Skill)
	{
		SetEmpty();
		return;
	}
	bShownOnce = true;

	Kind = EItemSlotKind::Skill;
	ItemRow = SkillRow;
	ItemColor = ItemSlotSkillColor(Skill->SkillType);
	ShowIcon(UTerminusDataSettings::FindSkillIcon(*Skill), Skill->DisplayName_KR);

	// 스킬 툴팁 (칸마다 하나 만들어 두고 내용만 바꿈)
	if (!SkillTooltip)
	{
		const TSubclassOf<USkillTooltipWidget> Class = SkillTooltipClass ? SkillTooltipClass : TSubclassOf<USkillTooltipWidget>(USkillTooltipWidget::StaticClass());
		SkillTooltip = CreateWidget<USkillTooltipWidget>(this, Class);
	}
	if (SkillTooltip)
	{
		SkillTooltip->SetSkill(Skill);
		SetToolTip(SkillTooltip);
	}

	ApplyFrameColor();
	ApplyButtonEnabled();
}

void UItemSlotWidget::SetRelic(FName RelicRow)
{
	if (bShownOnce && Kind == EItemSlotKind::Relic && ItemRow == RelicRow) return;

	const FRelicRow* Relic = RelicRow.IsNone() ? nullptr : UTerminusDataSettings::FindRelicRow(RelicRow);
	if (!Relic)
	{
		SetEmpty();
		return;
	}
	bShownOnce = true;

	Kind = EItemSlotKind::Relic;
	ItemRow = RelicRow;
	ItemColor = ItemSlotRelicColor(Relic->RelicTier);
	ShowIcon(Relic->Icon.LoadSynchronous(), Relic->RelicName);

	SetToolTip(nullptr);
	SetToolTipText(FText::FromString(FString::Printf(TEXT("%s  [%s]\n%s"),
		*Relic->RelicName.ToString(), *ItemSlotTierLabel(Relic->RelicTier), *Relic->RelicDesc.ToString())));

	ApplyFrameColor();
	ApplyButtonEnabled();
}

// =====================================================================
// 상태
// =====================================================================

void UItemSlotWidget::SetSelected(bool bSelected)
{
	if (bSelectedState == bSelected) return;
	bSelectedState = bSelected;
	ApplyFrameColor();
}

void UItemSlotWidget::SetUsable(bool bUsable)
{
	if (bUsableState == bUsable) return;
	bUsableState = bUsable;
	ApplyButtonEnabled();
}

void UItemSlotWidget::ApplyFrameColor()
{
	if (Frame)
	{
		Frame->SetBrushColor(bSelectedState ? SelectedColor : ItemColor);
	}
}

void UItemSlotWidget::ApplyButtonEnabled()
{
	if (SlotButton)
	{
		SlotButton->SetIsEnabled(Kind != EItemSlotKind::Empty && bUsableState);
	}
}

void UItemSlotWidget::HandleClicked()
{
	OnSlotClicked.ExecuteIfBound(this);
}
