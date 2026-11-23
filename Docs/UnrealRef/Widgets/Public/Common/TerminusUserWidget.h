#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerminusUserWidget.generated.h"

/**
 * 프로젝트 위젯 공통 부모. 화면에 붙을 때 스타일 가이드 테마를 입힘 (TerminusUI::ApplyTheme)
 *  - 글자: Noto Sans KR (크기 / 굵기는 WBP 에서 정한 그대로), 디자이너 기본 흰 글자 -> 본문색
 *  - 손대지 않은 기본 모양 버튼 -> 테마 버튼
 * WBP 디자이너에서 직접 고른 글꼴 / 색은 그대로 둠 (Roboto 만 바뀜)
 */
UCLASS(Abstract)
class TERMINUS_API UTerminusUserWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 끄면 이 위젯 트리는 테마를 안 입힘 (일부러 다른 글꼴을 쓰는 화면용)
	UPROPERTY(EditAnywhere, Category = "Terminus Theme")
	bool bApplyTheme = true;
};
