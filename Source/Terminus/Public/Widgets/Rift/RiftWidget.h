#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RiftWidget.generated.h"

class UButton;
class UTextBlock;
class UConfirmPopupWidget;

/**
 * 이공간 (멀티에서 누가 진행 중에 나갔을 때). 화면 전체를 덮고 나간 사람이 돌아오길 기다림
 *  - 그동안 세션은 주점 목록에 '재합류 대기' 로 뜸. 나간 사람이 그걸 찾아 들어오면 전원 지도로
 *  - 싸우던 방은 무효 (들어가기 전으로 롤백)
 *  - 기다리기 싫으면 나가기 (방장이 나가면 던전이 끝남)
 *
 * 띄우고 닫는 건 PC 의 Client_EnterRift / Client_LeaveRift. ESC 로는 안 닫힘
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_Rift) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 */
UCLASS()
class TERMINUS_API URiftWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 돌아오길 기다리는 사람 명단
	void SetWaiting(const TArray<FString>& WaitingFor);

	// 멀티 세이브를 불러와 모이는 중이면 설명 문구가 다름 (누가 나간 게 아님)
	void SetFromSave(bool bFromSave);

protected:
	virtual void NativeOnInitialized() override;

	// "이공간"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 설명 ("동료와의 연결이 끊겨 이공간에 빠졌습니다...")
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	// "기다리는 중: Kim, Lee"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WaitingText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> LeaveButton;

	// 나가기 확인 팝업. 비워 두면 C++ 기본
	UPROPERTY(EditAnywhere, Category = "Rift")
	TSubclassOf<UConfirmPopupWidget> PopupClass;

private:
	void BuildDefaultLayout();
	void HandleLeaveConfirmed();

	UFUNCTION() void HandleLeaveClicked();
};
