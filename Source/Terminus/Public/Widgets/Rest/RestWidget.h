#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RestWidget.generated.h"

class APlayerState;
class UButton;
class UPanelWidget;
class UTextBlock;
class URestWidget;

// 카드 버튼마다 누구를 골랐는지 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음)
UCLASS()
class TERMINUS_API URestClickRelay : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<URestWidget> Owner;
	TWeakObjectPtr<APlayerState> Target;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 휴식터 화면. 같은 휴식터에 들어온 사람 중 회복시킬 대상을 하나 고름 (사용자 결정 2026-10-06)
 *  - 대상은 최대 체력의 HealRatio(20%) 만큼 회복
 *  - 나 대신 동료를 고르면 나는 회복하지 않음. 싱글은 나 혼자
 *  - 고르면 다른 사람을 기다림. 같은 휴식터의 전원이 고르면 방이 끝나고 지도로 (화면은 구역이 닫힐 때 PC 가 닫음)
 * 띄우는 건 PC 의 Client_ShowRest
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 RestClass 에 지정 (아래 이름 전부 선택 사항)
 * 카드는 CardBox 안에 C++ 이 채움
 */
UCLASS()
class TERMINUS_API URestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const TArray<APlayerState*>& InOccupants, float InHealRatio);

	void HandleCardClicked(APlayerState* Target);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// "휴식터"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 설명 / 기다리는 중 안내
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// 대상 카드가 들어갈 곳 (가로 상자 추천)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CardBox;

	UPROPERTY(EditAnywhere, Category = "Rest")
	FVector2D CardSize = FVector2D(220.f, 200.f);

private:
	TArray<TWeakObjectPtr<APlayerState>> Occupants;
	TWeakObjectPtr<APlayerState> Chosen;
	float HealRatio = 0.2f;
	float RefreshTimer = 0.f;

	// 마지막으로 그린 체력 (바뀌었을 때만 다시 그림 -> 누르는 도중에 버튼이 바뀌지 않게)
	TArray<int32> ShownHealth;
	TArray<int32> ReadHealth() const;

	UPROPERTY()
	TArray<TObjectPtr<URestClickRelay>> Relays;

	void BuildDefaultLayout();
	void RebuildCards();
	void UpdateMessage();
};
