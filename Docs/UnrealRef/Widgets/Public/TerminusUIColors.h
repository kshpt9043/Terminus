#pragma once

#include "CoreMinimal.h"

// 1d 팔레트 도우미. FColor 는 sRGB 라 FLinearColor 로 바꾸면 에디터 Hex 칸과 같은 색이 된다
// cpp 마다 익명 namespace 에 두면 unity 빌드에서 두 파일이 한 덩어리로 묶일 때 이름이 겹친다
namespace TerminusUI
{
	inline FLinearColor Hex(const TCHAR* InHex)
	{
		return FLinearColor(FColor::FromHex(InHex));
	}

	inline FLinearColor Hex(const TCHAR* InHex, float Alpha)
	{
		FLinearColor Color = Hex(InHex);
		Color.A = Alpha;
		return Color;
	}

	// 스타일 가이드 보드(1920x1080) 팔레트. "이 값만 사용"
	// 여러 화면이 같이 쓰는 색. 화면 하나에서만 쓰는 색은 그 cpp 에 둠
	namespace Color
	{
		// 배경 / 판
		inline const FLinearColor Inset = Hex(TEXT("111317"));        // 칸 안쪽 / 빈 칸
		inline const FLinearColor Board = Hex(TEXT("1B1E25"));        // 기본 판 / 툴팁
		inline const FLinearColor Section = Hex(TEXT("23272E"));      // 섹션
		inline const FLinearColor Card = Hex(TEXT("262A32"));         // 칸 / 카드
		inline const FLinearColor Disabled = Hex(TEXT("2E333D"));     // 비활성 / 호버 바탕
		inline const FLinearColor Line = Hex(TEXT("3A3F4A"));         // 테두리

		// 글자
		inline const FLinearColor TextBody = Hex(TEXT("ECE3CF"));     // 본문
		inline const FLinearColor TextSub = Hex(TEXT("C9C1AE"));      // 부제목
		inline const FLinearColor TextAux = Hex(TEXT("A79F8E"));      // 보조
		inline const FLinearColor TextFaint = Hex(TEXT("8D8778"));    // 흐림

		// 금 / 상태
		inline const FLinearColor GoldHover = Hex(TEXT("E6C47A"));    // 호버 / 선택 글자
		inline const FLinearColor GoldBase = Hex(TEXT("C9A45C"));     // 기본 금 (제목 / 구분선)
		inline const FLinearColor GoldPressed = Hex(TEXT("A8894A"));  // 눌림 / 어두운 금
		inline const FLinearColor Ready = Hex(TEXT("6FB380"));        // 준비 / 선택됨
		inline const FLinearColor Alert = Hex(TEXT("C23B33"));        // 경고 / 심층

		// 버튼 바탕 (육각 플레이트 안쪽색)
		inline const FLinearColor PrimaryFill = Hex(TEXT("A8322C"));    // 주 행동
		inline const FLinearColor SecondaryFill = Hex(TEXT("3F7F86"));  // 보조 행동
		inline const FLinearColor CurrencyFill = Hex(TEXT("A88100"));   // 재화 / 구매

		// ---- 예전 이름 (여러 화면이 이미 씀). 값만 위 팔레트로 ----
		inline const FLinearColor Gold = GoldBase;            // 제목 / 강조 / 재화
		inline const FLinearColor TextBright = TextBody;      // 본문
		inline const FLinearColor TextSoft = TextSub;         // 안내 문구
		inline const FLinearColor TextMuted = TextAux;        // 부제목 / 보조 설명
		inline const FLinearColor TextDim = TextFaint;        // 흐린 설명 / 비어 있음
		inline const FLinearColor Warning = Alert;            // 경고 / 부족

		// 배경
		inline const FLinearColor Dim(0.f, 0.f, 0.f, 0.7f);         // 뒤 화면을 어둡게 (검정 70%)
		inline const FLinearColor DimStrong(0.f, 0.f, 0.f, 0.8f);
		inline const FLinearColor Panel = Board;                     // 가운데 판
		inline const FLinearColor Window = Hex(TEXT("111317"), 0.98f);  // 거점 / 창고 같은 전체 창

		// 버튼
		inline const FLinearColor ButtonIdle = Card;
		inline const FLinearColor ButtonConfirm = PrimaryFill;
	}

	// 던전 지도 화면 팔레트 (지도 시안 전용. 종이 / 먹 / 파티 판)
	namespace MapColor
	{
		inline const FLinearColor OutsidePaper = Hex(TEXT("0E0E10"));  // 종이 밖
		inline const FLinearColor TopBar = Hex(TEXT("313131"));        // 상단바
		inline const FLinearColor Plate = Hex(TEXT("23202A"));         // 판 (재화 / 유물 칸)
		inline const FLinearColor PartyPlate = Hex(TEXT("3A253D"));    // 파티 판
		inline const FLinearColor PartyEdge = Hex(TEXT("24162A"));
		inline const FLinearColor ChipFill = Hex(TEXT("5A4A5C"));      // 동료 칩 / 초상화 자리
		inline const FLinearColor Health = Hex(TEXT("FF1A4C"));        // 체력
		inline const FLinearColor HealthBack = Hex(TEXT("1A1218"));
		inline const FLinearColor Text = Hex(TEXT("E8DFCB"));
		inline const FLinearColor TextSub = Hex(TEXT("A89F8C"));
		inline const FLinearColor InkTaken = Hex(TEXT("2E2B27"));      // 지나온 길 (마른 먹)
		inline const FLinearColor InkDefault = Hex(TEXT("B3ADA6"), 0.6f);  // 길 기본 (흐린 먹)
		inline const FLinearColor Lantern = Hex(TEXT("FFEDA0"));       // 갈 수 있는 길 (등불)
		inline const FLinearColor RedInk = Hex(TEXT("C8352E"));        // 내 선택
		inline const FLinearColor BlueTorch = Hex(TEXT("5B8BD9"));     // 전투 중
		inline const FLinearColor GreenMana = Hex(TEXT("7BC46A"));     // 준비
	}
}
