#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RewardTypes.h"
#include "ShopWidget.generated.h"

class UButton;
class UItemCardWidget;
class UPanelWidget;
class UTextBlock;
class UItemSlotWidget;
class UItemPickPanelWidget;
class ATerminusPlayerState;
struct FRunState;
class UShopWidget;

UENUM()
enum class EShopAction : uint8
{
	BuyRelic,
	BuySkill,
	Upgrade,
	Sell
};

/**
 * 상점 화면 (사용자 결정 2026-10-09). 내 진열은 서버가 FShopState 로 보내 줌 (사람마다 따로)
 *  - 유물 구매 / 픽업 스킬 구매 (강화 칸이 꽉 차면 바꿀 칸을 고름)
 *  - 장착한 픽업 스킬 강화 (+1 ~ +3)
 *  - 보유 유물 판매 (직업 기본 유물 / 판매가 없는 유물은 못 팜)
 *  - 나가기: 같은 방 전원이 나가면 지도로
 * 던전 재화 / 보유 유물 / 장착 스킬은 내 PlayerState 의 OnRunStateChanged 알림을 받아 바뀌면 다시 그림
 * 띄우는 건 PC 의 Client_ShowShop
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 ShopClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetShopState(const FShopState& InState);

	void HandleAction(EShopAction Action, int32 Index, FName Row);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CurrencyText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// 각 칸이 들어갈 곳 (가로 상자 / 랩 박스)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicBuyBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> SkillBuyBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> UpgradeBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> SellBox;

	// 픽업 스킬을 살 때 강화 칸이 꽉 찼으면 바꿀 칸 고르기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UItemPickPanelWidget> ReplacePanel;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> LeaveButton;

	UPROPERTY(EditAnywhere, Category = "Shop")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditAnywhere, Category = "Shop")
	FVector2D SlotSize = FVector2D(72.f, 72.f);

	// 진열 칸을 아이콘만 보이게 (이름 / 테두리 없이, 정보는 마우스를 올리면 툴팁) (사용자 결정 2026-10-12)
	UPROPERTY(EditAnywhere, Category = "Shop")
	bool bIconOnlySlots = true;

	// 진열 칸 하나 (칸 + 설명 + 구매 / 강화 / 판매 버튼). 비우면 C++ 기본 모양. 칸 모양은 SlotClass
	UPROPERTY(EditAnywhere, Category = "Shop")
	TSubclassOf<UItemCardWidget> CardClass;

	// 기본 레이아웃일 때 바꿀 칸 고르기 패널 (비우면 C++ 기본)
	UPROPERTY(EditAnywhere, Category = "Shop")
	TSubclassOf<UItemPickPanelWidget> PickPanelClass;

private:
	FShopState State;
	int32 PendingSkillIndex = INDEX_NONE;   // 바꿀 칸을 고르는 중인 스킬 진열 번호
	FString ShownKey;
	TWeakObjectPtr<ATerminusPlayerState> WatchedPS;                       // 마지막으로 그린 내 상태 (재화 / 유물 / 스킬 / 강화 단계)


	void BuildDefaultLayout();
	void Rebuild();
	FString MakeStateKey() const;
	void AddCard(UPanelWidget* Box, bool bRelic, FName Row, const FString& Caption, const FString& ButtonLabel, bool bEnabled, EShopAction Action, int32 Index);
	void ShowReplace();

	void HandleReplacePicked(UItemSlotWidget* ClickedSlot);
	void HandleReplaceCancel();

	UFUNCTION() void HandleRunStateChanged(const FRunState& NewRunState);
	UFUNCTION() void HandleLeave();
};
