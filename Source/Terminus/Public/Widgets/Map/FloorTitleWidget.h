#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FloorTitleWidget.generated.h"

class UTextBlock;

/**
 * 층 도착 화면. 다음 층으로 넘어갈 때 잠깐 "2층 / 표층 · 슬라임 왕국 / 체력이 모두 회복되었습니다" 를 보여주고 스스로 사라짐
 * 띄우는 건 PC 의 Client_ShowFloorTitle
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들어 BP_DungeonPC 의 FloorTitleClass 에 지정 (아래 이름 전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UFloorTitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& InTitle, const FText& InSubtitle, const FText& InNote);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NoteText;

	// 보여주는 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Floor Title")
	float DisplaySeconds = 2.5f;

private:
	void BuildDefaultLayout();
};
