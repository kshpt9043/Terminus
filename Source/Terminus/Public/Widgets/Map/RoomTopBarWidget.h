#pragma once

#include "CoreMinimal.h"
#include "Widgets/Common/TerminusUserWidget.h"
#include "RoomTopBarWidget.generated.h"

class UMapTopBarWidget;
class UVerticalBox;

/**
 * 휴식 / 이벤트 / 상점 화면 위에 지도 상단바를 띄우는 틀 (사용자 결정 2026-10-12)
 * 지도 화면은 방에 들어가면 통째로 숨겨지므로, 같은 상단바 클래스(WBP_MapTopBar)를 하나 더 만들어 화면 맨 위에 붙임
 * 상단바 아래는 비어 있고 클릭이 통과함 (방 화면을 그대로 누를 수 있게)
 *
 * PC 가 방 화면을 열고 닫을 때 보이고 숨김. 따로 WBP 를 만들 필요 없음 (모양은 상단바 WBP 를 따름)
 */
UCLASS()
class TERMINUS_API URoomTopBarWidget : public UTerminusUserWidget
{
	GENERATED_BODY()

public:
	// 상단바 클래스 (보통 지도 화면의 상단바와 같은 WBP). 처음 한 번만 만듦
	void Setup(TSubclassOf<UMapTopBarWidget> TopBarClass);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> Root;

	UPROPERTY()
	TObjectPtr<UMapTopBarWidget> TopBar;
};
