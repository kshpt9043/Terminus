// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapCanvasWidget.generated.h"

class UScrollBox;
enum class ERoomType : uint8;
class UCanvasPanel;
class URoomNodeWidget;
class UTexture2D;

/**
 * 
 */
UCLASS()
class TERMINUS_API UMapCanvasWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// UMG의 바탕 CanvasPanel 바인딩
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* MapCanvasPanel;
	
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ScrollBox;
	

	// 에디터 패널에서 ERoomType별 아이콘 이미지들을 직접 등록할 TMap
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	TMap<ERoomType, UTexture2D*> RoomTypeIcons;

	// 방 위젯 클래스 리소스 (WBP_RoomNode 할당)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	TSubclassOf<URoomNodeWidget> RoomWidgetClass;

	// 격자 간격 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	FVector2D CellSize = FVector2D(120.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	FVector2D CanvasOffset = FVector2D(100.0f, 100.0f);

	void NativeConstruct();
	// 전체 지도 세팅 함수
	UFUNCTION(BlueprintCallable, Category = "Map")
	void BuildMapUI(const TArray<FRoomNode>& MapData);

protected:
	UFUNCTION()
	void HandleRoomClicked(int32 ClickedRoomId);

private:
	// 생성된 위젯들을 RoomId로 빠르게 찾기 위한 Map
	UPROPERTY()
	TMap<int32, URoomNodeWidget*> CreatedRoomWidgets;
	
};
