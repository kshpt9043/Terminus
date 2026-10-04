#include "Widgets/Relic/RelicBarWidget.h"
#include "Widgets/Common/ItemSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Player/TerminusPlayerState.h"

void URelicBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !RelicBox)
	{
		BuildDefaultLayout();
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URelicBarWidget::BuildDefaultLayout()
{
	RelicBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RelicBox"));
	WidgetTree->RootWidget = RelicBox;
}

void URelicBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	const TArray<FName> Relics = PS ? PS->GetRelics() : TArray<FName>();

	if (!bShownOnce || Relics != ShownRelics)
	{
		Rebuild(Relics);
	}
}

void URelicBarWidget::Rebuild(const TArray<FName>& Relics)
{
	bShownOnce = true;
	ShownRelics = Relics;

	if (!RelicBox) return;
	RelicBox->ClearChildren();

	// 칸 위젯은 모자랄 때만 더 만들고 재사용
	const TSubclassOf<UItemSlotWidget> Class = SlotClass ? SlotClass : TSubclassOf<UItemSlotWidget>(UItemSlotWidget::StaticClass());
	while (Slots.Num() < Relics.Num())
	{
		UItemSlotWidget* NewSlot = CreateWidget<UItemSlotWidget>(this, Class);
		if (!NewSlot) break;
		NewSlot->SetShowName(false);
		if (!SlotClass) NewSlot->SetSlotSize(FVector2D(IconSize, IconSize));
		Slots.Add(NewSlot);
	}

	for (int32 i = 0; i < Relics.Num() && Slots.IsValidIndex(i); ++i)
	{
		UItemSlotWidget* RelicSlot = Slots[i];
		RelicSlot->SetRelic(Relics[i]);

		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(RelicBox->AddChild(RelicSlot)))
		{
			HSlot->SetPadding(FMargin(i == 0 ? 0.f : Spacing, 0.f, 0.f, 0.f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}
