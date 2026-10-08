#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/TerminusProfileSubsystem.h"
#include "SettlementWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UItemSlotWidget;
class USettlementWidget;

// 유물마다 어디에 넘길지
UENUM()
enum class ESettlementChoice : uint8
{
	Mage,       // 마탑에 판매
	Religion,   // 테르미누스(종교)에 판매
	Keep        // 창고에 보관 (골드 없음)
};

// 유물 줄의 버튼마다 몇 번째 유물을 어떻게 할지 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음)
UCLASS()
class TERMINUS_API USettlementClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<USettlementWidget> Owner;
	int32 Index = INDEX_NONE;
	ESettlementChoice Choice = ESettlementChoice::Keep;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 정산 화면 (메인 화면에서 뜸). 기획 '정산' 순서 그대로: 획득한 유물 -> 완료된 퀘스트 -> 최종 골드
 *  - 유물마다 마탑에 판매 / 테르미누스에 판매 / 보관 중 하나. 세력마다 값이 조금 다름 (기본가 ±5%)
 *  - 창고(도감)에 이미 있는 유물은 보관해도 아무 일도 없음 (사용자 결정 10-06) -> 기본 선택은 비싼 쪽 판매
 *  - 강화 칸 스킬 중 아직 없는 것 하나를 보유 스킬로 (기획 플레이 로직)
 *  - 퀘스트는 아직 없어서 '없음'만 보여 줌
 *  - '정산 완료' -> 프로필에 반영 -> 최종 골드 -> 닫기
 * 내용은 프로필의 정산 대기(UTerminusProfileSubsystem::GetPendingSettlement)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 WBP_MainMenu 의 SettlementWidgetClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API USettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void HandleRelicChoice(int32 Index, ESettlementChoice Choice);

protected:
	virtual void NativeOnInitialized() override;

	// "정산"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "던전을 탈출했습니다. (배고픈 슬라임 원정대 · 3층)"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;

	// 유물 줄이 들어갈 곳 (스크롤 박스 / 세로 상자)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicList;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> QuestText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SkillText;

	// "판매 골드 +1,234"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TotalText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> SellAllButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ConfirmButton;

	// 최종 골드 (정산 완료 뒤)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SummaryPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SummaryText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	// 정산 목록 (유물 / 퀘스트 / 스킬 / 합계 / 버튼). 정산 완료 뒤 숨김
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MainPanel;

	UPROPERTY(EditAnywhere, Category = "Settlement")
	TSubclassOf<UItemSlotWidget> SlotClass;

private:
	FPendingSettlement Settlement;
	TArray<ESettlementChoice> Choices;

	UPROPERTY()
	TArray<TObjectPtr<USettlementClickRelay>> Relays;

	void BuildDefaultLayout();
	void RebuildRelics();
	void RefreshTotal();
	int32 CalcGold() const;
	bool IsAlreadyStored(FName Row) const;

	UFUNCTION() void HandleSellAll();
	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleClose();
};
