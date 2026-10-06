#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Online/SessionSubsystem.h"
#include "Widgets/Common/LoadingScreenWidget.h"
#include "LoadingScreenSubsystem.generated.h"

class ULoadingScreenWidget;
class SWidget;
class UWorld;
struct FWorldContext;

// 로딩 화면 설정. Project Settings > Game > Terminus Loading Screen
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Terminus Loading Screen"))
class TERMINUS_API ULoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULoadingScreenSettings() { CategoryName = TEXT("Game"); }

	// 전체 로딩 WBP (ULoadingScreenWidget 을 부모로). 레벨 이동 같은 큰 로딩. 비워 두면 C++ 기본 모양
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	TSoftClassPtr<ULoadingScreenWidget> FullWidgetClass;

	// 가벼운 로딩 WBP (ULoadingSpinnerWidget 을 부모로). 검색 / 잠깐 대기. 비워 두면 C++ 기본 모양 (블러 + 반투명 + 아이콘)
	UPROPERTY(Config, EditAnywhere, Category = "Loading")
	TSoftClassPtr<ULoadingScreenWidget> LightWidgetClass;

	// 준비 신호가 안 와도 이 시간이 지나면 걷음 (멈춘 것처럼 갇히지 않게)
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "1.0"))
	float MaxShowSeconds = 30.f;

	// 레벨을 불러오는 동안 최소한 보여줄 시간 (너무 빨리 깜빡이지 않게)
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "0.0"))
	float MinMapLoadSeconds = 0.3f;
};

/**
 * 로딩 화면. 기다리는 동안 화면을 덮어 "로딩 중" 인지 "안 된 건지" 구분되게 하고, 아래를 못 누르게 함
 *  - 두 가지: Full(레벨 이동 등 큰 로딩, 불투명 + 문구) / Light(검색 / 잠깐 대기, 블러 반투명 + 아이콘만)
 *    Light 를 띄운 중에 Full 이 오면 Full 로 바뀜. Full 중에 Light 요청은 무시
 *
 *  - Show(문구): 세션 만들기 / 참가 / 나가기 같은 비동기 작업, 레벨 이동 직전 (주점 목록 검색은 목록 패널 안에서 따로)
 *  - 레벨을 불러오는 동안(게임이 멈춤)에는 무비 플레이어가 같은 위젯을 그림 (패키징 빌드. 에디터에선 마지막 화면이 멈춰 보임)
 *  - 레벨이 바뀌어도 계속 덮고 있다가, 도착한 화면이 준비되면(메인 메뉴 / 주점 슬롯 / 던전 시작 화면) Hide
 *  - 레벨 이동은 Show 를 안 불러도 자동으로 덮음 (게임 시작 포함)
 *  - 준비 신호가 안 오면 MaxShowSeconds 뒤 자동으로 걷음
 *  - 덮고 있는 동안 ESC / 채팅 Enter 도 무시
 */
UCLASS()
class TERMINUS_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULoadingScreenSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 덮기. 이미 덮고 있으면 문구 / 종류만 바꿈 (Full 위에 Light 는 무시)
	void Show(const FText& Message, ELoadingScreenStyle Style = ELoadingScreenStyle::Full);

	// 아직 안 덮었거나 더 가벼운 걸로 덮고 있을 때만 (이미 더 구체적인 문구로 덮고 있으면 그대로)
	void ShowIfHidden(const FText& Message, ELoadingScreenStyle Style = ELoadingScreenStyle::Full);

	// 걷기 (준비 완료 / 실패)
	void Hide();

	bool IsShowing() const { return bShowing; }
	ELoadingScreenStyle GetStyle() const { return Style; }

	// 맵 이름에 맞는 기본 문구
	static FText MessageForMap(const FString& MapName);

private:
	bool bShowing = false;
	bool bInViewport = false;
	FText Message;
	ELoadingScreenStyle Style = ELoadingScreenStyle::Full;

	// 뷰포트에 덮는 것 (종류별) / 무비 플레이어가 그리는 것 (한 위젯을 두 곳에 동시에 못 붙여서 따로)
	UPROPERTY()
	TObjectPtr<ULoadingScreenWidget> FullWidget;

	UPROPERTY()
	TObjectPtr<ULoadingScreenWidget> LightWidget;

	UPROPERTY()
	TObjectPtr<ULoadingScreenWidget> MovieWidget;

	TSharedPtr<SWidget> OverlaySlate;
	FTSTicker::FDelegateHandle TimeoutHandle;

	ULoadingScreenWidget* CreateLoadingWidget(ELoadingScreenStyle InStyle);
	ULoadingScreenWidget* GetOverlayWidget();
	void AddToGameViewport();
	void RemoveFromGameViewport();
	void RestartTimeout();
	bool HandleTimeout(float DeltaTime);

	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// 세션 작업 결과. 실패면 걷음 (성공이면 이어서 레벨 이동이라 그대로)
	UFUNCTION() void HandleHostComplete(bool bWasSuccessful);
	UFUNCTION() void HandleJoinComplete(bool bWasSuccessful);
};
