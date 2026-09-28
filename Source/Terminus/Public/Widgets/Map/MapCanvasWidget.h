// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapCanvasWidget.generated.h"

struct FRoomNode;
class AMapManager;
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
	
	// 연결선 색상 및 두께 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|Line")
	FLinearColor LineColor = FLinearColor(0.8f, 0.8f, 0.8f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|Line")
	float LineThickness = 3.0f;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// 전체 지도 세팅 함수
	UFUNCTION(BlueprintCallable, Category = "Map")
	void BuildMapUI(const TArray<FRoomNode>& MapData);
	
	// 전체 방 위젯들의 선택 상태 동기화 갱신
	UFUNCTION(BlueprintCallable, Category = "Map")
	void RefreshAllRoomSelections();

protected:
	UFUNCTION()
	void HandleRoomClicked(int32 ClickedRoomId);
	
	// 내 RunState 변경 -> 현재 위치가 바뀌었을 수 있으니 지도 다시 그림
	UFUNCTION()
	void OnPlayerRunStateChanged(const FRunState& NewRunState);
	
	// 다른 사람 RunState 변경 -> 선택 표시(초상화)만 갱신
	UFUNCTION()
	void OnOtherRunStateChanged(const FRunState& NewRunState);
	
	// PlayerState 들은 복제가 위젯보다 늦게 올 수 있어서 한 번에 못 묶음
	// 타이머로 주기적으로 훑어서 새로 보이는 PS 에 바인딩함
	void TryBindPlayerStates();
	
	FTimerHandle BindTimer;
	
	// 지도를 받아올 MapManager. 서버가 스폰한 걸 클라가 복제로 받는 경우
	// 위젯보다 늦게 도착할 수 있어서 BindTimer 가 찾을 때까지 계속 찾음
	TWeakObjectPtr<AMapManager> BoundMapManager;
	
	// 쓸 수 있는 MapManager 찾기. 클라에선 복제로 온 것만 인정 (클라 로컬 스폰본은 가짜 지도)
	AMapManager* FindMapManager() const;
	
	// UMG의 UI 렌더링 단계에서 연결선을 그리기 위한 오버라이드
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// 생성된 위젯들을 RoomId로 빠르게 찾기 위한 Map
	UPROPERTY()
	TMap<int32, URoomNodeWidget*> CreatedRoomWidgets;
	
	// 맵 원본 데이터 저장 (OnPaint에서 선을 그릴 때 참조)
	TArray<FRoomNode> CachedMapData;
	
};
