// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CharacterTypes.h"
#include "Styling/SlateTypes.h"
#include "TavernWidget.generated.h"

class UPlayerSlot;
class UPanelWidget;
class UImage;
class UTextBlock;
class UButton;
class UClassButton;

/**
 * 주점(멀티 로비) 화면. 슬롯 4개와 캐릭터 정보 패널을 들고 있다.
 */
UCLASS()
class TERMINUS_API UTavernWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 슬롯
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> SlotPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UPlayerSlot> SlotClass;

	UPROPERTY()
	TArray<TObjectPtr<UPlayerSlot>> Slots;

	// 캐릭터 정보 패널
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage>     Illustration;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PassiveText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ClassButtonBox;
	
	// 싱글, 멀티 다르게 타이틀 띄우기
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// 선택한 캐릭터 이름을 따로 띄우고 싶을 때만
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ClassTitleText;

	// 지금 고른 클래스. 기본값은 기획서대로 무도가
	ECharacterClass Selected = ECharacterClass::Fighter;

	// 열거형으로 행을 찾는 유일한 창구. 나중에 CSV 로 옮겨도 여기만 고치면 된다
	const FCharacterClassRow* FindClassRow(ECharacterClass InClass) const;
	void ApplySelection(ECharacterClass InClass);
	
	// 캐릭터 선택 버튼 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|UI")
	TSubclassOf<UClassButton> ClassButtonClass;
	
	UPROPERTY()
	TArray<TObjectPtr<UClassButton>> ClassButtons;
	
	void CreateClassButtons();
	void HandleClassChosen(ECharacterClass InClass);
	
	// 준비 버튼. 아직 WBP 에 없어도 되게 Optional 로 둠
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton>    ReadyButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReadyButtonText;
	
	// 시작 버튼 호스트만 봐야함
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> StartButton;
	
	// 나가기 버튼
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> LeaveButton;
	
	// 초대 버튼
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> InviteButton;
	
	// 클라이언트 전용 출발 대응 버튼
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WaitText;
	
	// 세력 이름 (모험가 길드 등)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FactionText;

	// 기본 스킬 3개 / 공격 방어 특수
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SkillText;

	UFUNCTION()
	void HandleInviteClicked();
	
	UFUNCTION()
	void HandleStartClicked();
	
	bool AreAllPlayersReady() const;
	
	UFUNCTION()
	void HandleReadyClicked();
	
	UFUNCTION()
	void HandleLeaveClicked();

private:
	void CreateSlots();
	void RefreshSlots();

	FTimerHandle RefreshTimer;
	int32 LastPlayerCount = -1;
	static constexpr int32 MaxSlots = 4;
	
	bool IsSolo() const;
	void ApplySoloLayout();
	
	FButtonStyle ReadyStyle;
	FButtonStyle CancelStyle;
	
	// 마지막으로 끼운 상태. -1 은 아직 안 끼움 -> 첫 폴링에서 반드시 한 번 넣게
	int8 LastReadyShown = -1;
};
