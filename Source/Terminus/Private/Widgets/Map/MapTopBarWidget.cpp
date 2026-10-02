#include "Widgets/Map/MapTopBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dungeon/DungeonThemeData.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Map/MapManager.h"
#include "Online/SessionSubsystem.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Widgets/Common/ConfirmPopupWidget.h"
#include "Widgets/Map/PartyProfileCardWidget.h"

namespace
{
	const FLinearColor BarColor(0.03f, 0.03f, 0.04f, 0.85f);
	const FLinearColor CurrencyColor(1.f, 0.82f, 0.3f);

	UTextBlock* MakeBarText(UWidgetTree* Tree, const FString& Initial, int32 Size, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UButton* MakeTextButton(UWidgetTree* Tree, const FString& Label, int32 Size)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		UTextBlock* Text = MakeBarText(Tree, Label, Size, FLinearColor::Black);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		return Button;
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UMapTopBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (ExitButton) ExitButton->OnClicked.AddDynamic(this, &UMapTopBarWidget::HandleExitClicked);
}

void UMapTopBarWidget::BuildDefaultLayout()
{
	// 이 위젯 자체가 가로 바 하나. 크기는 지도 화면(WBP_MapScreen)에서 놓은 칸을 따름
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bar"));
	Bar->SetBrushColor(BarColor);
	Bar->SetPadding(FMargin(20.f, 8.f));
	Bar->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Bar;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Bar->SetContent(Row);

	// 왼쪽: 층 / 진행
	UVerticalBox* FloorBox = WidgetTree->ConstructWidget<UVerticalBox>();
	FloorText = MakeBarText(WidgetTree, TEXT(""), 20);
	RoomText = MakeBarText(WidgetTree, TEXT(""), 14, FLinearColor(0.75f, 0.75f, 0.75f));
	FloorBox->AddChildToVerticalBox(FloorText);
	FloorBox->AddChildToVerticalBox(RoomText);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(FloorBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	}

	// 가운데: 참가자 카드 (남는 폭을 다 쓰고 그 안에서 가운데)
	PartyBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PartyBox"));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PartyBox))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	// 오른쪽: 재화 / 나가기
	CurrencyText = MakeBarText(WidgetTree, TEXT(""), 20, CurrencyColor);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CurrencyText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(24.f, 0.f, 20.f, 0.f));
	}

	ExitButton = MakeTextButton(WidgetTree, TEXT("나가기"), 16);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ExitButton))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
}

void UMapTopBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshParty();
	RefreshTexts();
}

// =====================================================================
// 갱신
// =====================================================================

void UMapTopBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 참가자 구성이 바뀌었으면 (클라에 PS 가 늦게 도착, 누가 나감) 카드 다시 만들기
	TArray<TWeakObjectPtr<ATerminusPlayerState>> Current;
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS)) Current.Add(TPS);
		}
	}
	if (Current != ShownPlayers)
	{
		RefreshParty();
	}

	RefreshTexts();
}

void UMapTopBarWidget::RefreshParty()
{
	ShownPlayers.Reset();
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS)) ShownPlayers.Add(TPS);
		}
	}

	if (!PartyBox) return;
	PartyBox->ClearChildren();

	const TSubclassOf<UPartyProfileCardWidget> Class = ProfileCardClass ? ProfileCardClass : TSubclassOf<UPartyProfileCardWidget>(UPartyProfileCardWidget::StaticClass());

	for (int32 i = 0; i < ShownPlayers.Num(); ++i)
	{
		UPartyProfileCardWidget* Card = CreateWidget<UPartyProfileCardWidget>(this, Class);
		if (!Card) continue;

		Card->SetPlayer(ShownPlayers[i].Get());

		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(PartyBox->AddChild(Card)))
		{
			HSlot->SetPadding(FMargin(i == 0 ? 0.f : CardSpacing * 0.5f, 0.f, i == ShownPlayers.Num() - 1 ? 0.f : CardSpacing * 0.5f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UMapTopBarWidget::RefreshTexts()
{
	const ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));

	if (FloorText)
	{
		const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
		FString Label = FString::Printf(TEXT("%d층 · %s"), Floor, *AMapManager::GetTierName(Floor).ToString());
		if (MapMgr && MapMgr->FloorTheme && !MapMgr->FloorTheme->DisplayName.IsEmpty())
		{
			Label += FString::Printf(TEXT(" · %s"), *MapMgr->FloorTheme->DisplayName.ToString());
		}
		FloorText->SetText(FText::FromString(Label));
	}

	if (RoomText)
	{
		// CurrentMapLevel = 지금까지 지난 방 수 -> 다음에 들어갈 방은 그 다음
		const int32 Total = MapMgr ? MapMgr->TotalLevels : 12;
		const int32 Next = FMath::Min((LocalPS ? LocalPS->GetCurrentMapLevel() : 0) + 1, Total);
		RoomText->SetText(FText::FromString(FString::Printf(TEXT("다음 방 %d / %d"), Next, Total)));
	}

	if (CurrencyText)
	{
		CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("재화 %d"), LocalPS ? LocalPS->GetRunState().Currency : 0)));
	}
}

// =====================================================================
// 나가기
// =====================================================================

void UMapTopBarWidget::HandleExitClicked()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC) return;

	// 리슨 서버 호스트가 나가면 세션이 닫혀 다른 사람도 같이 끝남
	const UWorld* World = GetWorld();
	const bool bHostWithOthers = World && World->GetNetMode() == NM_ListenServer
		&& World->GetGameState() && World->GetGameState()->PlayerArray.Num() > 1;

	const FText Message = FText::FromString(bHostWithOthers
		? TEXT("메인 화면으로 돌아갑니다.\n방장이 나가면 다른 플레이어도 모두 게임이 끝납니다.")
		: TEXT("메인 화면으로 돌아갑니다.\n이번 진행은 사라집니다."));

	if (UConfirmPopupWidget* Popup = PC->ShowPopup(FText::FromString(TEXT("던전을 나가시겠습니까?")), Message,
		FText::FromString(TEXT("나가기")), FText::FromString(TEXT("취소"))))
	{
		Popup->OnConfirmedNative.BindUObject(this, &UMapTopBarWidget::LeaveToMenu);
	}
}

void UMapTopBarWidget::LeaveToMenu()
{
	if (USessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<USessionSubsystem>() : nullptr)
	{
		// 세션이 있으면 정리하고, 없으면 바로 메인(Lv_Lobby)으로
		Sessions->LeaveToMenu();
	}
}
