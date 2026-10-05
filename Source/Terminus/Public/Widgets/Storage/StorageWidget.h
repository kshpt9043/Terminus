#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StorageWidget.generated.h"

class UButton;
class UTextBlock;
class UUniformGridPanel;
class UItemSlotWidget;

UENUM(BlueprintType)
enum class EStorageTab : uint8
{
	Relic,
	Skill
};

/**
 * 창고 (메인 메뉴). 기획 UI 레퍼런스 > 창고
 *  - 첫 화면(Hub): [유물 창고] [스킬 창고] 카드 + 돌아가기
 *  - 목록 화면(List): 격자 + 상세 + 돌아가기(-> 첫 화면) / 메인화면(닫기)
 *  - 유물: 도감처럼 종류별 하나씩, 개수 제한 없음 (UTerminusProfileSubsystem::GetStoredRelics)
 *  - 스킬: 영구 보유 스킬 (GetOwnedSkills). 런 시작 때 이 중에서 하나 장착
 *  - 우상단 보유 골드. ESC = 한 단계 뒤로
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Storage) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * HubPanel 이 없으면 예전처럼 탭 방식 (RelicTabButton / SkillTabButton 이 탭)
 */
UCLASS()
class TERMINUS_API UStorageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 첫 화면부터 (HubPanel 이 없으면 Tab 목록을 바로)
	void Open(EStorageTab Tab = EStorageTab::Relic);
	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// ---- 첫 화면
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> HubPanel;

	// 유물 창고 / 스킬 창고 카드 (HubPanel 이 없으면 탭 버튼)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RelicTabButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> SkillTabButton;

	// 첫 화면의 돌아가기 (= 닫기)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	// ---- 목록 화면
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ListPanel;

	// "유물 창고" / "스킬 창고"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ListTitleText;

	// 목록의 돌아가기 (-> 첫 화면) / 메인화면 (닫기)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> MainMenuButton;

	// 칸들이 들어갈 격자
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> ItemGrid;

	// "유물 5개" 같은 개수
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;

	// 오른쪽 상세 (선택한 칸)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailInfo;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailDesc;

	// 보유 골드 (두 화면 공통, 우상단)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GoldText;

	// 칸 위젯 (공용 칸 UItemSlotWidget 또는 그걸 부모로 한 WBP). 비워 두면 C++ 기본 칸
	UPROPERTY(EditAnywhere, Category = "Storage")
	TSubclassOf<UItemSlotWidget> SlotClass;

	// 한 줄 칸 수 / 최소 칸 수 (빈 칸 포함. 내용이 더 많으면 줄이 늘어남)
	UPROPERTY(EditAnywhere, Category = "Storage")
	int32 Columns = 6;

	UPROPERTY(EditAnywhere, Category = "Storage")
	int32 MinSlots = 24;

	// 탭 방식일 때 탭 버튼 색 (선택 / 아님)
	UPROPERTY(EditAnywhere, Category = "Storage")
	FLinearColor ActiveTabColor = FLinearColor(1.f, 0.85f, 0.35f);

	UPROPERTY(EditAnywhere, Category = "Storage")
	FLinearColor InactiveTabColor = FLinearColor(0.45f, 0.45f, 0.45f);

private:
	EStorageTab CurrentTab = EStorageTab::Relic;
	bool bInList = false;

	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> Slots;

	TWeakObjectPtr<UItemSlotWidget> SelectedSlot;
	FDelegateHandle StorageChangedHandle;

	bool UsesHub() const { return HubPanel != nullptr; }

	void BuildDefaultLayout();
	void ShowHub();
	void ShowList(EStorageTab Tab);
	void Refresh();
	void RefreshGold();
	void ShowDetail(const UItemSlotWidget* InSlot);
	void HandleSlotClicked(UItemSlotWidget* InSlot);
	void HandleEscape();

	UFUNCTION() void HandleRelicTab();
	UFUNCTION() void HandleSkillTab();
	UFUNCTION() void HandleClose();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleGoldChanged(int32 NewGold, int32 Delta);
};
