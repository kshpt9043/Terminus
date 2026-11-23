#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "RelicTooltipWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
struct FRelicRow;

/**
 * 유물 정보 툴팁 (마우스를 올리면 뜨는 작은 창). 유물 칸(UItemSlotWidget)이 SetToolTip 으로 붙여 씀
 * 상점처럼 아이콘만 보여 주는 곳에서 이름 / 등급 / 설명을 여기서 보여 줌
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_RelicTooltip) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 그 WBP 는 칸(ItemSlotWidget / 칸 WBP)의 RelicTooltipClass 에 지정
 */
UCLASS()
class TERMINUS_API URelicTooltipWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	void SetRelic(const FRelicRow* InRelic);

protected:
	virtual void NativeOnInitialized() override;

	// 유물 이름
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;

	// "표층" / "심층" ...
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TierText;

	// 설명 (DT 의 RelicDesc)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;

	// 위쪽 3px 등급 색 띠 (선택). 기본 모양은 바깥 판의 바탕색이 띠
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> TierBand;

	// 유물 그림 (선택)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> IconImage;

	// 기본 모양의 폭 (설명이 길면 줄바꿈)
	UPROPERTY(EditAnywhere, Category = "Relic Tooltip")
	float DefaultWidth = 320.f;

private:
	void BuildDefaultLayout();
};
