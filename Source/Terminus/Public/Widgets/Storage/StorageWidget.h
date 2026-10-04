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
 * 창고 (메인 메뉴). 영구 보관 중인 유물 / 보유 스킬을 탭으로 나눠 인벤토리처럼 보여줌
 *  - 유물: 정산에서 팔지 않고 보관한 것 (UTerminusProfileSubsystem::GetStoredRelics)
 *  - 스킬: 영구 보유 스킬 (GetOwnedSkills). 런 시작 때 이 중에서 하나 장착
 *
 * 칸을 누르면 오른쪽 상세에 이름 / 분류 / 설명. ESC 또는 닫기 버튼으로 닫힘
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Storage) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 루트는 화면 전체 (뒤를 어둡게 덮는 패널) 로 둘 것
 */
UCLASS()
class TERMINUS_API UStorageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(EStorageTab Tab = EStorageTab::Relic);
	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RelicTabButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> SkillTabButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	// 칸들이 들어갈 격자
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> ItemGrid;

	// "유물 5개" 같은 개수
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;

	// 오른쪽 상세 (선택한 칸)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailInfo;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailDesc;

	// 칸 위젯 (공용 칸 UItemSlotWidget 또는 그걸 부모로 한 WBP). 비워 두면 C++ 기본 칸
	UPROPERTY(EditAnywhere, Category = "Storage")
	TSubclassOf<UItemSlotWidget> SlotClass;

	// 한 줄 칸 수 / 최소 칸 수 (빈 칸 포함. 내용이 더 많으면 줄이 늘어남)
	UPROPERTY(EditAnywhere, Category = "Storage")
	int32 Columns = 6;

	UPROPERTY(EditAnywhere, Category = "Storage")
	int32 MinSlots = 24;

	// 탭 버튼 색 (선택 / 아님)
	UPROPERTY(EditAnywhere, Category = "Storage")
	FLinearColor ActiveTabColor = FLinearColor(1.f, 0.85f, 0.35f);

	UPROPERTY(EditAnywhere, Category = "Storage")
	FLinearColor InactiveTabColor = FLinearColor(0.45f, 0.45f, 0.45f);

private:
	EStorageTab CurrentTab = EStorageTab::Relic;

	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> Slots;

	TWeakObjectPtr<UItemSlotWidget> SelectedSlot;

	FDelegateHandle StorageChangedHandle;

	void BuildDefaultLayout();
	void SetTab(EStorageTab Tab);
	void Refresh();
	void ShowDetail(const UItemSlotWidget* InSlot);

	void HandleSlotClicked(UItemSlotWidget* InSlot);

	UFUNCTION() void HandleRelicTab();
	UFUNCTION() void HandleSkillTab();
	UFUNCTION() void HandleClose();
};
