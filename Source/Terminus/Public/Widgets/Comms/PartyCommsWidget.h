#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Comms/PartyCommsTypes.h"
#include "PartyCommsWidget.generated.h"

class ADungeonArea;
class ATerminusBattler;
class ATerminusPlayerState;
class UBorder;
class UButton;
class UCanvasPanel;
class UCommsMenuWidget;
class UDungeonCombatComponent;
class UTextBlock;
class UVerticalBox;
struct FKeyEvent;
struct FPointerEvent;

/**
 * 파티 소통 도구 화면 (멀티 전용, PC 가 만듦). 채팅 말고 전투 중 빠르게 의사를 나누는 것들
 *
 *  1. 행동 계획 공유: 동료가 스킬 대상을 고르는 중이거나 Shift 로 '예약' 하면, 대상 머리 위에
 *     "홍길동: 신성 강타 피해 8" 이 뜨고, 적 머리 위엔 계획된 피해 합 "예상 피해 약 14 / 15" 가 뜸
 *  2. 핑: 전투 중 배틀러를 휠 클릭(또는 Alt + 클릭) -> 메뉴(적: 집중 공격 / 위험, 아군: 보호 필요 / 회복 필요)
 *  3. 퀵챗: T -> 문구 메뉴 (숫자 키로 바로). 채팅창에 한 줄 + 보낸 사람 머리 위 말풍선
 *  4. 재촉: 내 턴을 끝냈는데 아직 안 끝낸 사람이 있으면 오른쪽 아래 '재촉' 버튼 -> 그 사람 화면에 알림
 *
 * 화면은 C++ 이 전부 만듦 (WBP 필요 없음). 위치 / 키 / 시간은 이 클래스를 부모로 한 WBP 의 디테일에서 바꿀 수 있음
 * 바탕은 클릭을 통과시킴 -> 아래 전투 HUD / 지도 조작을 막지 않음
 *
 * 키 / 마우스는 Slate 입력 전처리기로 받음 (채팅 Enter, ESC 와 같은 방식) -> 포커스가 어디 있든 동작
 * 글을 쓰는 중(입력칸 포커스)이거나, 팝업이 떠 있거나, 로딩 화면이면 가로채지 않음
 */
UCLASS()
class TERMINUS_API UPartyCommsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---- 서버에서 받은 것 (PC 의 Client_ RPC 가 넘김)
	void HandlePlan(const FCombatPlan& Plan);
	void HandlePing(const FPartyPing& Ping);
	void HandleQuickChat(ATerminusPlayerState* Sender, EQuickChat Kind);
	void HandleNudged(ATerminusPlayerState* Sender);

	// ---- 입력 전처리기가 부름. 키 / 클릭을 먹었으면 true
	bool HandleKeyDown(const FKeyEvent& InKeyEvent);
	bool HandleMouseDown(const FPointerEvent& InMouseEvent);

	void OpenQuickChatMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 퀵챗 메뉴 키
	UPROPERTY(EditAnywhere, Category = "Party Comms|Input")
	FKey QuickChatKey = EKeys::T;

	// 핑 메뉴: 휠 클릭. bAltClickPing 이면 Alt + 왼쪽 클릭도 (휠 없는 마우스 / 노트북)
	UPROPERTY(EditAnywhere, Category = "Party Comms|Input")
	bool bAltClickPing = true;

	// 배틀러 몸 중심에서 이 거리(화면 px) 안을 누르면 그 배틀러를 고른 걸로
	UPROPERTY(EditAnywhere, Category = "Party Comms|Input")
	float PickRadius = 70.f;

	// 캐릭터 머리 / 몸 중심 높이 (월드 단위, 발 기준). 전투 HUD 와 같은 값
	UPROPERTY(EditAnywhere, Category = "Party Comms|Layout")
	float HeadHeight = 72.f;

	UPROPERTY(EditAnywhere, Category = "Party Comms|Layout")
	float BodyCenterHeight = 32.f;

	// 머리 위 표시를 머리보다 얼마나 더 위에 둘지 (화면 px). 몬스터 행동 예고 글자 위로 올리려고
	UPROPERTY(EditAnywhere, Category = "Party Comms|Layout")
	float HeadScreenOffset = 30.f;

	// 재촉 버튼 위치 (화면 오른쪽 아래 기준). 전투 HUD 의 턴 종료 / 유물 줄 위
	UPROPERTY(EditAnywhere, Category = "Party Comms|Layout")
	FVector2D NudgeButtonPosition = FVector2D(-24.f, -200.f);

	UPROPERTY(EditAnywhere, Category = "Party Comms|Layout")
	int32 FontSize = 15;

	// 핑 / 말풍선 / 재촉 알림이 떠 있는 시간(초)
	UPROPERTY(EditAnywhere, Category = "Party Comms|Timing")
	float PingDuration = 8.f;

	UPROPERTY(EditAnywhere, Category = "Party Comms|Timing")
	float BubbleDuration = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Party Comms|Timing")
	float ToastDuration = 3.f;

	// 재촉 버튼 다시 누를 수 있기까지(초). 서버도 따로 막음
	UPROPERTY(EditAnywhere, Category = "Party Comms|Timing")
	float NudgeCooldown = 10.f;

	// 핑 / 퀵챗 메뉴 클래스. 비워 두면 C++ 기본 모양
	UPROPERTY(EditAnywhere, Category = "Party Comms")
	TSubclassOf<UCommsMenuWidget> MenuClass;

private:
	// 배틀러 하나의 머리 위 표시 묶음 (위에서부터 말풍선 / 핑 / 계획 / 예상 피해)
	struct FCommsTag
	{
		TWeakObjectPtr<ATerminusBattler> Battler;
		TWeakObjectPtr<UVerticalBox> Box;
		TWeakObjectPtr<UBorder> Bubble;
		TWeakObjectPtr<UTextBlock> BubbleText;
		TWeakObjectPtr<UTextBlock> PingText;
		TWeakObjectPtr<UTextBlock> PlanText;
		TWeakObjectPtr<UTextBlock> EstimateText;
	};

	struct FActivePing
	{
		FPartyPing Ping;
		double ExpireTime = 0.0;
	};

	struct FBubble
	{
		TWeakObjectPtr<ATerminusPlayerState> Sender;
		FText Text;
		double ExpireTime = 0.0;
	};

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UTextBlock> ToastText;
	UPROPERTY() TObjectPtr<UButton> NudgeButton;
	UPROPERTY() TObjectPtr<UTextBlock> NudgeLabel;
	UPROPERTY() TObjectPtr<UCommsMenuWidget> Menu;

	TArray<FCommsTag> Tags;
	TArray<TWeakObjectPtr<ATerminusBattler>> TaggedBattlers;

	TMap<TWeakObjectPtr<ATerminusPlayerState>, FCombatPlan> Plans;
	TArray<FActivePing> Pings;
	TArray<FBubble> Bubbles;
	double ToastExpireTime = 0.0;
	double NextNudgeTime = 0.0;

	TSharedPtr<class FPartyCommsInputProcessor> Processor;

	void BuildLayout();
	void RebuildTags(const TArray<ATerminusBattler*>& Battlers);
	void UpdateTags(ADungeonArea* Area, UDungeonCombatComponent* Combat);
	void UpdateNudgeButton(ADungeonArea* Area, UDungeonCombatComponent* Combat);
	void ShowToast(const FText& Message);

	// 지금 보고 있는 구역의 배틀러 (몬스터 + 플레이어)
	void GatherBattlers(ADungeonArea* Area, UDungeonCombatComponent* Combat, TArray<ATerminusBattler*>& Out) const;

	// Viewer 편에서 Battler 가 적인가 (몬스터, 배신 전투면 상대 편 플레이어)
	static bool IsEnemyOf(const UDungeonCombatComponent* Combat, const ATerminusPlayerState* Viewer, const ATerminusBattler* Battler);

	// 이 계획이 이 배틀러에게 향하는가 (1명 대상 / 전체 / 자신)
	static bool PlanTouches(const FCombatPlan& Plan, const UDungeonCombatComponent* Combat, const ATerminusBattler* Battler);

	// 화면에 보여 줄 계획인가 (지금 보는 구역의 플레이어 턴, 같은 사이클, 아직 턴 안 끝낸 사람)
	bool IsPlanShown(const FCombatPlan& Plan, const ADungeonArea* Area, const UDungeonCombatComponent* Combat) const;

	FString DescribePlan(const FCombatPlan& Plan) const;

	// 핑 / 채팅 줄에 쓰는 이름 ("적 2", 플레이어 이름)
	FString DescribeTarget(const AActor* Target) const;

	// 핑 메뉴를 열 수 있는 상태에서 마우스 아래 배틀러. 없으면 nullptr
	ATerminusBattler* FindPingTarget(const FVector2D& ScreenSpacePosition) const;
	void OpenPingMenu(ATerminusBattler* Target, const FVector2D& LocalPosition);

	UCommsMenuWidget* GetMenu();

	// 키 입력을 받아도 되는 상태인가 (이 창이 활성, 글 쓰는 중 아님, 로딩 아님)
	bool CanTakeKeyboard() const;
	bool IsOwnWindowUnder(const FVector2D& ScreenSpacePosition) const;

	UFUNCTION() void HandleNudgeClicked();
};
