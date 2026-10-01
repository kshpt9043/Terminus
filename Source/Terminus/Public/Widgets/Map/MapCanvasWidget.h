// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Map/MapEdgeLayer.h"
#include "MapCanvasWidget.generated.h"

struct FRoomNode;
class AMapManager;
class ADungeonArea;
class ATerminusPlayerState;
class UScrollBox;
enum class ERoomType : uint8;
class UCanvasPanel;
class URoomNodeWidget;
class UTexture2D;

/**
 * 던전 지도. Slay the Spire 방식으로 그린다
 *  - 방 위치 = 격자(열, 줄) x 간격 + 방마다 고정된 작은 흔들림. 지도를 받을 때 한 번 계산해서 저장
 *  - 연결선 = 그 좌표 사이에 점을 찍는 전용 레이어(UMapEdgeLayer). 방 아이콘 뒤, 같은 캔버스 안
 *  - 화면 위치를 조회하지 않음 -> 창 크기가 바뀌어도 선이 밀리거나 깨지지 않음. 크기 대응은 UMG DPI 배율이 함
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

	// 열 사이 간격(px). 아이콘이 실제로 보이는 크기(RoomIconVisibleSize)보다 넉넉히 커야 옆 방과 안 붙음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "50.0"))
	float ColumnSpacing = 230.0f;

	// 줄(레벨) 사이 간격(px). 아이콘 보이는 크기 + 점선이 보일 틈. 좁으면 위아래 방이 붙어서 줄 구분이 안 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "50.0"))
	float RowSpacing = 250.0f;

	// 줄 안에서 방을 가운데로 고르게 펼치는 정도. 0 = 열 위치 그대로, 1 = 줄마다 가운데 기준 같은 간격
	// 열 위치 그대로면 방 2개짜리 줄이 한쪽(0,1열)에 쏠려 지도가 기울어 보이고 가로로 긴 선이 생김
	// 줄 안 순서는 안 바뀌므로 선이 엇갈리지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RowSpread = 0.5f;

	// 지도 가장자리 여백 (좌우, 위아래). 모자라면 아이콘이 안 잘리게 자동으로 늘림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	FVector2D MapPadding = FVector2D(190.0f, 170.0f);

	// 방 위치 흔들림. 간격 대비 비율 (X, Y). 자연스러워 보이게
	// 아이콘끼리 겹치지 않는 만큼으로 자동 제한됨 (RoomIconVisibleSize 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "0.0", ClampMax = "0.45"))
	FVector2D PositionJitter = FVector2D(0.18f, 0.15f);

	// 방 아이콘 그림이 실제로 보이는 크기(px). 아이콘 상자(200)에서 투명 여백을 뺀 크기
	// 지금 아이콘들은 그림이 상자의 55~65% -> 약 130. 흔들림 제한에 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ClampMin = "0.0"))
	float RoomIconVisibleSize = 130.0f;

	// 방 위젯 왼쪽 위에서 아이콘 중심까지의 거리(px). 이 점이 방 좌표에 놓이고 선도 여기서 나감
	// 비율(가운데 정렬)로 안 하는 이유: 방 위젯은 [아이콘 200x200][선택한 사람 초상화] 가로 배치라
	// 누가 방을 고르면 위젯 폭이 늘어남 -> 가운데 기준이면 아이콘이 옆으로 밀려 선과 어긋남
	// WBP_RoomNodeWidget 의 아이콘 SizeBox 크기를 바꾸면 여기도 그 절반으로 맞출 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	FVector2D RoomIconCenter = FVector2D(100.0f, 100.0f);

	// 연결선(점선) 모양
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|Edge")
	FMapEdgeStyle EdgeStyle;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// 전체 지도 세팅 함수. 지도 데이터가 이미 그려진 것과 같으면 위젯은 그대로 두고 상태만 갱신
	UFUNCTION(BlueprintCallable, Category = "Map")
	void BuildMapUI(const TArray<FRoomNode>& MapData);

	// 전체 방 위젯들의 선택 상태 동기화 갱신
	UFUNCTION(BlueprintCallable, Category = "Map")
	void RefreshAllRoomSelections();

	// 이미 만들어진 방 위젯들의 상태(선택 가능 여부 + 선택 표시)만 갱신. 위젯 재생성 / 스크롤 이동 없음
	UFUNCTION(BlueprintCallable, Category = "Map")
	void RefreshRoomStates();

protected:
	UFUNCTION()
	void HandleRoomClicked(int32 ClickedRoomId);
	
	// 내 RunState 변경 -> 현재 위치가 바뀌었을 수 있으니 방 상태 갱신 (지도 재생성 X)
	UFUNCTION()
	void OnPlayerRunStateChanged(const FRunState& NewRunState);
	
	// 다른 사람 RunState 변경 -> 선택 표시(초상화)만 갱신
	UFUNCTION()
	void OnOtherRunStateChanged(const FRunState& NewRunState);

	// 던전 구역에 들어가면 지도를 숨기고, 지도로 돌아오면 다시 보임
	UFUNCTION()
	void HandleViewAreaChanged(ADungeonArea* NewArea);

	// 숨겼다가 되돌릴 원래 가시성
	ESlateVisibility VisibleState = ESlateVisibility::SelfHitTestInvisible;
	
	// PlayerState 들은 복제가 위젯보다 늦게 올 수 있어서 한 번에 못 묶음
	// 타이머로 주기적으로 훑어서 새로 보이는 PS 에 바인딩함
	void TryBindPlayerStates();
	
	FTimerHandle BindTimer;
	
	// 지도를 받아올 MapManager. 서버가 스폰한 걸 클라가 복제로 받는 경우
	// 위젯보다 늦게 도착할 수 있어서 BindTimer 가 찾을 때까지 계속 찾음
	TWeakObjectPtr<AMapManager> BoundMapManager;
	
	// 쓸 수 있는 MapManager 찾기. 클라에선 복제로 온 것만 인정 (클라 로컬 스폰본은 가짜 지도)
	AMapManager* FindMapManager() const;

private:
	// 생성된 위젯들을 RoomId로 빠르게 찾기 위한 Map
	UPROPERTY()
	TMap<int32, URoomNodeWidget*> CreatedRoomWidgets;

	// 맵 원본 데이터
	TArray<FRoomNode> CachedMapData;

	// 방 좌표 (지도 좌표, 방 중심). 방 위젯과 연결선이 둘 다 이 값만 씀
	TMap<int32, FVector2D> RoomPositions;

	// 지도 전체 크기 (여백 포함). 연결선 레이어 크기 = 캔버스 크기 = 스크롤 범위
	FVector2D MapContentSize = FVector2D::ZeroVector;

	// 연결선 레이어. 캔버스의 첫 자식(방 아이콘 뒤)
	UPROPERTY()
	TObjectPtr<UMapEdgeLayer> EdgeLayer;

	// 지도 데이터로 방 좌표와 지도 크기 계산
	void ComputeLayout(const TArray<FRoomNode>& MapData);

	// 연결선 상태(지나온 길 / 갈 수 있는 길 / 나머지)를 다시 계산해서 레이어에 넘김
	void RefreshEdges(const ATerminusPlayerState* LocalPS);

	// 처음 그릴 때만 맨 아래(시작 지점)로 스크롤. 이후엔 사용자가 보던 위치 유지
	bool bInitialScrollDone = false;

	// 내 현재 위치 기준으로 이 방을 누를 수 있는가
	bool IsRoomSelectable(const FRoomNode& Node, const ATerminusPlayerState* LocalPS) const;

	// 새로 받은 지도가 지금 그려진 지도와 같은가 (같은 지도가 다시 복제돼 와도 재생성하지 않기 위해)
	static bool IsSameMap(const TArray<FRoomNode>& A, const TArray<FRoomNode>& B);

};
