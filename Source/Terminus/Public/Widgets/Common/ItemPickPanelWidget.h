#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemPickPanelWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UItemSlotWidget;
struct FRelicRow;

DECLARE_DELEGATE_OneParam(FOnItemPicked, UItemSlotWidget* /*Slot*/);
DECLARE_DELEGATE(FOnItemPickCancelled);

/**
 * "칸이 가득 찼습니다. 바꿀 것을 고르세요" 패널. 보상 / 이벤트 / 상점이 같이 씀
 *
 *  - ShowEnhanceSkills: 내 강화 스킬 칸 전부. 칸 번호(GetSlotIndex) = 강화 칸 번호
 *  - ShowRelics: 내 유물 중 Filter 를 통과한 것. 고른 유물은 GetItemRow
 *  - 칸을 누르면 OnPicked, 취소를 누르면 OnCancelled. 패널은 스스로 닫히지 않음 (쓰는 쪽이 Hide)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 쓰는 쪽 화면은 PickPanelClass 에 그 WBP 를 넣으면 됨
 */
UCLASS()
class TERMINUS_API UItemPickPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowEnhanceSkills(const FText& Guide);
	void ShowRelics(const FText& Guide, TFunctionRef<bool(const FRelicRow&)> Filter);
	void Hide();

	// 칸 위젯 / 크기 (비우면 칸 기본값)
	void SetSlotClass(TSubclassOf<UItemSlotWidget> InClass) { if (InClass) SlotClass = InClass; }
	void SetSlotSize(FVector2D InSize) { SlotSize = InSize; }

	FOnItemPicked OnPicked;
	FOnItemPickCancelled OnCancelled;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GuideText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> SlotBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;

	UPROPERTY(EditAnywhere, Category = "Item Pick")
	TSubclassOf<UItemSlotWidget> SlotClass;

	// 0 이면 칸 기본 크기
	UPROPERTY(EditAnywhere, Category = "Item Pick")
	FVector2D SlotSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Item Pick")
	float SlotSpacing = 12.f;

private:
	void BuildDefaultLayout();
	void Begin(const FText& Guide);
	UItemSlotWidget* AddSlot();
	void HandleSlotClicked(UItemSlotWidget* ClickedSlot);

	UFUNCTION()
	void HandleCancel();
};
