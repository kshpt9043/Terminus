#include "Widgets/Save/RunSaveListWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/TerminusSaveSubsystem.h"
#include "Widgets/Common/ConfirmPopupWidget.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/Save/RunSaveEntryWidget.h"

namespace
{
	UTextBlock* MakeSaveListText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Initial));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}
}

// =====================================================================
// 초기화 / 배치
// =====================================================================

void URunSaveListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget || !EntryBox)
	{
		BuildDefaultLayout();
	}

	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &URunSaveListWidget::HandleClose);

	SetVisibility(ESlateVisibility::Collapsed);
}

void URunSaveListWidget::NativeDestruct()
{
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

void URunSaveListWidget::BuildDefaultLayout()
{
	// 화면 전체 어둡게 + 가운데 창
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SaveListDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Dim;

	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	WindowSize->SetWidthOverride(760.f);
	WindowSize->SetHeightOverride(600.f);
	Dim->SetContent(WindowSize);

	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrushColor(FLinearColor(0.11f, 0.1f, 0.09f, 1.f));
	Window->SetPadding(FMargin(20.f));
	WindowSize->SetContent(Window);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Column);

	// 위: 제목 / 닫기
	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Top)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

	UTextBlock* Title = MakeSaveListText(WidgetTree, TEXT("TitleText"), TEXT("이어하기"), 22, FLinearColor::White);
	if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(Title))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLabel = MakeSaveListText(WidgetTree, NAME_None, TEXT("  닫기  "), 15, FLinearColor::Black);
	CloseButton->SetContent(CloseLabel);
	if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(CloseButton)) S->SetVerticalAlignment(VAlign_Center);

	StatusText = MakeSaveListText(WidgetTree, TEXT("StatusText"), FString(), 14, FLinearColor(1.f, 0.45f, 0.4f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	EmptyText = MakeSaveListText(WidgetTree, TEXT("EmptyText"), TEXT("저장된 진행이 없습니다. 던전에서 방을 끝내면 자동으로 저장됩니다."), 15, FLinearColor(0.7f, 0.7f, 0.7f));
	EmptyText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(EmptyText);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EntryBox"));
	EntryBox = Scroll;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

// =====================================================================
// 열기 / 닫기 / 갱신
// =====================================================================

void URunSaveListWidget::Open(const FString& InTavernMapPath)
{
	TavernMapPath = InTavernMapPath;
	bLoading = false;
	SetStatus(FText::GetEmpty());
	SetVisibility(ESlateVisibility::Visible);
	Refresh();

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &URunSaveListWidget::Close));
	}
}

void URunSaveListWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}
}

void URunSaveListWidget::HandleClose()
{
	Close();
}

void URunSaveListWidget::Refresh()
{
	UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this);
	const TArray<FRunSaveSummary> Saves = Save ? Save->GetRunSaves() : TArray<FRunSaveSummary>();

	if (EmptyText) EmptyText->SetVisibility(Saves.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!EntryBox) return;

	EntryBox->ClearChildren();
	const TSubclassOf<URunSaveEntryWidget> Class = EntryClass ? EntryClass : TSubclassOf<URunSaveEntryWidget>(URunSaveEntryWidget::StaticClass());

	for (const FRunSaveSummary& Summary : Saves)
	{
		URunSaveEntryWidget* Entry = CreateWidget<URunSaveEntryWidget>(this, Class);
		if (!Entry) continue;

		Entry->Setup(Summary);
		Entry->OnLoadClicked.BindUObject(this, &URunSaveListWidget::HandleLoad);
		Entry->OnDeleteClicked.BindUObject(this, &URunSaveListWidget::HandleDelete);

		UPanelSlot* PanelSlot = EntryBox->AddChild(Entry);
		if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
		{
			ScrollSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
	}
}

void URunSaveListWidget::SetStatus(const FText& Text)
{
	if (!StatusText) return;

	StatusText->SetText(Text);
	StatusText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// =====================================================================
// 불러오기 / 삭제
// =====================================================================

void URunSaveListWidget::HandleLoad(const FString& SlotName)
{
	if (bLoading) return;

	UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this);
	if (!Save) return;

	FText Error;
	if (!Save->ContinueRun(this, SlotName, TavernMapPath, DungeonMapPath, Error))
	{
		SetStatus(Error);
		Refresh();
		return;
	}

	// 주점으로 넘어가는 중. 멀티는 세션 생성이 비동기라 실패하면 메인 메뉴가 팝업을 띄움
	bLoading = true;
	SetStatus(FText::FromString(TEXT("불러오는 중...")));
	Close();
}

void URunSaveListWidget::HandleDelete(const FString& SlotName)
{
	PendingDeleteSlot = SlotName;

	const TSubclassOf<UConfirmPopupWidget> Class = PopupClass ? PopupClass : TSubclassOf<UConfirmPopupWidget>(UConfirmPopupWidget::StaticClass());
	UConfirmPopupWidget* Popup = CreateWidget<UConfirmPopupWidget>(GetOwningPlayer(), Class);
	if (!Popup)
	{
		HandleDeleteConfirmed();
		return;
	}

	Popup->Setup(FText::FromString(TEXT("세이브 삭제")),
		FText::FromString(TEXT("이 진행을 삭제할까요? 되돌릴 수 없습니다.")),
		FText::FromString(TEXT("삭제")), FText::FromString(TEXT("취소")));
	Popup->OnConfirmedNative.BindUObject(this, &URunSaveListWidget::HandleDeleteConfirmed);
	Popup->AddToViewport(50);
}

void URunSaveListWidget::HandleDeleteConfirmed()
{
	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		Save->DeleteRunSave(PendingDeleteSlot);
	}
	PendingDeleteSlot.Reset();
	Refresh();
}
