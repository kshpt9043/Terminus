#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class ADungeonArea;
class ATerminusBattler;
class UButton;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * 전투 HUD. 내가 보고 있는 구역에서 전투가 진행 중일 때만 보임
 *
 * 기획 전투 UI 시안 배치:
 *  - 위: n번째 방 / 지금 누구 턴인지
 *  - 왼쪽 아래: 에너지 + 기본 스킬 3개 / 강화 에너지
 *  - 오른쪽 아래: 턴 종료 + "체력 [최대/현재] 힘 방어 회피"
 *  - 캐릭터 / 몬스터 위치를 따라다니는 표시: 몬스터 머리 위 행동 예고, 발밑 체력바, 대상 선택 버튼
 *
 * 레이아웃은 이 클래스를 부모로 한 WBP(WBP_CombatUI)에서 잡음. 아래 위젯은 전부 BindWidget 이라 WBP 에 같은 이름으로 있어야 컴파일됨
 * 이 C++ 클래스를 WBP 없이 그대로 쓰면 C++ 이 기본 배치를 직접 만든다 -> 에셋 없이도 테스트 가능
 *
 * 스킬 사용: 적 1명 / 아군 1명 스킬은 누르면 대상 고르기 모드 -> 대상 클릭. 우클릭이나 같은 스킬을 다시 누르면 취소
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
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SkillButton0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SkillButton1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SkillButton2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SkillLabel0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SkillLabel1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SkillLabel2;

	// 장착한 강화 스킬 3칸 (강화 에너지로 사용). WBP 에 없으면 C++ 이 화면 아래 가운데에 임시로 만듦
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> EnhanceButton0;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> EnhanceButton1;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> EnhanceButton2;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EnhanceLabel0;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EnhanceLabel1;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EnhanceLabel2;

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
	TArray<FBattlerTag> Tags;

	// 표시를 다시 만들어야 하는지 판단용 (몬스터 / 플레이어 구성이 바뀌면)
	TArray<TWeakObjectPtr<ATerminusBattler>> TaggedBattlers;

	// 대상을 고르는 중인 스킬 칸 (0~2 기본, 3~5 강화. INDEX_NONE = 아님)
	int32 PendingSkillIndex = INDEX_NONE;

	// 보일지 말지는 타이머가 정함. 숨긴(Collapsed) 위젯은 NativeTick 이 안 불려서 Tick 으로는 다시 못 켬
	FTimerHandle VisibilityTimer;
	void UpdateVisibility();

	void BuildDefaultLayout();

	// WBP 에 강화 스킬 버튼이 없을 때 임시 버튼 줄 (TagLayer 위, 화면 아래 가운데)
	void BuildEnhanceFallback();

	// 강화 스킬 버튼 하나 만들기 (기본 배치 / 임시 줄 공용)
	void MakeEnhanceButton(class UPanelWidget* Parent, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutLabel);
	void RebuildTags();
	void UpdateTags();
	void UpdatePanels();

	void OnSkillClicked(int32 SkillIndex);
	void CancelTargeting();

	UFUNCTION() void HandleSkill0Clicked();
	UFUNCTION() void HandleSkill1Clicked();
	UFUNCTION() void HandleSkill2Clicked();
	UFUNCTION() void HandleEnhance0Clicked();
	UFUNCTION() void HandleEnhance1Clicked();
	UFUNCTION() void HandleEnhance2Clicked();
	UFUNCTION() void HandleEndTurnClicked();
	UFUNCTION() void HandleTargetClicked();
};
