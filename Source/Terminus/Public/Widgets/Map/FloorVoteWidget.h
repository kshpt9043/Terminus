#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/MapManager.h"
#include "FloorVoteWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UFloorVoteWidget;

// 카드 버튼마다 어느 선택지인지 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음)
UCLASS()
class TERMINUS_API UFloorVoteClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UFloorVoteWidget> Owner;
	EFloorChoice Choice = EFloorChoice::None;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 테마 끝 층 보스 뒤 선택 화면 (다음 층 / 탈출 / 배신). 멀티는 투표
 *  - 카드마다 이름 / 설명 / "n명 선택". 누가 골랐는지는 투표가 끝나고 공개 타이머(5초) 뒤에 카드 아래에 나옴
 *  - 배신은 선착순 1명 -> 누가 고르면 투표가 바로 끝남
 *  - 한 번 고르면 못 바꿈
 * 상태는 MapManager 의 VoteState (복제). PC 가 바뀔 때마다 Refresh 를 부름
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 FloorVoteClass 에 지정 (아래 이름 전부 선택 사항)
 * 카드는 CardBox 안에 C++ 이 채움
 */
UCLASS()
class TERMINUS_API UFloorVoteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Refresh(const FFloorVoteState& State);

	void HandleCardClicked(EFloorChoice Choice);

	// 서버가 내 표를 거절했을 때 (배신을 누가 먼저 골랐음 등)
	void ClearMyChoice();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// "2층 보스를 쓰러뜨렸습니다"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "다음 행선지를 고르세요 · 12초" / "공개까지 3초" / "결과: 다음 층"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

	// 카드가 들어갈 곳 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	UPROPERTY(EditAnywhere, Category = "Floor Vote")
	FVector2D CardSize = FVector2D(240.f, 300.f);

private:
	FFloorVoteState Shown;
	EFloorChoice MyChoice = EFloorChoice::None;

	UPROPERTY()
	TArray<TObjectPtr<UFloorVoteClickRelay>> Relays;

	void BuildDefaultLayout();
	void RebuildCards();
	void UpdateStatus();

	static FString ChoiceName(EFloorChoice Choice);
	FString ChoiceDescription(EFloorChoice Choice) const;
};
