// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/SessionSubsystem.h"
#include "RoomEntryWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnRoomEntryClicked, const FTerminusSessionInfo&);

/**
 * 
 */
UCLASS()
class TERMINUS_API URoomEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(const FTerminusSessionInfo& InInfo);
	
	FOnRoomEntryClicked OnClicked;
	
protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton>    JoinButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> RoomNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PlayersText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PingText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget>    LockMark;

private:
	UFUNCTION()
	void HandleClicked();

	FTerminusSessionInfo Info;
	
};
