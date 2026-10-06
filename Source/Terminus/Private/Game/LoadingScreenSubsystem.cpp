#include "Game/LoadingScreenSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Common/LoadingScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusLoading, Log, All);

namespace
{
	// 뷰포트 맨 위 (팝업 50 / 채팅 25 보다 훨씬 위)
	constexpr int32 LoadingZOrder = 10000;
}

ULoadingScreenSubsystem* ULoadingScreenSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
}

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// PreLoadMapWithContext 는 무비 플레이어가 붙어 있는 PreLoadMap 보다 먼저 불림 -> 여기서 로딩 화면을 준비하면 그대로 재생됨
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ULoadingScreenSubsystem::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULoadingScreenSubsystem::HandlePostLoadMap);

	// 세션 만들기 / 참가가 실패하면 걷음
	if (USessionSubsystem* Sessions = Collection.InitializeDependency<USessionSubsystem>())
	{
		Sessions->OnHostComplete.AddUniqueDynamic(this, &ULoadingScreenSubsystem::HandleHostComplete);
		Sessions->OnJoinComplete.AddUniqueDynamic(this, &ULoadingScreenSubsystem::HandleJoinComplete);
	}
}

void ULoadingScreenSubsystem::HandleHostComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful) Hide();
}

void ULoadingScreenSubsystem::HandleJoinComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful) Hide();
}

void ULoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	RemoveFromGameViewport();

	Super::Deinitialize();
}

FText ULoadingScreenSubsystem::MessageForMap(const FString& MapName)
{
	if (MapName.Contains(TEXT("Dungeon"))) return FText::FromString(TEXT("던전으로 이동하는 중..."));
	if (MapName.Contains(TEXT("Tavern")))  return FText::FromString(TEXT("주점으로 가는 중..."));
	if (MapName.Contains(TEXT("Lobby")))   return FText::FromString(TEXT("메인 화면으로 가는 중..."));
	return FText::FromString(TEXT("불러오는 중..."));
}

// =====================================================================
// 덮기 / 걷기
// =====================================================================

ULoadingScreenWidget* ULoadingScreenSubsystem::CreateLoadingWidget(ELoadingScreenStyle InStyle)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return nullptr;

	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
	TSubclassOf<ULoadingScreenWidget> Class = (InStyle == ELoadingScreenStyle::Full ? Settings->FullWidgetClass : Settings->LightWidgetClass).LoadSynchronous();
	if (!Class)
	{
		Class = InStyle == ELoadingScreenStyle::Full ? ULoadingScreenWidget::StaticClass() : ULoadingSpinnerWidget::StaticClass();
	}

	// 게임 인스턴스가 주인 -> 레벨이 바뀌어도 위젯이 안 사라짐
	return CreateWidget<ULoadingScreenWidget>(GI, Class);
}

ULoadingScreenWidget* ULoadingScreenSubsystem::GetOverlayWidget()
{
	TObjectPtr<ULoadingScreenWidget>& Widget = Style == ELoadingScreenStyle::Full ? FullWidget : LightWidget;
	if (!Widget)
	{
		Widget = CreateLoadingWidget(Style);
	}
	return Widget;
}

void ULoadingScreenSubsystem::Show(const FText& InMessage, ELoadingScreenStyle InStyle)
{
	// 큰 로딩 위에 가벼운 로딩은 무시
	if (bShowing && Style == ELoadingScreenStyle::Full && InStyle == ELoadingScreenStyle::Light) return;

	// 종류가 바뀌면 덮고 있던 걸 떼고 새로
	if (bShowing && Style != InStyle)
	{
		RemoveFromGameViewport();
	}

	Style = InStyle;
	Message = InMessage.IsEmpty() ? FText::FromString(TEXT("불러오는 중...")) : InMessage;

	if (ULoadingScreenWidget* Widget = GetOverlayWidget())
	{
		Widget->SetMessage(Message);
	}

	if (!bShowing)
	{
		UE_LOG(LogTerminusLoading, Log, TEXT("[Loading] 표시 (%s): %s"), Style == ELoadingScreenStyle::Full ? TEXT("전체") : TEXT("가벼움"), *Message.ToString());
	}
	bShowing = true;

	AddToGameViewport();
	RestartTimeout();
}

void ULoadingScreenSubsystem::ShowIfHidden(const FText& InMessage, ELoadingScreenStyle InStyle)
{
	// 안 덮었거나, 가벼운 걸 덮고 있는데 큰 로딩이 오면
	if (!bShowing || (Style == ELoadingScreenStyle::Light && InStyle == ELoadingScreenStyle::Full))
	{
		Show(InMessage, InStyle);
	}
}

void ULoadingScreenSubsystem::Hide()
{
	if (!bShowing) return;

	UE_LOG(LogTerminusLoading, Log, TEXT("[Loading] 숨김"));
	bShowing = false;
	RemoveFromGameViewport();
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle.Reset();
}

void ULoadingScreenSubsystem::AddToGameViewport()
{
	ULoadingScreenWidget* Widget = GetOverlayWidget();
	if (bInViewport || !Widget) return;

	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!Viewport) return;

	OverlaySlate = Widget->TakeWidget();
	Viewport->AddViewportWidgetContent(OverlaySlate.ToSharedRef(), LoadingZOrder);
	bInViewport = true;
}

void ULoadingScreenSubsystem::RemoveFromGameViewport()
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (Viewport && OverlaySlate.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(OverlaySlate.ToSharedRef());
	}
	bInViewport = false;
}

void ULoadingScreenSubsystem::RestartTimeout()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ULoadingScreenSubsystem::HandleTimeout),
		GetDefault<ULoadingScreenSettings>()->MaxShowSeconds);
}

bool ULoadingScreenSubsystem::HandleTimeout(float DeltaTime)
{
	UE_LOG(LogTerminusLoading, Warning, TEXT("[Loading] 준비 신호가 안 와서 로딩 화면을 걷음 (%s)"), *Message.ToString());
	TimeoutHandle.Reset();
	Hide();
	return false;   // 한 번만
}

// =====================================================================
// 레벨 이동
// =====================================================================

void ULoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	// PIE 창이 여러 개면 남의 인스턴스 소식도 옴
	if (WorldContext.OwningGameInstance != GetGameInstance()) return;

	// 레벨 이동은 언제나 크게 덮음 (게임 시작 포함). 이미 크게 덮고 있으면 문구 유지
	ShowIfHidden(MessageForMap(MapName), ELoadingScreenStyle::Full);

	// 맵 로드는 뷰포트 위젯을 전부 지움 -> 도착 후 다시 붙임
	bInViewport = false;

	// 불러오는 동안 게임이 멈춤 -> 무비 플레이어가 따로 그림 (에디터에선 동작 안 함)
	if (!GIsEditor && !IsRunningDedicatedServer())
	{
		MovieWidget = CreateLoadingWidget(ELoadingScreenStyle::Full);
		if (MovieWidget)
		{
			MovieWidget->SetMessage(Message);

			FLoadingScreenAttributes Attributes;
			Attributes.bAutoCompleteWhenLoadingCompletes = true;
			Attributes.bMoviesAreSkippable = false;
			Attributes.bWaitForManualStop = false;
			Attributes.MinimumLoadingScreenDisplayTime = GetDefault<ULoadingScreenSettings>()->MinMapLoadSeconds;
			Attributes.WidgetLoadingScreen = MovieWidget->TakeWidget();
			GetMoviePlayer()->SetupLoadingScreen(Attributes);
		}
	}
}

void ULoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()) return;

	MovieWidget = nullptr;

	// 새 레벨에서도 준비될 때까지 계속 덮음 (seamless 트래블은 뷰포트가 그대로라 지웠다 다시 붙임)
	if (bShowing)
	{
		RemoveFromGameViewport();
		AddToGameViewport();
		RestartTimeout();
	}
}
