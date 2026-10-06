#include "Widgets/Rest/RestWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TerminusDataSettings.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"

namespace
{
	UTextBlock* MakeRestText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}

	// 배틀러의 지금 / 최대 체력
	bool GetRestHealth(const APlayerState* PS, int32& OutHealth, int32& OutMax)
	{
		const ATerminusBattler* Battler = PS ? Cast<ATerminusBattler>(PS->GetPawn()) : nullptr;
		const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
		if (!Stats) return false;

		OutHealth = Stats->GetCombatState().Health;
		OutMax = Stats->GetStats().MaxHealth;
		return true;
	}

	const FLinearColor RestGreen(0.55f, 0.9f, 0.55f);
}

void URestClickRelay::HandleClicked()
{
	if (URestWidget* Widget = Owner.Get())
	{
		Widget->HandleCardClicked(Target.Get());
	}
}

void URestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	// 아래 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void URestWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RestDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.02f, 0.f, 0.6f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeRestText(WidgetTree, TEXT("휴식터"), 32, RestGreen);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	MessageText = MakeRestText(WidgetTree, TEXT(""), 16, FLinearColor(0.9f, 0.9f, 0.9f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 26.f));
	}

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CardBox)) S->SetHorizontalAlignment(HAlign_Center);
}

void URestWidget::Setup(const TArray<APlayerState*>& InOccupants, float InHealRatio)
{
	HealRatio = InHealRatio;
	Chosen.Reset();

	// 나를 맨 앞에
	Occupants.Reset();
	const APlayerState* Me = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	for (APlayerState* PS : InOccupants)
	{
		if (PS && PS == Me) Occupants.Add(PS);
	}
	for (APlayerState* PS : InOccupants)
	{
		if (PS && PS != Me) Occupants.Add(PS);
	}

	RebuildCards();
	UpdateMessage();
}

void URestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 체력이 바뀌면 (동료가 나를 회복) 다시 그림
	RefreshTimer += InDeltaTime;
	if (RefreshTimer >= 0.3f)
	{
		RefreshTimer = 0.f;
		if (ReadHealth() != ShownHealth)
		{
			RebuildCards();
		}
	}
}

TArray<int32> URestWidget::ReadHealth() const
{
	TArray<int32> Out;
	for (const TWeakObjectPtr<APlayerState>& Weak : Occupants)
	{
		int32 Health = -1, MaxHealth = -1;
		GetRestHealth(Weak.Get(), Health, MaxHealth);
		Out.Add(Health);
		Out.Add(MaxHealth);
	}
	return Out;
}

void URestWidget::RebuildCards()
{
	if (!CardBox) return;

	CardBox->ClearChildren();
	Relays.Reset();
	ShownHealth = ReadHealth();

	const APlayerState* Me = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;

	for (const TWeakObjectPtr<APlayerState>& Weak : Occupants)
	{
		APlayerState* PS = Weak.Get();
		if (!PS) continue;

		const bool bMe = PS == Me;
		const bool bChosen = Chosen.Get() == PS;

		int32 Health = 0, MaxHealth = 0;
		const bool bHasHealth = GetRestHealth(PS, Health, MaxHealth);
		const int32 Heal = FMath::RoundToInt(MaxHealth * HealRatio);

		UButton* Card = WidgetTree->ConstructWidget<UButton>();
		Card->SetBackgroundColor(bChosen ? FLinearColor(0.35f, 0.6f, 0.35f) : FLinearColor(0.3f, 0.32f, 0.3f));
		Card->SetIsEnabled(!Chosen.IsValid());

		URestClickRelay* Relay = NewObject<URestClickRelay>(this);
		Relay->Owner = this;
		Relay->Target = PS;
		Card->OnClicked.AddDynamic(Relay, &URestClickRelay::HandleClicked);
		Relays.Add(Relay);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y);
		Card->SetContent(Size);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
		Size->SetContent(Inside);

		Inside->AddChildToVerticalBox(MakeRestText(WidgetTree, bMe ? FString(TEXT("나")) : PS->GetPlayerName(), 22, FLinearColor::White));

		FString ClassName;
		if (const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
		{
			if (const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(TPS->GetCharacterClass()))
			{
				ClassName = Row->DisplayName.ToString();
			}
		}
		if (bMe) ClassName = FString::Printf(TEXT("%s · %s"), *PS->GetPlayerName(), *ClassName);
		Inside->AddChildToVerticalBox(MakeRestText(WidgetTree, ClassName, 14, FLinearColor(0.75f, 0.75f, 0.75f)));

		UTextBlock* HealthText = MakeRestText(WidgetTree,
			bHasHealth ? FString::Printf(TEXT("체력 %d / %d"), Health, MaxHealth) : FString(TEXT("체력 ?")), 18, FLinearColor::White);
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(HealthText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}

		Inside->AddChildToVerticalBox(MakeRestText(WidgetTree,
			bChosen ? FString(TEXT("회복시킴")) : FString::Printf(TEXT("+%d 회복"), Heal), 18, RestGreen));

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(CardBox->AddChild(Card)))
		{
			S->SetPadding(FMargin(10.f, 0.f));
		}
	}
}

void URestWidget::UpdateMessage()
{
	if (!MessageText) return;

	const int32 Percent = FMath::RoundToInt(HealRatio * 100.f);
	FString Message;
	if (Chosen.IsValid())
	{
		Message = Occupants.Num() > 1
			? FString(TEXT("다른 동료가 고르기를 기다리는 중..."))
			: FString(TEXT("회복했습니다."));
	}
	else
	{
		Message = Occupants.Num() > 1
			? FString::Printf(TEXT("회복시킬 사람을 고르세요. 최대 체력의 %d%%를 회복합니다.\n동료를 고르면 나는 회복하지 않습니다."), Percent)
			: FString::Printf(TEXT("잠시 쉬어 갑니다. 최대 체력의 %d%%를 회복합니다."), Percent);
	}
	MessageText->SetText(FText::FromString(Message));
}

void URestWidget::HandleCardClicked(APlayerState* Target)
{
	if (Chosen.IsValid() || !Target) return;

	Chosen = Target;
	RebuildCards();
	UpdateMessage();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_ChooseRestTarget(Target);
	}
}
