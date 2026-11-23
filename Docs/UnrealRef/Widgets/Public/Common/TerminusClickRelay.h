#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TerminusClickRelay.generated.h"

class UButton;

/**
 * 버튼마다 다른 값을 넘기는 중계 (UButton 의 OnClicked 는 인자가 없음). 위젯마다 따로 있던 XxxClickRelay 를 하나로
 *
 *   UTerminusClickRelay::Bind(Button, this, FSimpleDelegate::CreateUObject(this, &UMyWidget::HandlePick, Index), Relays);
 *
 * Relays 는 쓰는 쪽의 UPROPERTY 배열 (GC 되지 않게 붙잡아 둠). 버튼을 다시 만들 때 Reset
 */
UCLASS()
class TERMINUS_API UTerminusClickRelay : public UObject
{
	GENERATED_BODY()

public:
	static void Bind(UButton* Button, UObject* Outer, FSimpleDelegate Callback, TArray<TObjectPtr<UTerminusClickRelay>>& Keep);

private:
	FSimpleDelegate Callback;

	UFUNCTION()
	void HandleClicked();
};
