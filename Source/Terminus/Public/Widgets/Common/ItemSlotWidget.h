#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemSlotWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class USizeBox;
class UTextBlock;
class USkillTooltipWidget;
class UItemSlotWidget;

DECLARE_DELEGATE_OneParam(FOnItemSlotClicked, UItemSlotWidget* /*Slot*/);

// 칸에 든 것
UENUM()
enum class EItemSlotKind : uint8
{
	Empty,
	Skill,
	Relic
};

/**
 * 스킬 / 유물 칸 공용 위젯. 전투 HUD 스킬 칸, 창고 격자, 보유 유물 줄이 모두 이걸 씀
 *
 *  - 아이콘 (없으면 이름 앞 두 글자) + 이름 (SetShowName 으로 숨길 수 있음)
 *  - 테두리 색: 유물은 등급, 스킬은 공격 / 방어 / 특수. 선택(SetSelected)이면 강조 색
 *  - 마우스를 올리면 툴팁 (스킬 = 스킬 툴팁 위젯, 유물 = 이름 / 등급 / 설명)
 *  - SetUsable(false) 면 버튼이 꺼짐 (전투에서 비용 부족 등). 빈 칸은 항상 꺼짐
 *  - SlotIndex 는 쓰는 쪽이 정하는 번호 (전투 스킬 칸 번호 등). 눌리면 OnSlotClicked(this)
 *
 * 같은 내용을 다시 넣으면 그리지 않음 (전투 HUD 가 매 틱 부르므로)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEmpty();
	void SetSkill(FName SkillRow);
	void SetRelic(FName RelicRow);

	// 선택 / 대상 고르는 중 강조
	void SetSelected(bool bSelected);

	// 누를 수 있는지 (빈 칸은 무시하고 항상 꺼짐)
	void SetUsable(bool bUsable);

	// 이름 줄 보이기 (유물 줄처럼 아이콘만 보여줄 때 false)
	void SetShowName(bool bShow);

	// 칸 크기 바꾸기 (기본 모양의 SlotSizeBox 또는 WBP 의 SlotSizeBox)
	void SetSlotSize(FVector2D Size);

	void SetSlotIndex(int32 InIndex) { SlotIndex = InIndex; }
	int32 GetSlotIndex() const { return SlotIndex; }

	EItemSlotKind GetKind() const { return Kind; }
	FName GetItemRow() const { return ItemRow; }

	FOnItemSlotClicked OnSlotClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 칸 크기를 정하는 상자 (보통 루트)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> SlotSizeBox;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> SlotButton;

	// 등급 / 종류 색 테두리
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;

	// 아이콘이 없을 때 대신 보이는 이름 앞 두 글자
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> IconText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;

	// 스킬 칸 툴팁. 비워 두면 C++ 기본 스킬 툴팁
	UPROPERTY(EditAnywhere, Category = "Item Slot")
	TSubclassOf<USkillTooltipWidget> SkillTooltipClass;

	UPROPERTY(EditAnywhere, Category = "Item Slot")
	FVector2D DefaultSlotSize = FVector2D(84.f, 104.f);

	UPROPERTY(EditAnywhere, Category = "Item Slot")
	FLinearColor EmptyColor = FLinearColor(0.15f, 0.14f, 0.13f, 0.6f);

	UPROPERTY(EditAnywhere, Category = "Item Slot")
	FLinearColor SelectedColor = FLinearColor(1.f, 0.85f, 0.35f);

private:
	EItemSlotKind Kind = EItemSlotKind::Empty;
	FName ItemRow;
	FLinearColor ItemColor = FLinearColor::Gray;
	int32 SlotIndex = INDEX_NONE;

	bool bShownOnce = false;
	bool bSelectedState = false;
	bool bUsableState = true;

	UPROPERTY()
	TObjectPtr<USkillTooltipWidget> SkillTooltip;

	void BuildDefaultLayout();
	void ShowIcon(class UTexture2D* Texture, const FText& Name);
	void ApplyFrameColor();
	void ApplyButtonEnabled();

	UFUNCTION()
	void HandleClicked();
};
