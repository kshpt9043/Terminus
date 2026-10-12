#include "Widgets/Map/RoomTopBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Map/MapTopBarWidget.h"

void URoomTopBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 화면 전체를 덮는 세로 상자. 맨 위에 상단바, 나머지는 비워 둠
	Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomTopBarRoot"));
	WidgetTree->RootWidget = Root;

	// 상단바 말고는 클릭이 아래(방 화면)로 통과
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URoomTopBarWidget::Setup(TSubclassOf<UMapTopBarWidget> TopBarClass)
{
	if (TopBar || !Root) return;

	TopBar = CreateWidget<UMapTopBarWidget>(this, TopBarClass ? TopBarClass : TSubclassOf<UMapTopBarWidget>(UMapTopBarWidget::StaticClass()));
	if (UVerticalBoxSlot* S = TopBar ? Root->AddChildToVerticalBox(TopBar) : nullptr)
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}
}
