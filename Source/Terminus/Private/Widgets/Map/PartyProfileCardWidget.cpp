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

namespace
{
	UTextBlock* MakeProfileText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}
}

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
	CardBorder->SetPadding(FMargin(6.f));
	WidgetTree->RootWidget = CardBorder;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	CardBorder->SetContent(Row);

	USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>();
	PortraitSize->SetWidthOverride(52.f);
	PortraitSize->SetHeightOverride(52.f);
	Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Portrait"));
	PortraitSize->SetContent(Portrait);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PortraitSize))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	USizeBox* InfoSize = WidgetTree->ConstructWidget<USizeBox>();
	InfoSize->SetWidthOverride(130.f);
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
	InfoSize->SetContent(Info);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InfoSize))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	NameText = MakeProfileText(WidgetTree, 15, FLinearColor::White);
	ClassText = MakeProfileText(WidgetTree, 12, FLinearColor(0.75f, 0.75f, 0.75f));
	Info->AddChildToVerticalBox(NameText);
	Info->AddChildToVerticalBox(ClassText);

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.3f, 0.85f, 0.35f));
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(HealthBar))
	{
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	HealthText = MakeProfileText(WidgetTree, 12, FLinearColor::White);
	Info->AddChildToVerticalBox(HealthText);
}

void UPartyProfileCardWidget::SetPlayer(ATerminusPlayerState* InPlayer)
{
	Player = InPlayer;
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
		const APlayerController* LocalPC = GetOwningPlayer();
		const bool bLocal = LocalPC && LocalPC->PlayerState == PS;
		CardBorder->SetBrushColor(bLocal ? LocalCardColor : OtherCardColor);
	}
}

void UPartyProfileCardWidget::RefreshHealth()
{
	const ATerminusPlayerState* PS = Player.Get();
	if (!PS) return;

	// 최대 체력은 런 스텟. 현재 체력은 배틀러가 전투에서 들고 있는 값 (아직 없으면 가득 찬 걸로)
	const FRunState Run = PS->GetRunState();
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
