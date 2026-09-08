// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/TerminusPlayerController.h"

#include "Widgets/TavernWidget.h"
#include "Blueprint/UserWidget.h"


void ATerminusPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// 다른 플레이어의 컨트롤러는 리턴
	if (!IsLocalController())
	{
		return;
	}
	
	if (!TavernWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PC: TavernWidgetClass 가 비어 있음. BP 디폴트에서 WBP_TavernWidget 을 지정할 것"));
		return;
	}
	
	TavernWidget = CreateWidget<UTavernWidget>(this, TavernWidgetClass);
	if (!TavernWidget)
	{
		return;
	}
	TavernWidget->AddToViewport();
	
	FInputModeUIOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = true;
}
