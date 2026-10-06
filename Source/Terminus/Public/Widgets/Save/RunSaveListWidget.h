#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RunSaveListWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class URunSaveEntryWidget;
class UConfirmPopupWidget;

/**
 * 이어하기 (메인 메뉴). 자동 저장된 런 목록. 불러오기 / 삭제
 *  - 싱글 세이브 (하나뿐): 세션 없이 주점 -> 바로 던전으로
 *  - 멀티 세이브: 세이브 인원으로 세션을 열고 던전 이공간으로 바로. 세이브 당시 플레이어가 주점 목록의
 *    '재합류 대기' 방으로 다 들어오면 지도로 (각자 세이브 당시 직업 / 상태 그대로)
 *
 * ESC 또는 닫기 버튼으로 닫힘
 * WBP 로 꾸미려면 이 클래스를 부모로 WBP(WBP_SaveList) 를 만들고 아래 이름으로 위젯을 둘 것 (전부 선택 사항)
 * 루트는 화면 전체 (뒤를 어둡게 덮는 패널) 로 둘 것
 */
UCLASS()
class TERMINUS_API URunSaveListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(const FString& InTavernMapPath);
	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// 줄이 들어갈 곳 (스크롤 상자 / 세로 상자)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> EntryBox;

	// "저장된 진행이 없습니다"
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;

	// 불러오기 실패 사유 등
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	// 줄 위젯. 비워 두면 C++ 기본
	UPROPERTY(EditAnywhere, Category = "Save")
	TSubclassOf<URunSaveEntryWidget> EntryClass;

	// 멀티 이어하기는 던전으로 바로 감 (이공간에서 세이브 플레이어를 기다림)
	UPROPERTY(EditAnywhere, Category = "Save")
	FString DungeonMapPath = TEXT("/Game/Maps/Lv_Dungeon");

	// 삭제 확인 팝업. 비워 두면 C++ 기본
	UPROPERTY(EditAnywhere, Category = "Save")
	TSubclassOf<UConfirmPopupWidget> PopupClass;

private:
	FString TavernMapPath;
	FString PendingDeleteSlot;
	bool bLoading = false;

	void BuildDefaultLayout();
	void Refresh();
	void SetStatus(const FText& Text);

	void HandleLoad(const FString& SlotName);
	void HandleDelete(const FString& SlotName);
	void HandleDeleteConfirmed();

	UFUNCTION() void HandleClose();
};
