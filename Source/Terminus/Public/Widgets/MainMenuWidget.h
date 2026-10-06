#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class USessionSubsystem;
class URoomListWidget;
class UStorageWidget;
class URunSaveListWidget;
class UBaseWidget;

/**
 * 첫 화면. 버튼 다섯 개와 안내 팝업.
 * 세션 처리는 전부 SessionSubsystem 에 맡기고 여기서는 의도만 전달한다.
 */
UCLASS()
class TERMINUS_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 메뉴. 이름은 디자인 가이드 4절 그대로
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Dungeon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Tavern;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Btn_Quit;

	// 거점 (직업의 탑: 세력 -> 직업 -> 연무장 / 훈련소). 기획 UI 레퍼런스 > 메인 화면
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Base;

	// 예전 이름. Btn_Base 가 없으면 Btn_Tower 가 거점 버튼. Btn_Training 은 거점 안으로 들어가서 숨김
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Tower;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Training;

	// 거점 창. WBP 안에 같은 이름으로 넣어 두면 그걸 쓰고, 없으면 BaseWidgetClass 로 만들어 화면에 띄움
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBaseWidget> BaseWindow;

	// 거점 창 클래스 (WBP_Base). 비워 두면 C++ 기본 모양
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UBaseWidget> BaseWidgetClass;

	// 창고 (보관 유물 / 보유 스킬). 기획 UI 레퍼런스 > 건물 > 창고
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Storage;

	// 창고 창. WBP 안에 같은 이름으로 넣어 두면 그걸 쓰고, 없으면 StorageWidgetClass 로 만들어 화면에 띄움
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UStorageWidget> StorageWindow;

	// 창고 창 클래스 (WBP_Storage). 비워 두면 C++ 기본 모양
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UStorageWidget> StorageWidgetClass;

	// 이어하기 (자동 저장된 런 목록). 기획: 메인 메뉴에서 세이브 불러오기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Continue;

	// 이어하기 창. WBP 안에 같은 이름으로 넣어 두면 그걸 쓰고, 없으면 SaveListWidgetClass 로 만들어 화면에 띄움
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<URunSaveListWidget> SaveListWindow;

	// 이어하기 창 클래스 (WBP_SaveList). 비워 두면 C++ 기본 모양
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<URunSaveListWidget> SaveListWidgetClass;

	// 보유 골드 (UTerminusProfileSubsystem). 바뀌면 바로 갱신
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GoldText;

	// 골드 표시 형식. {Gold} 자리에 천 단위 쉼표가 붙은 숫자
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	FText GoldFormat = INVTEXT("{Gold} G");

	// --- 안내 팝업. 끊김과 주점 열기 실패에 같이 쓴다
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget>    DisconnectPopup;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PopupBody;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    Btn_PopupOK;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PopupTitle;

	// 주점 리스트 찾기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<URoomListWidget> RoomBrowser;

	// 레벨 이름이 바뀌어도 코드 대신 BP 디폴트만 고치게
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Flow")
	FString TavernMapPath = TEXT("/Game/Maps/Lv_Tavern");

	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Flow")
	int32 MaxPartySize = 4;

private:
	UFUNCTION() void HandleDungeonClicked();
	UFUNCTION() void HandleTavernClicked();
	UFUNCTION() void HandleQuitClicked();
	UFUNCTION() void HandlePopupOKClicked();

	// 글자 메뉴는 버튼 브러시가 없어서 호버를 글자 색으로 보여준다
	UFUNCTION() void HandleQuitHovered();
	UFUNCTION() void HandleQuitUnhovered();

	UFUNCTION() void HandleGoldChanged(int32 NewGold, int32 Delta);
	UFUNCTION() void HandleStorageClicked();
	UFUNCTION() void HandleContinueClicked();
	UFUNCTION() void HandleBaseClicked();

	// 싱글 새 게임: 주점(캐릭터 선택)으로. 진행 중인 싱글 세이브가 있으면 이 대신 그걸 이어서 함
	void StartNewSolo();

	// 이어하기로 읽어 둔 세이브를 버림 (새 판을 시작하거나 메뉴로 돌아왔을 때)
	void ClearPendingContinue();

	// 이어하기 버튼은 세이브가 있을 때만 보임
	void RefreshContinueButton();
	FDelegateHandle RunSavesChangedHandle;
	void RefreshGold();

	// SessionSubsystem 의 비동기 결과
	UFUNCTION() void HandleHostComplete(bool bWasSuccessful);
	UFUNCTION() void HandleJoinComplete(bool bWasSuccessful);

	void ShowPopup(const FText& Title, const FText& Body);
	void SetMenuEnabled(bool bEnabled);
	static void SetButtonTextColor(UButton* Button, const FLinearColor& Color);
	USessionSubsystem* GetSessions() const;
};