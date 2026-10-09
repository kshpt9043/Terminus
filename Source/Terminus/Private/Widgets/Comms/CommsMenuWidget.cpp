#include "Widgets/Comms/CommsMenuWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Widgets/Common/EscapeStackSubsystem.h"

namespace
{
	// 유니티 빌드에서 다른 파일의 MakeText 와 겹치지 않게 이름을 따로
	UTextBlock* MakeCommsMenuText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}
}

void UCommsMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void UCommsMenuWidget::BuildLayout()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CommsMenuRoot"));
	WidgetTree->RootWidget = Root;

	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.06f, 0.05f, 0.05f, 0.92f));
	Panel->SetPadding(FMargin(10.f, 8.f));

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	TitleText = MakeCommsMenuText(WidgetTree, FText::GetEmpty(), FontSize - 2, FLinearColor(1.f, 0.85f, 0.45f));
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(TitleText))
	{
		S->SetPadding(FMargin(2.f, 0.f, 2.f, 6.f));
	}

	List = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->AddChildToVerticalBox(List);
	Panel->SetContent(Box);

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAutoSize(true);
	}

	// 바탕은 클릭 통과, 목록만 받음
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UCommsMenuWidget::Open(const FText& Title, const TArray<FText>& Options, const FVector2D& Position, FOnCommsMenuPicked InOnPicked)
{
	if (!List || !Panel) return;

	OnPicked = InOnPicked;

	TitleText->SetText(Title);
	TitleText->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	List->ClearChildren();
	Buttons.Reset();
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetBackgroundColor(FLinearColor(0.25f, 0.22f, 0.2f, 1.f));

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(ButtonWidth);
		Size->SetContent(MakeCommsMenuText(WidgetTree, FText::Format(INVTEXT("{0}. {1}"), FText::AsNumber(i + 1), Options[i]), FontSize, FLinearColor::White));
		Button->SetContent(Size);
		Button->OnClicked.AddDynamic(this, &UCommsMenuWidget::HandleButtonClicked);

		if (UVerticalBoxSlot* S = List->AddChildToVerticalBox(Button))
		{
			S->SetPadding(FMargin(0.f, i == 0 ? 0.f : 4.f, 0.f, 0.f));
		}
		Buttons.Add(Button);
	}

	// 화면 오른쪽 / 아래쪽 절반이면 마우스 반대쪽으로 펼침 (화면 밖으로 안 나가게)
	const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D Alignment(Position.X > Viewport.X * 0.5f ? 1.f : 0.f, Position.Y > Viewport.Y * 0.5f ? 1.f : 0.f);
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
	{
		PanelSlot->SetAlignment(Alignment);
		PanelSlot->SetPosition(Position + FVector2D(Alignment.X > 0.f ? -8.f : 8.f, Alignment.Y > 0.f ? -8.f : 8.f));
	}

	if (!IsInViewport())
	{
		AddToViewport(26);   // 채팅(25) 위, 팝업(50) 아래
	}
	bOpen = true;

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &UCommsMenuWidget::Close));
	}
}

void UCommsMenuWidget::Close()
{
	if (!bOpen) return;
	bOpen = false;

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}
	OnPicked.Unbind();
	RemoveFromParent();
}

void UCommsMenuWidget::NativeDestruct()
{
	// 레벨 이동 등으로 뷰포트에서 빠짐 -> ESC 목록에서도 빼기
	if (bOpen)
	{
		bOpen = false;
		if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
		{
			Escape->Remove(this);
		}
	}
	Super::NativeDestruct();
}

bool UCommsMenuWidget::PickByNumber(int32 Number)
{
	if (!bOpen || !Buttons.IsValidIndex(Number - 1)) return false;
	Pick(Number - 1);
	return true;
}

bool UCommsMenuWidget::IsOverPanel(const FVector2D& ScreenSpacePosition) const
{
	return bOpen && Panel && Panel->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition);
}

void UCommsMenuWidget::Pick(int32 Index)
{
	// 닫기(Close) 가 델리게이트를 풀기 때문에 먼저 꺼내 둠
	const FOnCommsMenuPicked Callback = OnPicked;
	Close();
	Callback.ExecuteIfBound(Index);
}

void UCommsMenuWidget::HandleButtonClicked()
{
	// 버튼 클릭 이벤트엔 누가 눌렸는지가 안 와서, 마우스가 올라가 있는 버튼으로 찾음 (전투 HUD 대상 버튼과 같은 방식)
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		if (Buttons[i] && Buttons[i]->IsHovered())
		{
			Pick(i);
			return;
		}
	}
}
