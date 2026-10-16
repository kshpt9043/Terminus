#include "Widgets/Map/PartyProfileCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

void UPartyProfileCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
}

void UPartyProfileCardWidget::BuildDefaultLayout()
{
	// [초상화] [이름 / 직업 / 체력바]
	CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardBorder"));
	CardBorder->SetBrush(TerminusUI::MakeBoxBrush(TerminusUI::MapColor::PartyPlate, TerminusUI::MapColor::PartyEdge, 0.f));
	CardBorder->SetPadding(FMargin(6.f, 6.f, 10.f, 6.f));
	WidgetTree->RootWidget = CardBorder;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	CardBorder->SetContent(Row);

	USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>();
	PortraitSize->SetWidthOverride(40.f);
	PortraitSize->SetHeightOverride(40.f);
	Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Portrait"));
	PortraitSize->SetContent(Portrait);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PortraitSize))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	USizeBox* InfoSize = WidgetTree->ConstructWidget<USizeBox>();
	InfoSize->SetWidthOverride(156.f);
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
	InfoSize->SetContent(Info);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoSize))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	// [이름 직업 ....... 상태 점]
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Info->AddChildToVerticalBox(NameRow);

	NameText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 14, TerminusUI::MapColor::Text, ETextJustify::Left);
	TerminusUI::SetBold(NameText);
	ClassText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 12, TerminusUI::MapColor::TextSub, ETextJustify::Left);
	NameRow->AddChildToHorizontalBox(NameText)->SetVerticalAlignment(VAlign_Center);
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(ClassText))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(6.f, 1.f, 0.f, 0.f));
	}

	StatusDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StatusDot"));
	FSlateBrush DotBrush = TerminusUI::MakeBoxBrush(FLinearColor::White, FLinearColor::White, 4.f, 0.f);
	DotBrush.ImageSize = FVector2D(8.f, 8.f);
	StatusDot->SetBrush(DotBrush);
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(StatusDot))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}

	// 체력 줄 6px
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetHeightOverride(6.f);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	FProgressBarStyle BarStyle = HealthBar->GetWidgetStyle();
	BarStyle.BackgroundImage = TerminusUI::MakeBoxBrush(TerminusUI::MapColor::HealthBack, TerminusUI::MapColor::HealthBack, 0.f, 0.f);
	BarStyle.FillImage = FSlateBrush();
	HealthBar->SetWidgetStyle(BarStyle);
	HealthBar->SetFillColorAndOpacity(TerminusUI::MapColor::Health);
	BarSize->SetContent(HealthBar);
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(BarSize))
	{
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
}

void UPartyProfileCardWidget::SetPlayer(ATerminusPlayerState* InPlayer)
{
	Player = InPlayer;
	ShownHealth = ShownMaxHealth = INDEX_NONE;   // 사람이 바뀌면 체력도 다시 그림
	RefreshStatic();
	RefreshHealth();
}

void UPartyProfileCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (const ATerminusPlayerState* PS = Player.Get())
	{
		if (PS->GetPlayerName() != ShownName || static_cast<uint8>(PS->GetCharacterClass()) != ShownClass)
		{
			RefreshStatic();
		}
	}
	RefreshHealth();
}

void UPartyProfileCardWidget::RefreshStatic()
{
	ATerminusPlayerState* PS = Player.Get();
	if (!PS) return;

	ShownName = PS->GetPlayerName();
	ShownClass = static_cast<uint8>(PS->GetCharacterClass());

	if (NameText)
	{
		NameText->SetText(FText::FromString(PS->GetPlayerName()));
	}

	const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(PS->GetCharacterClass());

	if (ClassText)
	{
		ClassText->SetText(Row ? Row->DisplayName : FText::GetEmpty());
	}

	if (Portrait)
	{
		UTexture2D* Tex = Row ? Row->Illustration.LoadSynchronous() : nullptr;
		if (Tex)
		{
			Portrait->SetBrushFromTexture(Tex);
		}
		// Hidden 으로 자리는 남김 (카드 폭이 사람마다 달라지지 않게)
		Portrait->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (CardBorder)
	{
		// 내 카드는 금 테두리
		const APlayerController* LocalPC = GetOwningPlayer();
		if (LocalPC && LocalPC->PlayerState == PS)
		{
			FSlateBrush Brush = CardBorder->Background;
			Brush.OutlineSettings.Color = FSlateColor(LocalOutlineColor);
			Brush.OutlineSettings.Width = FMath::Max(1.f, Brush.OutlineSettings.Width);
			CardBorder->SetBrush(Brush);
		}
	}
}

void UPartyProfileCardWidget::RefreshHealth()
{
	const ATerminusPlayerState* PS = Player.Get();
	if (!PS) return;

	// 최대 체력은 런 스텟. 현재 체력은 배틀러가 전투에서 들고 있는 값 (아직 없으면 가득 찬 걸로)
	const FRunState& Run = PS->GetRunState();
	int32 MaxHealth = Run.bStatsInitialized ? Run.Stats.MaxHealth : 0;
	int32 Health = MaxHealth;

	if (const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn()))
	{
		if (const UCombatStatsComponent* Stats = Battler->GetCombatStats())
		{
			if (Stats->GetStats().MaxHealth > 0)
			{
				MaxHealth = Stats->GetStats().MaxHealth;
				Health = Stats->GetCombatState().Health;
			}
		}
	}

	RefreshStatus(MaxHealth > 0 && Health <= 0);

	// 매 틱 불리므로 바뀌었을 때만 넣음
	if (Health == ShownHealth && MaxHealth == ShownMaxHealth) return;
	ShownHealth = Health;
	ShownMaxHealth = MaxHealth;

	if (HealthText)
	{
		HealthText->SetText(MaxHealth > 0
			? FText::FromString(FString::Printf(TEXT("%d / %d"), Health, MaxHealth))
			: FText::GetEmpty());
	}

	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.f);
	}
}

void UPartyProfileCardWidget::RefreshStatus(bool bDown)
{
	const ATerminusPlayerState* PS = Player.Get();
	if (!PS) return;

	// 쓰러짐 > 전투 중(던전 구역 안) > 준비
	const EStatus Status = bDown ? EStatus::Down : PS->GetCurrentArea() ? EStatus::InCombat : EStatus::Ready;
	if (Status == ShownStatus) return;
	ShownStatus = Status;

	if (StatusDot)
	{
		const FLinearColor DotColor = Status == EStatus::Down ? TerminusUI::MapColor::TextSub
			: Status == EStatus::InCombat ? TerminusUI::MapColor::BlueTorch : TerminusUI::MapColor::GreenMana;
		StatusDot->SetColorAndOpacity(DotColor);
	}

	// 쓰러짐: 흑백 + 흐림 (UMG 엔 흑백 효과가 없어 회색 틴트로)
	SetRenderOpacity(Status == EStatus::Down ? DownOpacity : 1.f);
	SetColorAndOpacity(Status == EStatus::Down ? FLinearColor(0.55f, 0.55f, 0.55f, 1.f) : FLinearColor::White);
}
