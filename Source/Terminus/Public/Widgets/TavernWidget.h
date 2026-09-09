// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CharacterTypes.h"
#include "TavernWidget.generated.h"

class UPlayerSlot;
class UPanelWidget;
class UImage;
class UTextBlock;
class UDataTable;
class UClassButton;

/**
 * 주점(멀티 로비) 화면. 슬롯 4개와 캐릭터 정보 패널을 들고 있다.
 */
UCLASS()
class TERMINUS_API UTavernWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
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

	// 선택한 캐릭터 이름을 따로 띄우고 싶을 때만
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ClassTitleText;

	// 캐릭터 정보 담은 DT
	UPROPERTY(EditDefaultsOnly, Category = "Terminus|Data")
	TObjectPtr<UDataTable> CharacterClassTable;

	// 지금 고른 클래스. 기본값은 기획서대로 무도가
	ECharacterClass Selected = ECharacterClass::Monk;

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

private:
	void CreateSlots();
	void RefreshSlots();

	FTimerHandle RefreshTimer;
	int32 LastPlayerCount = -1;
	static constexpr int32 MaxSlots = 4;
};
