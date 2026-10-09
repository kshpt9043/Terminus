#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommsMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FOnCommsMenuPicked, int32 /*OptionIndex*/);

/**
 * 핑 / 퀵챗 고르기에 쓰는 작은 목록 메뉴. 마우스 자리에 뜸
 *
 * - 항목 클릭 또는 숫자 키(1 ~ 9)로 고름. 숫자 키는 UPartyCommsWidget 의 입력 전처리기가 PickByNumber 로 넘김
 * - ESC / 메뉴 밖 클릭이면 닫힘
 *
 * 화면 전체를 덮는 위젯이지만 바탕은 클릭을 통과시키고(SelfHitTestInvisible) 목록만 클릭을 받음
 * C++ 만으로 모양을 만듦 -> WBP 필요 없음
 */
UCLASS()
class TERMINUS_API UCommsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 목록을 채우고 Position(화면 위젯 좌표) 근처에 띄움. 이미 떠 있으면 내용만 바꿔 옮김
	void Open(const FText& Title, const TArray<FText>& Options, const FVector2D& Position, FOnCommsMenuPicked InOnPicked);

	void Close();

	bool IsOpen() const { return bOpen; }

	// 숫자 키 (1 부터). 범위 밖이면 false
	bool PickByNumber(int32 Number);

	// 이 화면 좌표(절대 좌표)가 목록 위인가 (메뉴 밖 클릭 판정)
	bool IsOverPanel(const FVector2D& ScreenSpacePosition) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, Category = "Comms Menu")
	int32 FontSize = 16;

	UPROPERTY(EditAnywhere, Category = "Comms Menu")
	float ButtonWidth = 170.f;

private:
	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UBorder> Panel;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UVerticalBox> List;

	// 항목 버튼 (인덱스 = 항목 번호)
	UPROPERTY() TArray<TObjectPtr<UButton>> Buttons;

	FOnCommsMenuPicked OnPicked;
	bool bOpen = false;

	void BuildLayout();
	void Pick(int32 Index);

	UFUNCTION() void HandleButtonClicked();
};
