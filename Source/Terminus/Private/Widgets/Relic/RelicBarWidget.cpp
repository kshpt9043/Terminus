#include "Widgets/Relic/RelicBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Data/RelicTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Engine/Texture2D.h"
#include "Player/TerminusPlayerState.h"

namespace
{
	// 등급별 테두리 색
	FLinearColor RelicTierColor(ERelicTier Tier)
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

	FString RelicTierLabel(ERelicTier Tier)
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

void URelicBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !RelicBox)
	{
		BuildDefaultLayout();
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URelicBarWidget::BuildDefaultLayout()
{
	RelicBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RelicBox"));
	WidgetTree->RootWidget = RelicBox;
}

void URelicBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const TArray<FName> Relics = PS ? PS->GetRelics() : TArray<FName>();

	if (!bShownOnce || Relics != ShownRelics)
	{
		Rebuild(Relics);
	}
}

void URelicBarWidget::Rebuild(const TArray<FName>& Relics)
{
	bShownOnce = true;
	ShownRelics = Relics;

	if (!RelicBox) return;
	RelicBox->ClearChildren();

	for (int32 i = 0; i < Relics.Num(); ++i)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Relics[i]);
		if (!Relic) continue;

		// [테두리(등급 색) > 아이콘 또는 이름 두 글자]
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(IconSize);
		Size->SetHeightOverride(IconSize);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(RelicTierColor(Relic->RelicTier));
		Frame->SetPadding(FMargin(2.f));
		Size->SetContent(Frame);

		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>();
		Inner->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.07f, 0.95f));
		Inner->SetHorizontalAlignment(HAlign_Center);
		Inner->SetVerticalAlignment(VAlign_Center);
		Frame->SetContent(Inner);

		if (UTexture2D* Tex = Relic->Icon.LoadSynchronous())
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>();
			Icon->SetBrushFromTexture(Tex);
			Inner->SetContent(Icon);
		}
		else
		{
			UTextBlock* Short = WidgetTree->ConstructWidget<UTextBlock>();
			Short->SetText(FText::FromString(Relic->RelicName.ToString().Left(2)));
			FSlateFontInfo Font = Short->GetFont();
			Font.Size = 11;
			Short->SetFont(Font);
			Short->SetJustification(ETextJustify::Center);
			Inner->SetContent(Short);
		}

		// 툴팁: 이름 [등급] / 설명
		Size->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s  [%s]\n%s"),
			*Relic->RelicName.ToString(), *RelicTierLabel(Relic->RelicTier), *Relic->RelicDesc.ToString())));
		Size->SetVisibility(ESlateVisibility::Visible);   // 툴팁이 뜨려면 마우스를 받아야 함

		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(RelicBox->AddChild(Size)))
		{
			HSlot->SetPadding(FMargin(i == 0 ? 0.f : Spacing, 0.f, 0.f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}
