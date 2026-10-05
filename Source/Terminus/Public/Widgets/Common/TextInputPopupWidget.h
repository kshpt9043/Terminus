#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "TextInputPopupWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnTextInputConfirmed, const FString& /*Text*/);

/**
 * 글자 입력 팝업 (범용). 제목 / 설명 / 입력칸 / [랜덤] [확인] [취소]
 *  - Enter = 확인, ESC = 취소 (UEscapeStackSubsystem)
 *  - 랜덤 버튼은 SetRandomProvider 를 줬을 때만 보임 (누르면 입력칸을 새 값으로)
 *  - 확인 때 앞뒤 공백을 지운 값. 비어 있으면 랜덤 값(있으면) 또는 처음 값
 *
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 */
UCLASS()
class TERMINUS_API UTextInputPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& InTitle, const FText& InMessage, const FString& InDefaultText, int32 InMaxLength = 24);

	// 랜덤 버튼이 부를 함수. 안 주면 버튼이 숨음
	void SetRandomProvider(TFunction<FString()> InProvider);

	FOnTextInputConfirmed OnConfirmedText;
	FSimpleDelegate OnCancelledNative;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> InputBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RandomButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;

private:
	FString DefaultText;
	int32 MaxLength = 24;
	bool bClosed = false;
	TFunction<FString()> RandomProvider;

	void BuildDefaultLayout();
	void Close(bool bConfirmed);

	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandleRandomClicked();
	UFUNCTION() void HandleTextChanged(const FText& Text);
	UFUNCTION() void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
