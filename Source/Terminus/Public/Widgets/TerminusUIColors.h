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
}
