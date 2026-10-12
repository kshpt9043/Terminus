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

	// 여러 화면이 같이 쓰는 색. 화면 하나에서만 쓰는 색은 그 cpp 에 둠
	namespace Color
	{
		// 글자
		inline const FLinearColor Gold(1.f, 0.85f, 0.35f);          // 제목 / 강조 / 재화
		inline const FLinearColor TextBright(0.9f, 0.9f, 0.9f);     // 본문
		inline const FLinearColor TextSoft(0.85f, 0.85f, 0.85f);    // 안내 문구
		inline const FLinearColor TextMuted(0.8f, 0.8f, 0.8f);      // 부제목 / 보조 설명
		inline const FLinearColor TextDim(0.7f, 0.7f, 0.7f);        // 흐린 설명 / 비어 있음
		inline const FLinearColor Warning(1.f, 0.45f, 0.4f);        // 경고 / 부족

		// 배경
		inline const FLinearColor Dim(0.f, 0.f, 0.f, 0.6f);         // 뒤 화면을 어둡게
		inline const FLinearColor DimStrong(0.f, 0.f, 0.f, 0.75f);
		inline const FLinearColor Panel(0.05f, 0.05f, 0.07f, 0.97f);    // 가운데 판
		inline const FLinearColor Window(0.07f, 0.065f, 0.06f, 0.97f);  // 거점 / 창고 같은 전체 창

		// 버튼
		inline const FLinearColor ButtonIdle(0.24f, 0.24f, 0.28f);
		inline const FLinearColor ButtonConfirm(0.25f, 0.55f, 0.3f);
	}
}
