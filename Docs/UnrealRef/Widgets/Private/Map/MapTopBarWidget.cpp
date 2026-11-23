#include "Widgets/Map/MapTopBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
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
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/TerminusUISettings.h"


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
	Bar->SetBrushColor(TerminusUI::MapColor::TopBar);
	Bar->SetPadding(FMargin(28.f, 8.f));
	Bar->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Bar;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Bar->SetContent(Row);

	// 왼쪽: 층 / 진행
	UVerticalBox* FloorBox = WidgetTree->ConstructWidget<UVerticalBox>();
	FloorText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 24, TerminusUI::MapColor::Text, ETextJustify::Left);
	TerminusUI::SetBold(FloorText);
	RoomText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 14, TerminusUI::MapColor::TextSub, ETextJustify::Left);
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

	// 오른쪽: 동전 / 재화 / 나가기
	UImage* Coin = WidgetTree->ConstructWidget<UImage>();
	if (UTexture2D* CoinTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/Map/T_Map_Coin.T_Map_Coin")))
	{
		Coin->SetBrushFromTexture(CoinTexture);
	}
	Coin->SetDesiredSizeOverride(FVector2D(18.f, 18.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(24.f, 0.f, 10.f, 0.f));
	}

	CurrencyText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 20, TerminusUI::Color::GoldHover, ETextJustify::Left);
	TerminusUI::SetBold(CurrencyText);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CurrencyText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
	}

	ExitButton = TerminusUI::MakeButton(WidgetTree, NAME_None, TEXT("나가기"), 16);
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

	const TSubclassOf<UPartyProfileCardWidget> Class = UTerminusUISettings::Pick(ProfileCardClass, &UTerminusUISettings::ProfileCard);

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

AMapManager* UMapTopBarWidget::GetMapManager()
{
	if (!CachedMapManager.IsValid())
	{
		CachedMapManager = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	}
	return CachedMapManager.Get();
}

void UMapTopBarWidget::RefreshTexts()
{
	// 매 틱 불림 -> 글자는 바뀌었을 때만 넣음 (SetTextIfChanged)
	const ATerminusPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const AMapManager* MapMgr = GetMapManager();

	if (FloorText)
	{
		const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
		FString Sub = AMapManager::GetTierName(Floor).ToString();
		if (MapMgr && MapMgr->FloorTheme && !MapMgr->FloorTheme->DisplayName.IsEmpty())
		{
			Sub += FString::Printf(TEXT(" · %s"), *MapMgr->FloorTheme->DisplayName.ToString());
		}

		// 시안: 굵은 "2층" 옆에 흐린 "표층 · 슬라임 왕국"
		const FString FloorLabel = FString::Printf(TEXT("%d층"), Floor);
		if (FloorSubText)
		{
			TerminusUI::SetTextIfChanged(FloorText, FloorLabel);
			TerminusUI::SetTextIfChanged(FloorSubText, Sub);
		}
		else
		{
			TerminusUI::SetTextIfChanged(FloorText, FloorLabel + TEXT(" · ") + Sub);
		}
	}

	if (RoomText)
	{
		// CurrentMapLevel = 지금까지 지난 방 수 -> 다음에 들어갈 방은 그 다음
		const int32 Total = MapMgr ? MapMgr->GetLevelCount() : 12;   // 퀘스트 방이 없는 층은 한 줄 적음
		const int32 Next = FMath::Min((LocalPS ? LocalPS->GetCurrentMapLevel() : 0) + 1, Total);
		TerminusUI::SetTextIfChanged(RoomText, FString::Printf(TEXT("다음 방 %d / %d"), Next, Total));
	}

	if (CurrencyText)
	{
		// 숫자만 (옆에 동전 그림)
		TerminusUI::SetTextIfChanged(CurrencyText, FText::AsNumber(LocalPS ? LocalPS->GetRunState().Currency : 0));
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
