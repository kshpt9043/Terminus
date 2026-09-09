// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CharacterTypes.h"
#include "ClassButton.generated.h"

class UButton;
class UTextBlock;

// C++ 단에서 버튼 눌렸을 때 캐릭터 열거형 받아오는 델리게이트
DECLARE_DELEGATE_OneParam(FOnClassButtonClicked, ECharacterClass);
/**
 * 
 */
UCLASS()
class TERMINUS_API UClassButton : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ClickArea;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> LabelText;
	
	// 나중에 선택했을 때 켜지는 표시 만드려고
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SelectedMark;
	
	UFUNCTION()
	void HandleClicked();
	
	ECharacterClass MyClass = ECharacterClass::Monk;
	
public:
	FOnClassButtonClicked OnClicked;
	
	void Setup(ECharacterClass InClass, const FText& Label);
	void SetSelected(bool bSelected);
	
	ECharacterClass GetCharacterClass() const { return MyClass; }
};
