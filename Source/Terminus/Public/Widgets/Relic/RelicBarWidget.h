#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RelicBarWidget.generated.h"

class UPanelWidget;

/**
 * 내 보유 유물 줄. 아이콘 + 마우스를 올리면 이름 / 등급 / 설명 툴팁
 * 지도 화면과 전투 HUD 의 같은 자리(오른쪽 아래)에 둠 (기획 전투 UI 시안: 우하단 보유 유물)
 *
 * 보유 유물(RunState.Relics)이 바뀌면 알아서 다시 그림. 아이콘이 없는 유물은 이름 앞 두 글자로
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 RelicBox(가로 상자 추천)를 둘 것
 */
UCLASS()
class TERMINUS_API URelicBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 유물 칸이 들어갈 상자. 있던 자식은 지우고 채움
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RelicBox;

	// 칸 크기 / 간격
	UPROPERTY(EditAnywhere, Category = "Relic Bar")
	float IconSize = 40.f;

	UPROPERTY(EditAnywhere, Category = "Relic Bar")
	float Spacing = 4.f;

private:
	// 지금 그려진 유물 목록 (바뀌면 다시 그림)
	TArray<FName> ShownRelics;
	bool bShownOnce = false;

	void BuildDefaultLayout();
	void Rebuild(const TArray<FName>& Relics);
};
