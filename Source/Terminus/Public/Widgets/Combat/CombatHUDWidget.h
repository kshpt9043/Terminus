#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class ADungeonArea;
class ATerminusBattler;
class UButton;
class UCanvasPanel;
class UPanelWidget;
class UItemSlotWidget;
class URelicBarWidget;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * 전투 HUD. 내가 보고 있는 구역에서 전투가 진행 중일 때만 보임
 *
 * 기획 전투 UI 시안 배치:
 *  - 위: n번째 방 / 지금 누구 턴인지
 *  - 왼쪽 아래: 에너지 + 기본 스킬 3개 / 강화 에너지 + 강화 스킬 칸
 *  - 오른쪽 아래: 턴 종료 + "체력 [최대/현재] 힘 방어 회피"
 *  - 캐릭터 / 몬스터 위치를 따라다니는 표시: 몬스터 머리 위 행동 예고, 발밑 체력바, 대상 선택 버튼
 *
 * 레이아웃은 이 클래스를 부모로 한 WBP(WBP_CombatUI)에서 잡음. 아래 위젯은 전부 BindWidget 이라 WBP 에 같은 이름으로 있어야 컴파일됨
 * 이 C++ 클래스를 WBP 없이 그대로 쓰면 C++ 이 기본 배치를 직접 만든다 -> 에셋 없이도 테스트 가능
 *
 * 스킬 칸: SkillBox / EnhanceSkillBox 상자만 두면 코드가 SkillSlotClass(WBP_SkillSlot) 로 칸을 만들어 채움
 *  - 칸 번호 0~2 = 기본 스킬, 3~ = 강화 스킬. 강화 칸 수는 캐릭터 EnhanceSlots 를 따름 (최대 3)
 *
 * 스킬 사용: 적 1명 / 아군 1명 스킬은 누르면 대상 고르기 모드 -> 대상 클릭. 우클릭이나 같은 스킬을 다시 누르면 취소
 *
 * 행동 계획 공유 (UPartyCommsWidget 이 표시): 대상 고르는 중이면 마우스가 올라간 대상이 동료 화면에 실시간으로 보임
 *  - Shift + 대상 클릭 (또는 자신 / 전체 스킬을 Shift + 클릭) = 쓰지 않고 '예약' 만 공유. 이번 사이클 동안 유지
 *  - 예약은 스킬을 쓰거나, 턴 종료하거나, 같은 스킬을 Shift + 클릭하면 사라짐
 */
UCLASS()
class TERMINUS_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 보여 줄 구역. nullptr 이면 숨김 (지도로 돌아갔을 때)
	void SetArea(ADungeonArea* InArea);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ---- 위젯 (WBP 에서 같은 이름으로 만들면 그걸 씀)

	// 캐릭터 / 몬스터를 따라다니는 표시들이 올라가는 전체 화면 캔버스 (앵커 전체 늘이기)
	// 루트로 써서 다른 위젯을 이 안에 배치해도 됨. 코드는 자기가 만든 표시만 넣고 지움
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> TagLayer;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> RoomText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HintText;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EnergyText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SkillEnergyText;
	// 기본 스킬 칸이 들어갈 상자 (가로 상자 추천). 있던 자식은 지우고 칸을 채움
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> SkillBox;

	// 강화 스킬 칸이 들어갈 상자
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> EnhanceSkillBox;

	// 보유 유물 줄 (오른쪽 아래). WBP 에 없으면 C++ 이 턴 종료 위쪽에 만듦
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<URelicBarWidget> RelicBar;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EndTurnButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;

	// 캐릭터 머리 위 / 몸 중심 높이 (월드 단위, 발 기준). 캐릭터 스프라이트가 64 라 기본값이 그에 맞춤
	UPROPERTY(EditAnywhere, Category = "Combat HUD")
	float HeadHeight = 72.f;

	UPROPERTY(EditAnywhere, Category = "Combat HUD")
	float BodyCenterHeight = 32.f;

	// 대상 선택 버튼 크기 (화면 px)
	UPROPERTY(EditAnywhere, Category = "Combat HUD")
	FVector2D TargetButtonSize = FVector2D(90.f, 110.f);

	// 스킬 칸 위젯 (공용 칸 UItemSlotWidget 또는 그걸 부모로 한 WBP). 비워 두면 C++ 기본 칸
	UPROPERTY(EditAnywhere, Category = "Combat HUD")
	TSubclassOf<UItemSlotWidget> SkillSlotClass;

	// 칸 사이 간격 (상자가 가로 상자일 때)
	UPROPERTY(EditAnywhere, Category = "Combat HUD")
	float SkillSlotSpacing = 8.f;

private:
	// 캐릭터 / 몬스터 하나를 따라다니는 표시 묶음
	struct FBattlerTag
	{
		TWeakObjectPtr<ATerminusBattler> Battler;
		bool bMonster = false;
		int32 Index = INDEX_NONE;   // 몬스터면 전투의 Monsters 인덱스, 플레이어면 구역 Occupants 인덱스

		TWeakObjectPtr<UTextBlock> IntentText;      // 몬스터 머리 위
		TWeakObjectPtr<UVerticalBox> HealthBox;     // 발밑 (체력바 + 숫자)
		TWeakObjectPtr<UProgressBar> HealthBar;
		TWeakObjectPtr<UTextBlock> HealthText;
		TWeakObjectPtr<UButton> TargetButton;
	};

	TWeakObjectPtr<ADungeonArea> Area;

	// 층 표시용 ("n층 n번째 방")
	TWeakObjectPtr<class AMapManager> CachedMap;
	TArray<FBattlerTag> Tags;

	// 표시를 다시 만들어야 하는지 판단용 (몬스터 / 플레이어 구성이 바뀌면)
	TArray<TWeakObjectPtr<ATerminusBattler>> TaggedBattlers;

	// 대상을 고르는 중인 스킬 칸 (0~2 기본, 3~5 강화. INDEX_NONE = 아님)
	int32 PendingSkillIndex = INDEX_NONE;

	// 만든 칸들. 인덱스 = 칸 번호 (0~2 기본, 3~ 강화)
	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> SkillSlots;

	// 지금 만들어 둔 강화 칸 수
	int32 NumEnhanceSlots = INDEX_NONE;

	// 보일지 말지는 타이머가 정함. 숨긴(Collapsed) 위젯은 NativeTick 이 안 불려서 Tick 으로는 다시 못 켬
	FTimerHandle VisibilityTimer;
	void UpdateVisibility();

	void BuildDefaultLayout();

	// 기본 칸 3개 + 강화 칸 EnhanceCount 개를 다시 만듦
	void BuildSkillSlots(int32 EnhanceCount);
	UItemSlotWidget* AddSkillSlot(UPanelWidget* Box, int32 SlotIndex, bool bFirst);
	void HandleSkillSlotClicked(UItemSlotWidget* ClickedSlot);
	void RebuildTags();
	void UpdateTags();
	void UpdatePanels();

	void OnSkillClicked(int32 SkillIndex);
	void CancelTargeting();

	// ---- 행동 계획 공유
	// Shift 로 예약한 계획 (INDEX_NONE = 없음). 예약한 사이클이 지나면 버림
	int32 DeclaredSkillIndex = INDEX_NONE;
	TWeakObjectPtr<AActor> DeclaredTarget;
	int32 DeclaredCycle = INDEX_NONE;

	void DeclarePlan(int32 SkillIndex, AActor* Target);
	void ClearDeclaredPlan();

	// 지금 계획(대상 고르는 중 / 예약)을 PC 에 알림 -> 바뀐 것만 서버로
	void UpdateSharedPlan();

	// 대상 고르는 중 마우스가 올라가 있는 대상 배틀러. 없으면 nullptr
	ATerminusBattler* GetHoveredTarget() const;

	// 스킬을 실제로 썼을 때 (예약 / 공유 계획 지우기)
	void NotifySkillUsed();

	UFUNCTION() void HandleEndTurnClicked();
	UFUNCTION() void HandleTargetClicked();
};
