#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/TextLayout.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UVerticalBoxSlot;
class UWidget;
class UWidgetTree;

/**
 * C++ 기본 레이아웃을 만드는 공통 도우미. 위젯마다 따로 있던 MakeXxxText / MakeXxxButton / AddCentered 를 모은 것
 * (전부 같은 namespace 라 unity 빌드에서 이름이 겹칠 걱정도 없음)
 *
 * 스타일 가이드 보드 기준: 글꼴 Noto Sans KR, 글자 크기 40 / 22 / 17 / 16 / 13, 판 / 육각 버튼 / 금 구분선
 */
namespace TerminusUI
{
	// 글자 크기 단계 (스타일 가이드 02)
	namespace FontSize
	{
		constexpr int32 ScreenTitle = 40;   // 화면 제목 900
		constexpr int32 Section = 22;       // 섹션 제목 700
		constexpr int32 Subtitle = 17;      // 부제목 500
		constexpr int32 Body = 16;          // 본문 400
		constexpr int32 Aux = 13;           // 보조 400
	}

	// 육각 플레이트 버튼 종류 (스타일 가이드 03)
	enum class EHexButton : uint8
	{
		Primary,     // 주 행동 (붉은)
		Secondary,   // 보조 행동 (청록)
		Currency,    // 재화 / 구매 (금)
	};

	// ---- 글꼴 ----

	// Noto Sans KR (Content/UI/Fonts 의 TTF). bBold = 700 이상
	TERMINUS_API FSlateFontInfo GetFont(int32 Size, bool bBold = false);

	// 위젯 트리의 글자를 테마 글꼴로 바꿈 (크기 / 굵기 / 외곽선 유지). 흰 글자는 본문색으로,
	// 손대지 않은 기본 모양 버튼은 테마 버튼으로. 같은 트리에 여러 번 불러도 됨
	TERMINUS_API void ApplyTheme(UWidgetTree* Tree);

	// ---- 글자 ----

	// 글자 하나. Initial 이 비어 있으면 글자를 넣지 않음
	TERMINUS_API UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);
	TERMINUS_API UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FText& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);

	// MakeText + 자동 줄바꿈
	TERMINUS_API UTextBlock* MakeWrapText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Center);

	// 화면 제목 (40 / 900, 금, 자간 넓게)
	TERMINUS_API UTextBlock* MakeTitle(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size = FontSize::ScreenTitle);

	// 굵게 / 자간
	TERMINUS_API void SetBold(UTextBlock* Text, bool bBold = true);
	TERMINUS_API void SetLetterSpacing(UTextBlock* Text, int32 Spacing);

	// 월드 위에 뜨는 글자처럼 배경 없이 읽혀야 할 때 그림자
	TERMINUS_API void AddShadow(UTextBlock* Text, float Opacity = 0.85f);

	// ---- 버튼 ----

	// 보조 / 작은 버튼 (스타일 가이드 03 "보조 · 비활성": 6px 둥근 판, 호버 금 테두리)
	// Background 가 흰색(기본)이면 테마 판색, 아니면 그 색을 바탕으로
	TERMINUS_API UButton* MakeButton(UWidgetTree* Tree, const FName& Name, const FString& Label, int32 Size,
		const FLinearColor& TextColor = FLinearColor::White, const FLinearColor& Background = FLinearColor::White);

	// 육각 플레이트 버튼 (280x56, 양 끝 18px)
	TERMINUS_API UButton* MakeHexButton(UWidgetTree* Tree, const FName& Name, const FString& Label, EHexButton Kind, int32 Size = FontSize::Subtitle);

	// 이미 있는 버튼에 스타일 입히기
	TERMINUS_API FButtonStyle MakeFlatButtonStyle(const FLinearColor& Fill);
	TERMINUS_API FButtonStyle MakeHexButtonStyle(EHexButton Kind);
	TERMINUS_API void StyleHexButton(UButton* Button, EHexButton Kind);
	TERMINUS_API void StyleFlatButton(UButton* Button, const FLinearColor& Fill = FLinearColor::White);

	// ---- 판 ----

	// 세로 상자에 한 줄 추가 (위 여백 + 가로 정렬)
	TERMINUS_API UVerticalBoxSlot* AddRow(UVerticalBox* Column, UWidget* Widget, float Top, EHorizontalAlignment Align = HAlign_Center);

	// 화면 전체를 덮는 어두운 배경을 루트로 (내용은 가운데). 아래 화면을 못 누르게 하는 용도
	TERMINUS_API UBorder* MakeDimRoot(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color);

	// 가운데 판 (배경색 + 안쪽 여백). 6px 둥근 모서리 + 1px 테두리(#3A3F4A)
	TERMINUS_API UBorder* MakePanel(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, const FMargin& Padding);

	// 둥근 판 브러시 (바탕 + 테두리)
	TERMINUS_API FSlateBrush MakeBoxBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Radius = 6.f, float OutlineWidth = 1.f);

	// 금 구분선 (스타일 가이드 07). bTitle = 제목 아래 2px 금, 아니면 카드 안 1px 어두운 금. 양 끝이 흐려짐
	TERMINUS_API UImage* MakeGoldDivider(UWidgetTree* Tree, bool bTitle = false);

	// ---- 기타 ----

	// 같은 글자면 다시 넣지 않음 (매 틱 / 자주 부르는 곳에서 레이아웃이 다시 잡히지 않게)
	TERMINUS_API void SetTextIfChanged(UTextBlock* Text, const FText& Value);
	TERMINUS_API void SetTextIfChanged(UTextBlock* Text, const FString& Value);
}
