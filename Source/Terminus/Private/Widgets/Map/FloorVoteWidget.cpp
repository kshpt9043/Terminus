#include "Widgets/Map/FloorVoteWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/TerminusPlayerController.h"

namespace
{
	UTextBlock* MakeVoteText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	}

	const FLinearColor VoteGold(1.f, 0.85f, 0.35f);
	const FLinearColor VoteRed(0.85f, 0.3f, 0.28f);
}

void UFloorVoteClickRelay::HandleClicked()
{
	if (UFloorVoteWidget* Widget = Owner.Get())
	{
		Widget->HandleCardClicked(Choice);
	}
}

void UFloorVoteWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !CardBox)
	{
		BuildDefaultLayout();
	}

	// 아래 지도를 못 누르게
	SetVisibility(ESlateVisibility::Visible);
}

void UFloorVoteWidget::BuildDefaultLayout()
{
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FloorVoteDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Dim->SetContent(Column);

	TitleText = MakeVoteText(WidgetTree, TEXT("TitleText"), TEXT(""), 30, FLinearColor::White);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

	StatusText = MakeVoteText(WidgetTree, TEXT("StatusText"), TEXT(""), 17, VoteGold);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 28.f));
	}

	CardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CardBox"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CardBox)) S->SetHorizontalAlignment(HAlign_Center);
}

// =====================================================================
// 상태 반영
// =====================================================================

void UFloorVoteWidget::Refresh(const FFloorVoteState& State)
{
	// 새 투표면 내 선택 초기화 (내 표는 공개 전엔 서버가 안 알려 줌 -> 이 화면이 기억)
	if (State.VoteId != Shown.VoteId)
	{
		MyChoice = EFloorChoice::None;
	}
	Shown = State;

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%d층 보스를 쓰러뜨렸습니다"), State.Floor)));
	}

	RebuildCards();
	UpdateStatus();
}

void UFloorVoteWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateStatus();
}

void UFloorVoteWidget::UpdateStatus()
{
	if (!StatusText) return;

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	const int32 Remaining = FMath::Max(0, FMath::CeilToInt(Shown.PhaseEndTime - Now));

	FString Status;
	switch (Shown.Phase)
	{
	case EFloorVotePhase::Voting:
		Status = MyChoice == EFloorChoice::None
			? FString::Printf(TEXT("다음 행선지를 고르세요 · %d초  (%d / %d명 투표)"), Remaining, Shown.VotedCount, Shown.TotalVoters)
			: FString::Printf(TEXT("다른 사람을 기다리는 중 · %d초  (%d / %d명 투표)"), Remaining, Shown.VotedCount, Shown.TotalVoters);
		break;
	case EFloorVotePhase::Revealing:
		Status = FString::Printf(TEXT("투표 끝! 누가 무엇을 골랐는지 %d초 뒤 공개"), Remaining);
		break;
	case EFloorVotePhase::Done:
		Status = FString::Printf(TEXT("결과: %s"), *ChoiceName(Shown.Result));
		break;
	default:
		break;
	}
	StatusText->SetText(FText::FromString(Status));
}

void UFloorVoteWidget::RebuildCards()
{
	if (!CardBox) return;

	CardBox->ClearChildren();
	Relays.Reset();

	const bool bVoting = Shown.Phase == EFloorVotePhase::Voting;
	const bool bRevealed = Shown.Phase == EFloorVotePhase::Done;

	for (int32 i = 0; i < Shown.Options.Num(); ++i)
	{
		const EFloorChoice Choice = Shown.Options[i];
		const int32 Count = Shown.Counts.IsValidIndex(i) ? Shown.Counts[i] : 0;
		const bool bMine = MyChoice == Choice;
		const bool bBetrayTaken = Choice == EFloorChoice::Betray && Count > 0;
		const bool bWinner = bRevealed && Shown.Result == Choice;

		UButton* Card = WidgetTree->ConstructWidget<UButton>();
		Card->SetBackgroundColor(bWinner ? VoteGold : (bMine ? FLinearColor(0.55f, 0.5f, 0.35f) : (Choice == EFloorChoice::Betray ? VoteRed : FLinearColor(0.35f, 0.35f, 0.35f))));
		Card->SetIsEnabled(bVoting && MyChoice == EFloorChoice::None && !bBetrayTaken);

		UFloorVoteClickRelay* Relay = NewObject<UFloorVoteClickRelay>(this);
		Relay->Owner = this;
		Relay->Choice = Choice;
		Card->OnClicked.AddDynamic(Relay, &UFloorVoteClickRelay::HandleClicked);
		Relays.Add(Relay);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y);
		Card->SetContent(Size);

		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>();
		Size->SetContent(Inside);

		Inside->AddChildToVerticalBox(MakeVoteText(WidgetTree, NAME_None, ChoiceName(Choice), 24, FLinearColor::White));

		UTextBlock* Desc = MakeVoteText(WidgetTree, NAME_None, ChoiceDescription(Choice), 14, FLinearColor(0.9f, 0.9f, 0.9f));
		if (UVerticalBoxSlot* S = Inside->AddChildToVerticalBox(Desc))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}

		// 표 수 (누가 골랐는지는 공개 뒤에)
		FString CountLabel = Shown.TotalVoters > 1 ? FString::Printf(TEXT("%d명 선택"), Count) : FString();
		if (bMine) CountLabel += TEXT("  (내 선택)");
		Inside->AddChildToVerticalBox(MakeVoteText(WidgetTree, NAME_None, CountLabel, 16, VoteGold));

		// 공개: 카드 아래에 고른 사람
		UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		CardColumn->AddChildToVerticalBox(Card);
		if (bRevealed && Shown.TotalVoters > 1)
		{
			TArray<FString> Names;
			for (const FFloorVoteEntry& Entry : Shown.RevealedVotes)
			{
				if (Entry.Choice == Choice && Entry.Player) Names.Add(Entry.Player->GetPlayerName());
			}
			UTextBlock* Who = MakeVoteText(WidgetTree, NAME_None, Names.Num() > 0 ? FString::Join(Names, TEXT("\n")) : FString(TEXT("-")), 15, FLinearColor::White);
			if (UVerticalBoxSlot* S = CardColumn->AddChildToVerticalBox(Who)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(CardBox->AddChild(CardColumn)))
		{
			S->SetPadding(FMargin(12.f, 0.f));
			S->SetVerticalAlignment(VAlign_Top);
		}
	}
}

void UFloorVoteWidget::ClearMyChoice()
{
	MyChoice = EFloorChoice::None;
	RebuildCards();
	UpdateStatus();
}

void UFloorVoteWidget::HandleCardClicked(EFloorChoice Choice)
{
	if (Shown.Phase != EFloorVotePhase::Voting || MyChoice != EFloorChoice::None) return;

	// 서버 결과를 기다리지 않고 바로 내 선택으로 표시 (서버가 거절하면 다음 복제에서 되돌아감)
	MyChoice = Choice;
	RebuildCards();
	UpdateStatus();

	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_CastFloorVote(Choice);
	}
}

// =====================================================================
// 문구
// =====================================================================

FString UFloorVoteWidget::ChoiceName(EFloorChoice Choice)
{
	switch (Choice)
	{
	case EFloorChoice::NextFloor: return TEXT("다음 층");
	case EFloorChoice::Escape:    return TEXT("탈출");
	case EFloorChoice::Betray:    return TEXT("배신");
	default:                      return TEXT("-");
	}
}

FString UFloorVoteWidget::ChoiceDescription(EFloorChoice Choice) const
{
	switch (Choice)
	{
	case EFloorChoice::NextFloor:
		return FString::Printf(TEXT("%d층(%s)으로 내려갑니다.\n새 테마, 더 강한 적과 보상."), Shown.Floor + 1, *AMapManager::GetTierName(Shown.Floor + 1).ToString());
	case EFloorChoice::Escape:
		return TEXT("던전을 나가 정산합니다.\n얻은 유물 중 하나를 창고에 보관하고,\n장착한 픽업 스킬 하나를 가져갑니다.");
	case EFloorChoice::Betray:
		return TEXT("동료를 배신하고 홀로 싸웁니다.\n이기면 동료들의 보스 유물을 빼앗습니다.\n선착순 1명만 고를 수 있습니다.");
	default:
		return FString();
	}
}
