#include "Widgets/Common/TerminusUserWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Widgets/Common/TerminusUIBuilder.h"

void UTerminusUserWidget::NativeConstruct()
{
	// 자식 클래스의 NativeOnInitialized(기본 레이아웃 만들기)는 이미 끝난 뒤
	// 이후에 MakeText 로 만드는 글자는 처음부터 테마 글꼴이라 다시 안 훑어도 됨
	if (bApplyTheme)
	{
		TerminusUI::ApplyTheme(WidgetTree);
	}

	Super::NativeConstruct();
}
