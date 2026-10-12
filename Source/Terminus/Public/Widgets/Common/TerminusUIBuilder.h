#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextLayout.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UVerticalBoxSlot;
class UWidget;
class UWidgetTree;

/**
 * C++ 기본 레이아웃을 만드는 공통 도우미. 위젯마다 따로 있던 MakeXxxText / MakeXxxButton / AddCentered 를 모은 것
 * (전부 같은 namespace 라 unity 빌드에서 이름이 겹칠 걱정도 없음)
 */
namespace TerminusUI
{
	// 글자 하나. Initial 이 비어 있으면 글자를 넣지 않음
	TERMINUS_API UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);
	TERMINUS_API UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FText& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);

	// MakeText + 자동 줄바꿈
	TERMINUS_API UTextBlock* MakeWrapText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);

	// 월드 위에 뜨는 글자처럼 배경 없이 읽혀야 할 때 그림자
	TERMINUS_API void AddShadow(UTextBlock* Text, float Opacity = 0.85f);

	// 글자 버튼. 배경색은 UButton 기본(흰색)이면 그대로
	TERMINUS_API UButton* MakeButton(UWidgetTree* Tree, const FName& Name, const FString& Label, int32 Size,
		const FLinearColor& TextColor = FLinearColor::Black, const FLinearColor& Background = FLinearColor::White);

	// 세로 상자에 한 줄 추가 (위 여백 + 가로 정렬)
	TERMINUS_API UVerticalBoxSlot* AddRow(UVerticalBox* Column, UWidget* Widget, float Top, EHorizontalAlignment Align = HAlign_Center);

	// 화면 전체를 덮는 어두운 배경을 루트로 (내용은 가운데). 아래 화면을 못 누르게 하는 용도
	TERMINUS_API UBorder* MakeDimRoot(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color);

	// 가운데 판 (배경색 + 안쪽 여백)
	TERMINUS_API UBorder* MakePanel(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, const FMargin& Padding);

	// 같은 글자면 다시 넣지 않음 (매 틱 / 자주 부르는 곳에서 레이아웃이 다시 잡히지 않게)
	TERMINUS_API void SetTextIfChanged(UTextBlock* Text, const FText& Value);
	TERMINUS_API void SetTextIfChanged(UTextBlock* Text, const FString& Value);
}
