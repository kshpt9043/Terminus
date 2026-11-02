#include "Widgets/Common/ItemSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
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
	if (Background) OrigInnerPadding = Background->GetPadding();
	if (const UVerticalBoxSlot* S = NameText ? Cast<UVerticalBoxSlot>(NameText->Slot) : nullptr) OrigNamePadding = S->GetPadding();
	if (SlotButton)
	{
		OrigButtonPadding = SlotButton->GetStyle().NormalPadding;
		OrigButtonPressedPadding = SlotButton->GetStyle().PressedPadding;
		if (const UButtonSlot* S = Cast<UButtonSlot>(SlotButton->GetContentSlot())) OrigButtonSlotPadding = S->GetPadding();
	}

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
	// 유물 / 스킬은 기획 아이콘이 있어서 아이콘을 칸에 맞게 크게, 이름은 작게 (11-01 스킬 아이콘 추가)
	const bool bRelic = Kind == EItemSlotKind::Relic || Kind == EItemSlotKind::Skill;
	bBigIcon = bRelic;

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

	// 안쪽 여백: 유물 / 스킬은 거의 없앰 (이름 위 여백 포함)
	const FMargin InnerPadding = bRelic ? FMargin(IconPadding) : OrigInnerPadding;
	const FMargin NamePadding = bRelic ? FMargin(0.f, IconPadding, 0.f, 0.f) : OrigNamePadding;
	if (Background && !(Background->GetPadding() == InnerPadding))
	{
		Background->SetPadding(InnerPadding);
	}
	if (UVerticalBoxSlot* S = NameText ? Cast<UVerticalBoxSlot>(NameText->Slot) : nullptr)
	{
		if (!(S->GetPadding() == NamePadding)) S->SetPadding(NamePadding);
	}
	// 버튼 안쪽 여백도 (WBP_ItemSlot 은 버튼이 Frame 안에 있고 스타일 / 슬롯 여백이 있어서 아이콘 폭이 반쯤 깎였음)
	FMargin ButtonPadding(0.f);
	if (SlotButton)
	{
		const FMargin Normal = bRelic ? FMargin(0.f) : OrigButtonPadding;
		const FMargin Pressed = bRelic ? FMargin(0.f) : OrigButtonPressedPadding;
		const FMargin Content = bRelic ? FMargin(0.f) : OrigButtonSlotPadding;
		FButtonStyle Style = SlotButton->GetStyle();
		if (!(Style.NormalPadding == Normal) || !(Style.PressedPadding == Pressed))
		{
			Style.NormalPadding = Normal;
			Style.PressedPadding = Pressed;
			SlotButton->SetStyle(Style);
		}
		if (UButtonSlot* S = Cast<UButtonSlot>(SlotButton->GetContentSlot()); S && !(S->GetPadding() == Content))
		{
			S->SetPadding(Content);
		}
		ButtonPadding = Normal + Content;
	}
	// 아이콘 슬롯이 가로 Fill 이면 이름 폭만큼 늘어나 찌그러짐 -> 유물 / 스킬은 가운데
	if (UVerticalBoxSlot* S = Icon ? Cast<UVerticalBoxSlot>(Icon->Slot) : nullptr)
	{
		if (!bIconAlignSaved) { OrigIconHAlign = S->GetHorizontalAlignment(); bIconAlignSaved = true; }
		const EHorizontalAlignment Want = bRelic ? HAlign_Center : OrigIconHAlign.GetValue();
		if (S->GetHorizontalAlignment() != Want) S->SetHorizontalAlignment(Want);
	}

	// 아이콘 크기: 유물 / 스킬은 칸에서 테두리 / 띠 / 여백 / 이름 자리를 뺀 만큼 정사각형으로 크게
	if (Icon)
	{
		FVector2D IconSize = OrigIconSize;
		if (bRelic && !Size.IsZero())
		{
			const FMargin FramePad = Frame ? Frame->GetPadding() : FMargin(0.f);
			const float W = Size.X - FramePad.Left - FramePad.Right - InnerPadding.Left - InnerPadding.Right - ButtonPadding.Left - ButtonPadding.Right;
			const float H = Size.Y - FramePad.Top - FramePad.Bottom - InnerPadding.Top - InnerPadding.Bottom - ButtonPadding.Top - ButtonPadding.Bottom
				- (bNameShown ? RelicNameReserve + NamePadding.Top : 0.f);
			IconSize = FVector2D(FMath::Max(16.f, FMath::FloorToFloat(FMath::Min(W, H))));
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

void UItemSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 유물 / 스킬 아이콘을 실제로 그려진 칸에 맞춰 정사각형으로 (줄이기 + 키우기)
	// SetSlotSize 값으로만 계산하면 쓰는 쪽 판 / WBP 여백이 달라 한쪽만 눌려 찌그러짐 (상점 WBP_ItemSlot)
	// 아이콘 자기 크기는 첫 배치 때 잠깐 작게 잡히면 거기 묶여 버려서, 아이콘이 든 세로 줄을 담은 판(바탕 / 버튼) 크기를 기준으로 함
	if (!bBigIcon || !Icon) return;

	// 테마(ApplyTheme, NativeConstruct)가 버튼 스타일을 다시 입히면 여백이 되살아남 -> 다시 맞춤
	if (SlotButton && !(SlotButton->GetStyle().NormalPadding == FMargin(0.f))) ApplyKindLayout();

	UWidget* Column = Icon->GetParent();
	while (Column && !Column->IsA<UVerticalBox>()) Column = Column->GetParent();
	UWidget* Holder = Column ? Column->GetParent() : nullptr;
	if (!Holder) return;

	FMargin Pad(0.f);
	if (const UBorder* Border = Cast<UBorder>(Holder)) Pad = Border->GetPadding();
	else if (const UButton* Button = Cast<UButton>(Holder))
	{
		Pad = Button->GetStyle().NormalPadding;
		if (const UButtonSlot* S = Cast<UButtonSlot>(Button->GetContentSlot())) Pad = Pad + S->GetPadding();
	}

	const FVector2D HolderSize = Holder->GetCachedGeometry().GetLocalSize();
	if (HolderSize.X < 1.f || HolderSize.Y < 1.f) return;

	const bool bNameShown = NameText && NameText->GetVisibility() != ESlateVisibility::Collapsed;
	float NameSpace = 0.f;
	if (bNameShown)
	{
		const UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(NameText->Slot);
		NameSpace = RelicNameReserve + (S ? S->GetPadding().Top + S->GetPadding().Bottom : 0.f);
	}

	const float W = HolderSize.X - Pad.Left - Pad.Right;
	const float H = HolderSize.Y - Pad.Top - Pad.Bottom - NameSpace;
	const float Edge = FMath::FloorToFloat(FMath::Min(W, H));
	const FVector2D Cur = Icon->GetBrush().GetImageSize();
	if (Edge >= 8.f && FMath::Abs(Edge - Cur.X) >= 1.f)
	{
		FSlateBrush Brush = Icon->GetBrush();
		Brush.ImageSize = FVector2D(Edge);
		Icon->SetBrush(Brush);
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
