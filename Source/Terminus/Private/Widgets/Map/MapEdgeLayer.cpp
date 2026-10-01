#include "Widgets/Map/MapEdgeLayer.h"

#include "Rendering/DrawElements.h"

void UMapEdgeLayer::SetEdgeStyle(const FMapEdgeStyle& InStyle)
{
	Style = InStyle;

	// 둥근 점은 크기의 절반으로 모서리를 깎아야 원이 됨
	RoundDotBrush = FSlateRoundedBoxBrush(FLinearColor::White, FVector2f(Style.DotSize));

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UMapEdgeLayer::SetEdges(TArray<FMapEdgeDraw> InEdges)
{
	InEdges.StableSort([](const FMapEdgeDraw& A, const FMapEdgeDraw& B)
	{
		return static_cast<uint8>(A.State) < static_cast<uint8>(B.State);
	});

	Edges = MoveTemp(InEdges);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UMapEdgeLayer::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FSlateBrush* Brush = Style.DotBrush.GetResourceObject() ? &Style.DotBrush : &RoundDotBrush;
	const float Spacing = FMath::Max(2.f, Style.DotSpacing);
	const FLinearColor InheritedTint = InWidgetStyle.GetColorAndOpacityTint();

	for (const FMapEdgeDraw& Edge : Edges)
	{
		const FVector2D Delta = Edge.To - Edge.From;
		const float Length = Delta.Size();
		const float Usable = Length - Style.EndPadding * 2.f;
		if (Usable <= 0.f) continue;   // 아이콘끼리 너무 붙어 있음

		const FVector2D Dir = Delta / Length;
		const float Angle = FMath::Atan2(Dir.Y, Dir.X);   // 라디안

		FLinearColor Color = Style.DefaultColor;
		float DotScale = 1.f;
		switch (Edge.State)
		{
		case EMapEdgeState::Available: Color = Style.AvailableColor; DotScale = Style.EmphasisDotScale; break;
		case EMapEdgeState::Taken:     Color = Style.TakenColor;     DotScale = Style.EmphasisDotScale; break;
		default: break;
		}

		const FVector2D DotSize = Style.DotSize * DotScale;

		// 점 개수를 정하고 간격을 선 길이에 맞게 살짝 늘림 -> 양 끝 여백이 항상 같음
		const int32 Count = FMath::FloorToInt(Usable / Spacing) + 1;
		const float Step = Count > 1 ? Usable / (Count - 1) : 0.f;
		const FVector2D Start = Edge.From + Dir * (Count > 1 ? Style.EndPadding : Length * 0.5f);

		for (int32 k = 0; k < Count; ++k)
		{
			const FVector2D Center = Start + Dir * (Step * k);

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				MaxLayer,
				AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(Center - DotSize * 0.5f)),
				Brush,
				ESlateDrawEffect::None,
				Angle,
				TOptional<FVector2f>(),   // 점 가운데를 기준으로 회전
				FSlateDrawElement::RelativeToElement,
				Color * InheritedTint);
		}
	}

	return MaxLayer;
}
