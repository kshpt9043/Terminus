#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapTopBarWidget.generated.h"

class ATerminusPlayerState;
class UButton;
class UPanelWidget;
class UPartyProfileCardWidget;
class UTextBlock;

/**
 * 지도 화면 상단바: 지금 층 / 참가자 프로필 카드 / 내 재화 / 나가기
 * 지도 화면(UMapScreenWidget) 안에 들어감. 구역에 들어갈 때 숨기는 건 지도 화면이 함
 *
 * 나가기: 범용 팝업(PC 의 ShowPopup)으로 확인받고, 확인하면 세션을 정리하고 메인 화면(Lv_Lobby)으로
 * (기획 UI 레퍼런스: 우상단 게임 종료 -> 메인 UI)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 이 위젯 자체가 바 하나. 지도 화면에서 위쪽에 바 높이만큼 배치. 프로필 카드 모양은 ProfileCardClass 로
 */
UCLASS()
class TERMINUS_API UMapTopBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// "1층 · 표층 · 슬라임 왕국"
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> FloorText;

	// "다음 방 3 / 12"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RoomText;

	// 프로필 카드가 들어갈 칸 (가로 상자 추천). 있던 자식은 지우고 채움
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> PartyBox;

	// "재화 120"
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CurrencyText;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ExitButton;

	// 프로필 카드 클래스. 비워 두면 C++ 기본 카드
	UPROPERTY(EditAnywhere, Category = "Map Top Bar")
	TSubclassOf<UPartyProfileCardWidget> ProfileCardClass;

	// 카드 사이 간격 (PartyBox 가 가로 상자일 때)
	UPROPERTY(EditAnywhere, Category = "Map Top Bar")
	float CardSpacing = 10.f;

private:
	// 지금 카드를 만든 참가자들 (바뀌면 다시 만듦)
	TArray<TWeakObjectPtr<ATerminusPlayerState>> ShownPlayers;

	void BuildDefaultLayout();

	void RefreshParty();
	void RefreshTexts();

	UFUNCTION() void HandleExitClicked();

	// 팝업에서 "나가기" 확인
	void LeaveToMenu();
};
