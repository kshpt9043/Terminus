#include "Widgets/Common/TerminusClickRelay.h"

#include "Components/Button.h"

void UTerminusClickRelay::Bind(UButton* Button, UObject* Outer, FSimpleDelegate Callback, TArray<TObjectPtr<UTerminusClickRelay>>& Keep)
{
	if (!Button || !Outer) return;

	UTerminusClickRelay* Relay = NewObject<UTerminusClickRelay>(Outer);
	Relay->Callback = MoveTemp(Callback);
	Button->OnClicked.AddDynamic(Relay, &UTerminusClickRelay::HandleClicked);
	Keep.Add(Relay);
}

void UTerminusClickRelay::HandleClicked()
{
	Callback.ExecuteIfBound();
}
