#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "MapEdgeLayer.generated.h"

// 지도 연결선 상태. 그리는 순서도 이 순서 (뒤가 위에 그려짐)
UENUM(BlueprintType)
enum class EMapEdgeState : uint8
{
	Default,     // 그 외 모든 길
	Available,   // 지금 위치에서 갈 수 있는 길
	Taken        // 지나온 길 + 지금 고른 길
};

// 점선 모양. 지도 위젯(WBP_MapCanvasWidget)의 디테일에서 조정
USTRUCT(BlueprintType)
struct FMapEdgeStyle
{
	GENERATED_BODY()

	// 점 이미지. 비워 두면 둥근 점. 이미지를 쓰면 선 방향으로 회전돼서 찍힘 (가로가 선 방향)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	FSlateBrush DotBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	FVector2D DotSize = FVector2D(20.f, 20.f);

	// 점 사이 간격 (지도 좌표 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge", meta = (ClampMin = "2.0"))
	float DotSpacing = 19.f;

	// 선 양 끝을 비우는 길이. 방 아이콘이 실제로 보이는 반지름쯤 -> 점이 아이콘 밑으로 파고들지 않음
	// (지도 시안 아이콘은 64 -> 반지름 32 + 여유)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	float EndPadding = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	FLinearColor DefaultColor = FLinearColor(0.451f, 0.417f, 0.381f, 0.6f);   // #B3ADA6 60% 흐린 먹

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	FLinearColor AvailableColor = FLinearColor(1.f, 0.846f, 0.351f, 1.f);     // #FFEDA0 등불 먹

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge")
	FLinearColor TakenColor = FLinearColor(0.027f, 0.024f, 0.020f, 1.f);      // #2E2B27 마른 먹

	// 갈 수 있는 길 / 지나온 길의 점 크기 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge", meta = (ClampMin = "0.1"))
	float EmphasisDotScale = 1.15f;

	// 갈 수 있는 길(등불 먹) 뒤에 번지는 빛. 비워 두면 /Game/UI/Textures/Map/T_Map_SoftDot
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge|Glow")
	FSlateBrush GlowBrush;

	// 빛 크기 (점 크기 배율) / 진하기. 진하기 0 = 빛 없음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge|Glow", meta = (ClampMin = "1.0"))
	float GlowScale = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge|Glow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlowOpacity = 0.35f;
};

// 그릴 선 하나. 좌표는 지도 좌표 (이 레이어 기준 로컬)
struct FMapEdgeDraw
{
	FVector2D From;
	FVector2D To;
	EMapEdgeState State = EMapEdgeState::Default;
};

/**
 * 지도 연결선 레이어. 지도 캔버스 안, 방 아이콘들 뒤에 지도와 같은 크기로 깔림.
 *
 * Slay the Spire 처럼 두 방 사이에 작은 점을 일정 간격으로 찍어 점선을 만든다.
 * 좌표는 지도 위젯이 계산한 지도 좌표를 그대로 받음 -> 화면 위치 조회가 없어서
 * 창 크기가 바뀌어도 한 프레임 밀리거나 깨지지 않고, 스크롤 / 잘라내기 / 배율이 방 아이콘과 똑같이 적용됨
 */
UCLASS()
class TERMINUS_API UMapEdgeLayer : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	void SetEdgeStyle(const FMapEdgeStyle& InStyle);

	// 상태 순으로 정렬해서 보관 (지나온 길이 맨 위에 그려지게)
	void SetEdges(TArray<FMapEdgeDraw> InEdges);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// UPROPERTY: 브러시가 들고 있는 텍스처(점 / 빛)를 GC 가 지우지 않게
	UPROPERTY()
	FMapEdgeStyle Style;

	// 점 이미지가 없을 때 쓰는 둥근 점
	FSlateRoundedBoxBrush RoundDotBrush = FSlateRoundedBoxBrush(FLinearColor::White, FVector2f(6.f, 6.f));

	TArray<FMapEdgeDraw> Edges;
};
