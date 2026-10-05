#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CharacterTypes.h"
#include "StartRelicPickWidget.generated.h"

class UButton;
class UTextBlock;
class UUniformGridPanel;
class UItemSlotWidget;

/**
 * 런 시작 유물 고르기. 시작 스킬 고르기 다음, 지도 보기 전에 화면 전체를 덮고 뜸
 *
 * 창고(보관 유물)를 격자로 보여주고, 누를 때마다 고름 / 해제. 최대 MaxSelect 개 (기획: 1개, 안 골라도 됨)
 * 1개만 고를 때는 다른 칸을 누르면 그쪽으로 바뀜
 * 시작을 누르면 서버에 장착 요청 -> 화면이 닫힘
 * 창고 유물은 영구 소유라 그대로 남음. 하드 모드(주점에서 방장이 켬)일 때만 들고 간 유물이 창고에서 사라짐
 *
 *  - 다른 직업 전용 유물은 흐리게, 못 고름
 *  - 같은 유물을 여러 개 보관 중이어도 하나만 (런에서 같은 유물은 하나만 가질 수 있음)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항). 루트는 화면 전체
 */
UCLASS()
class TERMINUS_API UStartRelicPickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 창고 유물 목록, 내 직업 (직업 전용 유물 판정), 하드 모드 (들고 간 유물을 창고에서 뺄지)
	void SetCandidates(const TArray<FName>& StoredRelics, ECharacterClass InClass, bool bInHardMode);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "3 / 15"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountText;

	// 하드 모드 안내 ("들고 간 유물은 창고에서 사라집니다")
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> ItemGrid;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> StartButton;

	// 칸 위젯 (공용 칸). 비워 두면 C++ 기본 칸
	UPROPERTY(EditAnywhere, Category = "Start Relic")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditAnywhere, Category = "Start Relic")
	int32 Columns = 6;

	// 최소 칸 수 (빈 칸 포함)
	UPROPERTY(EditAnywhere, Category = "Start Relic")
	int32 MinSlots = 18;

	// 고를 수 있는 최대 개수 (서버 한도 ATerminusPlayerState::MaxStartRelics 를 넘을 수 없음)
	UPROPERTY(EditAnywhere, Category = "Start Relic")
	int32 MaxSelect = 1;

private:
	ECharacterClass PlayerClass = ECharacterClass::Fighter;
	bool bHardMode = false;

	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> Slots;

	// 칸 번호 -> 고름
	TSet<int32> SelectedIndices;

	bool bStarted = false;

	void BuildDefaultLayout();
	void RefreshStates();
	bool IsRowSelected(FName Row) const;
	bool CanEquip(FName Row) const;

	void HandleSlotClicked(UItemSlotWidget* ClickedSlot);

	UFUNCTION() void HandleStartClicked();
};
