#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PartyProfileCardWidget.generated.h"

class ATerminusPlayerState;
class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;

/**
 * 지도 상단바의 참가자 프로필 카드 한 장 (초상화, 이름, 직업, 체력)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 하나도 없으면(C++ 클래스 그대로) C++ 이 기본 모양을 만든다
 */
UCLASS()
class TERMINUS_API UPartyProfileCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayer(ATerminusPlayerState* InPlayer);

	ATerminusPlayerState* GetPlayer() const { return Player.Get(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 카드 바탕. 내 카드면 테두리 색을 바꿔 강조
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> CardBorder;

	// 직업 일러스트 (DT_CharacterClass 의 Illustration). 없으면 숨김
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Portrait;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ClassText;

	// "현재/최대" 체력
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;

	// 카드 바탕색 (내 카드 / 다른 사람)
	UPROPERTY(EditAnywhere, Category = "Profile Card")
	FLinearColor LocalCardColor = FLinearColor(0.35f, 0.27f, 0.12f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "Profile Card")
	FLinearColor OtherCardColor = FLinearColor(0.08f, 0.08f, 0.1f, 0.85f);

private:
	TWeakObjectPtr<ATerminusPlayerState> Player;

	// 지금 카드에 그려진 이름 / 직업. 복제가 늦게 와서 바뀌면 다시 그림
	FString ShownName;
	uint8 ShownClass = 0xFF;

	void BuildDefaultLayout();

	// 이름 / 직업 / 초상화. SetPlayer 때, 그리고 이름이나 직업이 바뀌었을 때만
	void RefreshStatic();

	// 체력 (매 틱)
	void RefreshHealth();
};
