#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfirmPopupWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPopupAnswered);

/**
 * 범용 팝업. 화면 전체를 어둡게 덮고 가운데에 제목 / 내용 / 버튼
 *  - 확인 + 취소 : 나가기 확인 같은 것
 *  - 확인 하나   : 알림 (CancelLabel 을 비우면 취소 버튼이 숨음)
 *
 * 띄우는 건 PC 의 ShowPopup 으로 (PopupClass 에 지정한 WBP 를 씀). 버튼을 누르면 스스로 닫힘
 * 키보드: Enter = 확인, Esc = 취소 (취소 버튼이 없으면 Esc 도 확인)
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UConfirmPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 내용 채우기. CancelLabel 이 비어 있으면 버튼 하나짜리 알림
	UFUNCTION(BlueprintCallable, Category = "Popup")
	void Setup(const FText& InTitle, const FText& InMessage, const FText& InConfirmLabel, const FText& InCancelLabel);

	// C++ 에서 받을 때
	FSimpleDelegate OnConfirmedNative;
	FSimpleDelegate OnCancelledNative;

	// BP 에서 받을 때
	UPROPERTY(BlueprintAssignable, Category = "Popup")
	FOnPopupAnswered OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Popup")
	FOnPopupAnswered OnCancelled;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ConfirmLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CancelButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CancelLabel;

private:
	bool bClosed = false;
	bool bHasCancel = true;

	void BuildDefaultLayout();
	void Close(bool bConfirmed);

	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();
};
