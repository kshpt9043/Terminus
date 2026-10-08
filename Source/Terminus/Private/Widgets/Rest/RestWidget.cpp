#include "Widgets/Rest/RestWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Player/TerminusPlayerController.h"

namespace
{
	UTextBlock* MakeRestText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Center)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
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

	const FLinearColor RestGold(1.f, 0.82f, 0.35f);
	const FLinearColor RestOptionColor(0.12f, 0.26f, 0.28f, 0.95f);
}

void URestClickRelay::HandleClicked()
{
	if (URestWidget* Widget = Owner.Get())
	{
		Widget->HandleOption(Option, Target.Get());
	}
}

void URestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !OptionBox)
	{
		BuildDefaultLayout();
	}

	if (ArtImage && ArtTexture)
	{
		ArtImage->SetBrushFromTexture(ArtTexture);
	}

	// 아래 화면을 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void URestWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RestDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	// 판: 왼쪽 그림 | 오른쪽 글 + 선택지
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RestPanel"));
	Panel->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.96f));
	Panel->SetPadding(FMargin(30.f));
	Dim->SetContent(Panel);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Panel->SetContent(Row);

	// 그림 칸 (비어 있으면 어두운 칸)
	USizeBox* ArtBox = WidgetTree->ConstructWidget<USizeBox>();
	ArtBox->SetWidthOverride(ArtSize.X);
	ArtBox->SetHeightOverride(ArtSize.Y);
	UBorder* ArtFrame = WidgetTree->ConstructWidget<UBorder>();
	ArtFrame->SetBrushColor(FLinearColor(0.08f, 0.12f, 0.1f, 1.f));
	ArtFrame->SetPadding(FMargin(0.f));
	ArtBox->SetContent(ArtFrame);
	ArtImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ArtImage"));
	ArtImage->SetColorAndOpacity(ArtTexture ? FLinearColor::White : FLinearColor::Transparent);
	ArtFrame->SetContent(ArtImage);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ArtBox)) S->SetVerticalAlignment(VAlign_Center);

	// 글 + 선택지
	USizeBox* RightBox = WidgetTree->ConstructWidget<USizeBox>();
	RightBox->SetWidthOverride(OptionSize.X);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(RightBox))
	{
		S->SetPadding(FMargin(40.f, 0.f, 0.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	RightBox->SetContent(Column);

	TitleText = MakeRestText(WidgetTree, TEXT("휴식터"), 30, RestGold);
	Column->AddChildToVerticalBox(TitleText);

	MessageText = MakeRestText(WidgetTree, TEXT(""), 16, FLinearColor(0.92f, 0.9f, 0.86f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageText))
	{
		S->SetPadding(FMargin(0.f, 12.f, 0.f, 36.f));
	}

	OptionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OptionBox"));
	Column->AddChildToVerticalBox(OptionBox);
}

void URestWidget::Setup(const TArray<APlayerState*>& InOccupants, float InHealRatio, FIntPoint InExploreCurrency, float InExploreRelicChance)
{
	HealRatio = InHealRatio;
	ExploreCurrency = InExploreCurrency;
	ExploreRelicChance = InExploreRelicChance;
	Stage = EStage::Choose;
	ResultText = FText::GetEmpty();

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

	Rebuild();
}

void URestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 대상 고르는 중에 체력이 바뀌면 (동료가 나를 회복) 다시 그림
	RefreshTimer += InDeltaTime;
	if (RefreshTimer >= 0.3f)
	{
		RefreshTimer = 0.f;
		if (Stage == EStage::PickTarget && ReadHealth() != ShownHealth)
		{
			Rebuild();
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

void URestWidget::AddOption(ERestOption Option, APlayerState* Target, const FString& Title, const FString& Description, bool bEnabled)
{
	if (!OptionBox) return;

	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(RestOptionColor);
	Button->SetIsEnabled(bEnabled);

	URestClickRelay* Relay = NewObject<URestClickRelay>(this);
	Relay->Owner = this;
	Relay->Option = Option;
	Relay->Target = Target;
	Button->OnClicked.AddDynamic(Relay, &URestClickRelay::HandleClicked);
	Relays.Add(Relay);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(OptionSize.X);
	Size->SetMinDesiredHeight(OptionSize.Y);
	Button->SetContent(Size);

	UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
	Size->SetContent(Inside);
	if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(MakeRestText(WidgetTree, Title, 17, RestGold, ETextJustify::Left)))
	{
		S->SetPadding(FMargin(14.f, 6.f, 14.f, 0.f));
	}
	if (!Description.IsEmpty())
	{
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(MakeRestText(WidgetTree, Description, 14, FLinearColor(0.9f, 0.9f, 0.9f), ETextJustify::Left)))
		{
			S->SetPadding(FMargin(14.f, 2.f, 14.f, 6.f));
		}
	}

	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(OptionBox->AddChild(Button)))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
}

void URestWidget::Rebuild()
{
	if (OptionBox)
	{
		OptionBox->ClearChildren();
	}
	Relays.Reset();
	ShownHealth = ReadHealth();

	const int32 Percent = FMath::RoundToInt(HealRatio * 100.f);
	const bool bMulti = Occupants.Num() > 1;
	FString Message;

	switch (Stage)
	{
	case EStage::Choose:
	{
		Message = TEXT("모닥불이 타닥거리는 조용한 쉼터입니다.\n잠시 쉬어 갈 수도, 주변을 둘러볼 수도 있습니다.");

		int32 Health = 0, MaxHealth = 0;
		const APlayerState* Me = Occupants.Num() > 0 ? Occupants[0].Get() : nullptr;
		const bool bHasHealth = GetRestHealth(Me, Health, MaxHealth);
		const int32 Heal = FMath::Max(1, FMath::RoundToInt(MaxHealth * HealRatio));

		AddOption(ERestOption::Rest, nullptr, TEXT("휴식"), bMulti
			? FString::Printf(TEXT("나 또는 동료 한 명의 최대 체력 %d%%를 회복합니다."), Percent)
			: (bHasHealth
				? FString::Printf(TEXT("최대 체력의 %d%% (+%d)를 회복합니다. (체력 %d / %d)"), Percent, Heal, Health, MaxHealth)
				: FString::Printf(TEXT("최대 체력의 %d%%를 회복합니다."), Percent)));

		AddOption(ERestOption::Explore, nullptr, TEXT("탐색"),
			FString::Printf(TEXT("주변을 뒤져 던전 재화 %d~%d 를 얻습니다. %d%% 확률로 유물을 발견합니다."),
				ExploreCurrency.X, ExploreCurrency.Y, FMath::RoundToInt(ExploreRelicChance * 100.f)));
		break;
	}

	case EStage::PickTarget:
	{
		Message = FString::Printf(TEXT("누구를 회복시킬까요? 최대 체력의 %d%%를 회복합니다.\n동료를 고르면 나는 회복하지 않습니다."), Percent);

		const APlayerState* Me = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
		for (const TWeakObjectPtr<APlayerState>& Weak : Occupants)
		{
			APlayerState* PS = Weak.Get();
			if (!PS) continue;

			int32 Health = 0, MaxHealth = 0;
			const bool bHasHealth = GetRestHealth(PS, Health, MaxHealth);
			const int32 Heal = FMath::Max(1, FMath::RoundToInt(MaxHealth * HealRatio));

			AddOption(ERestOption::Target, PS,
				PS == Me ? FString::Printf(TEXT("나 (%s)"), *PS->GetPlayerName()) : PS->GetPlayerName(),
				bHasHealth ? FString::Printf(TEXT("체력 %d / %d  →  +%d 회복"), Health, MaxHealth, Heal) : FString(TEXT("체력 ?")));
		}
		AddOption(ERestOption::Back, nullptr, TEXT("돌아가기"), FString());
		break;
	}

	case EStage::Waiting:
		Message = TEXT("...");
		break;

	case EStage::Done:
		Message = ResultText.ToString();
		if (bMulti)
		{
			Message += TEXT("\n\n다른 동료가 고르기를 기다리는 중...");
		}
		break;
	}

	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Message));
	}
}

void URestWidget::HandleOption(ERestOption Option, APlayerState* Target)
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();

	switch (Option)
	{
	case ERestOption::Rest:
		if (Stage != EStage::Choose) return;

		// 싱글(혼자 들어온 휴식터)은 바로 나를 회복
		if (Occupants.Num() <= 1)
		{
			Stage = EStage::Waiting;
			Rebuild();
			if (PC) PC->Server_ChooseRestTarget(PC->PlayerState);
			return;
		}
		Stage = EStage::PickTarget;
		Rebuild();
		return;

	case ERestOption::Explore:
		if (Stage != EStage::Choose) return;
		Stage = EStage::Waiting;
		Rebuild();
		if (PC) PC->Server_ChooseRestExplore();
		return;

	case ERestOption::Target:
		if (Stage != EStage::PickTarget || !Target) return;
		Stage = EStage::Waiting;
		Rebuild();
		if (PC) PC->Server_ChooseRestTarget(Target);
		return;

	case ERestOption::Back:
		if (Stage != EStage::PickTarget) return;
		Stage = EStage::Choose;
		Rebuild();
		return;
	}
}

void URestWidget::ShowResult(const FText& Result)
{
	ResultText = Result;
	Stage = EStage::Done;
	Rebuild();
}
