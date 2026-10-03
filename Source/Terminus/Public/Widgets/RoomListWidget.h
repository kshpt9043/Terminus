#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/SessionSubsystem.h"
#include "RoomListWidget.generated.h"

class UButton;
class UTextBlock;
class UPanelWidget;
class UEditableTextBox;
class UCheckBox;
class URoomEntryWidget;

/**
 * 주점 찾기/열기 화면. 메인 메뉴 안에 숨겨 두었다가 주점 입장을 누르면 연다.
 * 세션 처리는 SessionSubsystem 에 맡기고 여기서는 입력과 목록만 다룬다.
 */
UCLASS()
class TERMINUS_API URoomListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 메인 메뉴가 부른다. 정원과 주점 맵은 메뉴가 들고 있는 값을 받음
	void Open(int32 InMaxPlayers, const FString& InMapPath);
	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 목록
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ListBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>      Btn_Refresh;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>      Btn_OpenCreate;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>      Btn_Close;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<URoomEntryWidget> EntryClass;

	// 입력칸 세 개(방 이름, 비밀번호, 참가 비밀번호) 글자색. 평소와 입력 중 모두 이 색
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	FLinearColor InputTextColor = FLinearColor::White;

	// --- 주점 열기
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget>          CreatePanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> RoomNameInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> PasswordInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>          Btn_CreateConfirm;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>          Btn_CreateCancel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox>  PrivateCheck;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CreateErrorText;

	// --- 비밀번호 입력
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget>          PasswordPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> JoinPasswordInput;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>          Btn_JoinConfirm;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>          Btn_JoinCancel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> JoinErrorText;

private:
	UFUNCTION() void HandleRefreshClicked();
	UFUNCTION() void HandleOpenCreateClicked();
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleCreateConfirmClicked();
	UFUNCTION() void HandleCreateCancelClicked();
	UFUNCTION() void HandleJoinConfirmClicked();
	UFUNCTION() void HandleJoinCancelClicked();

	// SessionSubsystem 의 비동기 결과. 시그니처는 델리게이트 선언과 같아야 함
	UFUNCTION() void HandleFindComplete(bool bWasSuccessful, const TArray<FTerminusSessionInfo>& Sessions);
	UFUNCTION() void HandleJoinComplete(bool bWasSuccessful);
	UFUNCTION() void HandleHostComplete(bool bWasSuccessful);

	void HandleEntryClicked(const FTerminusSessionInfo& Info);
	void Join(int32 Index, const FString& Password);
	void SetStatus(const FString& InText);
	void SetBusy(bool bBusy);
	void ShowPanel(UWidget* Panel, bool bShow);
	void PaintInputText(UEditableTextBox* Box) const;
	USessionSubsystem* GetSessions() const;

	int32   MaxPlayers = 4;
	FString MapPath;

	// 비밀번호를 기다리는 방. 참가는 검색 결과 인덱스로 하므로 입력 중엔 새로고침을 막는다
	int32 PendingJoinIndex = INDEX_NONE;
};