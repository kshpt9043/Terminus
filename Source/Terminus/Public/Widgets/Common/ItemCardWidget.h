#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "ItemCardWidget.generated.h"

class UButton;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UItemSlotWidget;

/**
 * 스킬 / 유물 칸 + 설명 한 줄 + 버튼 한 개짜리 카드. 상점 진열 칸, 전투 보상의 유물 카드가 같이 씀
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 *  - ItemSlot : 칸 (WBP_ItemSlot 같은 걸 넣어 두면 그대로 씀. 없으면 SlotHolder 안에 C++ 이 만듦)
 *  - SlotHolder : ItemSlot 을 안 뒀을 때 칸이 들어갈 자리
 *  - CaptionText : 설명 / 단계 ("+1", 유물 설명). 비면 숨김
 *  - ActionButton + ActionText : "구매 30" 같은 버튼. 라벨이 비면 숨김
 * 그 WBP 는 상점의 CardClass, 보상 화면의 RelicCardClass 에 지정
 * 블루프린트에서 ItemRow / bRelic 을 읽을 수 있고, 바뀔 때마다 OnCardUpdated 가 불림
 */
UCLASS()
class TERMINUS_API UItemCardWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	// 칸을 만들기 전에 (첫 SetItem 전에) 불러야 적용됨
	void SetSlotClass(TSubclassOf<UItemSlotWidget> InClass) { if (InClass) SlotClass = InClass; }

	void SetItem(bool bInRelic, FName Row, FVector2D SlotSize = FVector2D::ZeroVector, bool bShowName = true);
	void SetCaption(const FString& Caption);
	void SetAction(const FString& Label, bool bEnabled, const FLinearColor& DefaultTint = FLinearColor::White);

	// 기본 모양에서 버튼 글자색만 바꾸기 (상점: 재화 부족이면 경고색). SetAction 뒤에 부를 것
	void SetActionLabelColor(const FLinearColor& Color);

	// 기본 모양에서 설명 줄 폭 / 글자 크기
	void SetCaptionStyle(float Width, int32 FontSize, const FLinearColor& Color);

	UItemSlotWidget* GetItemSlot() const { return ItemSlot; }

	FSimpleDelegate OnAction;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Item Card")
	void OnCardUpdated();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UItemSlotWidget> ItemSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> SlotHolder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> CaptionSizeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CaptionText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ActionButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ActionText;

	UPROPERTY(EditAnywhere, Category = "Item Card")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(BlueprintReadOnly, Category = "Item Card")
	FName ItemRow;

	UPROPERTY(BlueprintReadOnly, Category = "Item Card")
	bool bRelic = false;

private:
	bool bDefaultLayout = false;

	void BuildDefaultLayout();

	UFUNCTION()
	void HandleAction();
};
