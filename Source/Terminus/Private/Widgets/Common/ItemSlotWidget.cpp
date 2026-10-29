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
#include "Widgets/Relic/RelicTooltipWidget.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/Common/TerminusUIText.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUISettings.h"

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

	// WBP 판 색을 따름. 단 예전 C++ 기본색 그대로 복사된 WBP 면 새 팔레트(#262A32) 사용
	if (Background && !Background->GetBrushColor().Equals(FLinearColor(0.07f, 0.06f, 0.06f, 0.95f), 0.005f))
	{
		BackgroundColor = Background->GetBrushColor();
	}

	// 유물 / 스킬 전환 때 되돌릴 원래 값
	if (SlotSizeBox) RequestedSize = FVector2D(SlotSizeBox->GetWidthOverride(), SlotSizeBox->GetHeightOverride());
	if (NameText) OrigNameFontSize = NameText->GetFont().Size;
	if (Icon) OrigIconSize = Icon->GetBrush().GetImageSize();

	ApplyIconOnly();

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
	Frame->SetPadding(FMargin(1.f, 5.f, 1.f, 1.f));
	SlotButton->SetContent(Frame);

	UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background = Inner;
	Inner->SetBrushColor(BackgroundColor);
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
	bShowNameWanted = bShow;
	if (NameText)
	{
		NameText->SetVisibility(bShow && !bIconOnly ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyKindLayout();
}

void UItemSlotWidget::ApplyKindLayout()
{
	const bool bRelic = Kind == EItemSlotKind::Relic;

	const bool bNameShown = NameText && NameText->GetVisibility() != ESlateVisibility::Collapsed;
	if (NameText && OrigNameFontSize > 0.f)
	{
		FSlateFontInfo Font = NameText->GetFont();
		const float Want = bRelic ? float(RelicNameFontSize) : OrigNameFontSize;
		if (Font.Size != Want)
		{
			Font.Size = Want;
			NameText->SetFont(Font);
		}
	}

	FVector2D Size = RequestedSize;
	if (SlotSizeBox && !RequestedSize.IsZero())
	{
		if (bRelic && bNameShown) Size = RequestedSize.ComponentMax(RelicNamedMinSize);
		SlotSizeBox->SetWidthOverride(Size.X);
		SlotSizeBox->SetHeightOverride(Size.Y);
	}

	// 아이콘 크기: 유물은 칸에서 테두리 / 띠 / 이름 자리를 뺀 만큼 정사각형으로 크게, 스킬은 원래 크기
	if (Icon)
	{
		FVector2D IconSize = OrigIconSize;
		if (bRelic && !Size.IsZero())
		{
			const float Edge = FMath::Min(Size.X - 12.f, Size.Y - 17.f - (bNameShown ? RelicNameReserve : 0.f));
			IconSize = FVector2D(FMath::Max(16.f, FMath::FloorToFloat(Edge)));
		}
		// SetDesiredSizeOverride 는 아직 안 그려진 위젯엔 안 남아서 브러시 크기를 직접 바꿈
		if (Icon->GetBrush().GetImageSize() != IconSize)
		{
			FSlateBrush Brush = Icon->GetBrush();
			Brush.ImageSize = IconSize;
			Icon->SetBrush(Brush);
		}
	}
}

void UItemSlotWidget::SetIconOnly(bool bInIconOnly)
{
	bIconOnly = bInIconOnly;
	ApplyIconOnly();
}

void UItemSlotWidget::ApplyIconOnly()
{
	SetShowName(bShowNameWanted);
	ApplyFrameColor();
}

void UItemSlotWidget::SetSlotSize(FVector2D Size)
{
	RequestedSize = Size;
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(Size.X);
		SlotSizeBox->SetHeightOverride(Size.Y);
	}
	ApplyKindLayout();
}

// =====================================================================
// 내용
// =====================================================================

void UItemSlotWidget::ShowIcon(UTexture2D* Texture, const FText& Name)
{
	if (Icon)
	{
		if (Texture)
		{
			Icon->SetBrushFromTexture(Texture);
		}
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
	ApplyKindLayout();
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
	ItemColor = TerminusUI::SkillTypeColor(Skill->SkillType);
	ShowIcon(UTerminusDataSettings::FindSkillIcon(*Skill), Skill->DisplayName_KR);
	ApplyKindLayout();

	// 스킬 툴팁 (칸마다 하나 만들어 두고 내용만 바꿈)
	if (!SkillTooltip)
	{
		const TSubclassOf<USkillTooltipWidget> Class = UTerminusUISettings::Pick(SkillTooltipClass, &UTerminusUISettings::SkillTooltip);
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
	ItemColor = TerminusUI::RelicTierColor(Relic->RelicTier);
	ShowIcon(Relic->Icon.LoadSynchronous(), Relic->RelicName);
	ApplyKindLayout();

	// 유물 툴팁 (칸마다 하나 만들어 두고 내용만 바꿈)
	if (!RelicTooltip)
	{
		const TSubclassOf<URelicTooltipWidget> Class = UTerminusUISettings::Pick(RelicTooltipClass, &UTerminusUISettings::RelicTooltip);
		RelicTooltip = CreateWidget<URelicTooltipWidget>(this, Class);
	}
	if (RelicTooltip)
	{
		RelicTooltip->SetRelic(Relic);
		SetToolTip(RelicTooltip);
	}

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

void UItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHoveredState = true;
	ApplyFrameColor();
}

void UItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHoveredState = false;
	ApplyFrameColor();
}

void UItemSlotWidget::ApplyFrameColor()
{
	using namespace TerminusUI;

	const bool bEmpty = Kind == EItemSlotKind::Empty;
	const bool bHover = bHoveredState && !bEmpty && SlotButton && SlotButton->GetIsEnabled();

	if (Frame)
	{
		// 테두리 판의 바탕색이 위쪽 띠 (안쪽 판이 나머지를 덮음). 아이콘만 모드면 띠 없이 고를 때(강조) 테두리만
		const FLinearColor Band = bIconOnly ? FLinearColor::Transparent : (bEmpty ? EmptyColor : ItemColor);
		const FLinearColor Outline = bSelectedState ? SelectedColor
			: bIconOnly ? FLinearColor::Transparent
			: bHover ? Color::GoldPressed : Color::Line;
		const float Width = bSelectedState ? 2.f : 1.f;
		Frame->SetBrush(MakeBoxBrush(Band, Outline, 4.f, Width));
		Frame->SetPadding(FMargin(Width, 4.f + Width, Width, Width));
	}

	if (Background)
	{
		Background->SetBrushColor(bIconOnly ? FLinearColor::Transparent : bEmpty ? EmptyColor : bHover ? Color::Disabled : BackgroundColor);
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

bool UItemSlotWidget::IsPressed() const
{
	return SlotButton && SlotButton->GetIsEnabled() && SlotButton->IsPressed();
}
