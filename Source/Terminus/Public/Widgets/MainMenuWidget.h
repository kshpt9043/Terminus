#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class USessionSubsystem;
class URoomListWidget;
class UStorageWidget;

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

	// 기능이 아직 없는 건물. 없어도 되게 Optional
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Tower;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Training;

	// 창고 (보관 유물 / 보유 스킬). 기획 UI 레퍼런스 > 건물 > 창고
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Storage;

	// 창고 창. WBP 안에 같은 이름으로 넣어 두면 그걸 쓰고, 없으면 StorageWidgetClass 로 만들어 화면에 띄움
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UStorageWidget> StorageWindow;

	// 창고 창 클래스 (WBP_Storage). 비워 두면 C++ 기본 모양
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UStorageWidget> StorageWidgetClass;

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
	void RefreshGold();

	// SessionSubsystem 의 비동기 결과
	UFUNCTION() void HandleHostComplete(bool bWasSuccessful);
	UFUNCTION() void HandleJoinComplete(bool bWasSuccessful);

	void ShowPopup(const FText& Title, const FText& Body);
	void SetMenuEnabled(bool bEnabled);
	static void SetButtonTextColor(UButton* Button, const FLinearColor& Color);
	USessionSubsystem* GetSessions() const;
};