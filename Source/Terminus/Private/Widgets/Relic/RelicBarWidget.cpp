#include "Widgets/Relic/RelicBarWidget.h"
#include "Widgets/Common/ItemSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Player/TerminusPlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

void URelicBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryBind();
}

void URelicBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void URelicBarWidget::TryBind()
{
	const APlayerController* PC = GetOwningPlayer();
	ATerminusPlayerState* PS = PC ? PC->GetPlayerState<ATerminusPlayerState>() : nullptr;
	UWorld* World = GetWorld();

	if (!PS)
	{
		// 클라: PlayerState 가 아직 안 옴 -> 잠깐 뒤 다시
		if (World && !BindTimer.IsValid())
		{
			World->GetTimerManager().SetTimer(BindTimer, this, &URelicBarWidget::TryBind, 0.25f, true);
		}
		if (!bShownOnce) Rebuild(TArray<FName>());
		return;
	}

	if (World) World->GetTimerManager().ClearTimer(BindTimer);
	if (WatchedPS.Get() != PS)
	{
		Unbind();
		PS->OnRunStateChanged.AddUniqueDynamic(this, &URelicBarWidget::HandleRunStateChanged);
		WatchedPS = PS;
	}
	Rebuild(PS->GetRelics());
}

void URelicBarWidget::Unbind()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindTimer);
	}
	if (ATerminusPlayerState* PS = WatchedPS.Get())
	{
		PS->OnRunStateChanged.RemoveDynamic(this, &URelicBarWidget::HandleRunStateChanged);
	}
	WatchedPS = nullptr;
}

void URelicBarWidget::HandleRunStateChanged(const FRunState& NewRunState)
{
	if (!bShownOnce || NewRunState.Relics != ShownRelics)
	{
		Rebuild(NewRunState.Relics);
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
