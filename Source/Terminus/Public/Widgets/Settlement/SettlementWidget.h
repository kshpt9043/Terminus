#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "Game/TerminusProfileSubsystem.h"
#include "SettlementWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UItemSlotWidget;

/**
 * 정산 화면 (메인 화면에서 뜸). 순서: 유물 보관 / 판매 -> 완료된 퀘스트 -> 가져가는 스킬 -> 최종 골드
 *  - 노션(10-26) + 사용자 결정(10-26): 던전에서 얻은 유물(시작 유물 제외) 중 원하는 만큼 골라 창고에 보관,
 *    안 고른 유물은 자동으로 정산 판매가에 팔려 골드가 됨. 사망(전멸)도 퇴장이라 똑같이 고름
 *  - 창고(도감)에 이미 있는 유물을 보관하면 아무 일도 없음 (사용자 결정 10-06) -> 파는 쪽이 이득이라 안내
 *  - 장착 중인 픽업 스킬 중 아직 없는 것 하나를 보유 스킬로 (전부 있으면 건너뜀). 사망이면 스킬은 못 가져감
 *  - 퀘스트 골드 / 배신 분배 골드는 BonusGold
 *  - '정산 완료' -> 프로필에 반영 -> 최종 골드 -> 닫기
 * 내용은 프로필의 정산 대기(UTerminusProfileSubsystem::GetPendingSettlement)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 WBP_MainMenu 의 SettlementWidgetClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API USettlementWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	// "정산"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "던전을 탈출했습니다. (배고픈 슬라임 원정대 · 3층)"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;

	// "1. 보관할 유물" / 사망이면 "1. 판매된 유물" 과 그 설명
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RelicHeaderText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RelicHelpText;

	// 유물 칸이 들어갈 곳 (랩 박스 / 가로 상자)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicList;

	// "고른 유물: ..." / 안내
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RelicGuideText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> QuestText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SkillText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ConfirmButton;

	// 최종 골드 (정산 완료 뒤)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SummaryPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SummaryText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	// 정산 목록 (유물 / 퀘스트 / 스킬 / 버튼). 정산 완료 뒤 숨김
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> MainPanel;

	UPROPERTY(EditAnywhere, Category = "Settlement")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditAnywhere, Category = "Settlement")
	FVector2D RelicSlotSize = FVector2D(84.f, 84.f);

private:
	FPendingSettlement Settlement;
	TArray<FName> Kept;   // 보관할 유물 (나머지는 판매)

	void BuildDefaultLayout();
	void RebuildRelics();
	void RefreshGuide();
	bool IsAlreadyStored(FName Row) const;
	int32 SaleGold() const;   // 안 고른 유물 판매 골드

	void HandleRelicClicked(UItemSlotWidget* ClickedSlot);

	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleClose();
};
