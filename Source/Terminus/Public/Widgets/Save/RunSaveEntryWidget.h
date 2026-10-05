#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/TerminusSaveSubsystem.h"
#include "RunSaveEntryWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnRunSaveEntryAction, const FString& /*SlotName*/);

/**
 * 세이브 목록의 한 줄. [제목 / 플레이어 / 저장 시각] [불러오기] [삭제]
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 */
UCLASS()
class TERMINUS_API URunSaveEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FRunSaveSummary& InSummary);

	FOnRunSaveEntryAction OnLoadClicked;
	FOnRunSaveEntryAction OnDeleteClicked;

protected:
	virtual void NativeOnInitialized() override;

	// 방(던전) 이름 "배고픈 슬라임 원정대"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// "멀티 3인 · 1층 표층 · 방 4개 지남"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> InfoText;

	// "무도가 Joe, 성기사 Kim"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PlayersText;

	// "2026-10-05 14:32"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DateText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> LoadButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> DeleteButton;

private:
	FString SlotName;

	void BuildDefaultLayout();

	UFUNCTION() void HandleLoad();
	UFUNCTION() void HandleDelete();
};
