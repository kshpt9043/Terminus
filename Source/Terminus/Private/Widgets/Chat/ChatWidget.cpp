#include "Widgets/Chat/ChatWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/ChatSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Widgets/Common/EscapeStackSubsystem.h"

// =====================================================================
// 초기화 / 배치
// =====================================================================

void UChatWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (InputBox)
	{
		InputBox->OnTextCommitted.AddDynamic(this, &UChatWidget::HandleTextCommitted);
		InputBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 채팅창 영역은 클릭을 받음 (드래그). 크기를 정해 붙이므로 그 바깥은 아래 화면이 그대로 눌림
	SetVisibility(ESlateVisibility::Visible);
}

void UChatWidget::BuildDefaultLayout()
{
	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetPadding(FMargin(0.f));
	WidgetTree->RootWidget = Background;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Column);

	// 제목줄 (여기를 잡고 끔)
	UBorder* Header = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragHandle"));
	Header->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Header->SetPadding(FMargin(8.f, 3.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(FText::FromString(TEXT("채팅  (Enter)")));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 11;
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	Header->SetContent(Title);
	Column->AddChildToVerticalBox(Header);
	DragHandle = Header;

	MessageList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MessageList"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageList))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(8.f, 4.f));
	}

	InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("InputBox"));
	InputBox->SetHintText(FText::FromString(TEXT("메시지 입력 (Enter 전송, ESC 닫기)")));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(InputBox))
	{
		S->SetPadding(FMargin(4.f));
	}
}

void UChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면에 붙일 크기 / 위치 (뷰포트에 직접 붙은 위젯일 때)
	SetDesiredSizeInViewport(ChatSize);
	SetAlignmentInViewport(FVector2D::ZeroVector);

	UChatSubsystem* Chat = UChatSubsystem::Get(this);
	if (Chat && Chat->SavedPosition.IsSet())
	{
		MoveTo(Chat->SavedPosition.GetValue());
	}
	else
	{
		const FVector2D Viewport = GetViewportSizeInSlateUnits();
		SetPositionInViewport(Viewport * DefaultPositionRatio, false);
	}

	// 지난 대화 (레벨 이동 전 것 포함) 다시 그리고, 새 메시지 받기
	if (MessageList)
	{
		MessageList->ClearChildren();
	}
	if (Chat)
	{
		for (const FChatMessage& Message : Chat->GetHistory())
		{
			AddLine(Message);
		}
		MessageAddedHandle = Chat->OnMessageAdded.AddUObject(this, &UChatWidget::HandleMessageAdded);
		Chat->SetActiveWidget(this);
	}
	if (MessageList)
	{
		MessageList->ScrollToEnd();
	}

	ApplyBackground();
}

void UChatWidget::NativeDestruct()
{
	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		Chat->OnMessageAdded.Remove(MessageAddedHandle);
		Chat->ClearActiveWidget(this);
	}
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

// =====================================================================
// 메시지
// =====================================================================

void UChatWidget::HandleMessageAdded(const FChatMessage& Message)
{
	// 내가 보낸 건 항상 맨 아래로. 남이 보낸 건 맨 아래를 보고 있을 때만 따라 내려감
	// (위로 올려 지난 대화를 읽는 중이면 화면이 튀지 않게 그대로)
	const bool bWasAtEnd = !MessageList || MessageList->GetScrollOffset() >= MessageList->GetScrollOffsetOfEnd() - 1.f;

	AddLine(Message);

	if (MessageList && (bWasAtEnd || IsMyMessage(Message)))
	{
		MessageList->ScrollToEnd();
	}
}

bool UChatWidget::IsMyMessage(const FChatMessage& Message) const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC && PC->PlayerState && Message.Kind == EChatMessageKind::Player
		&& Message.Sender == PC->PlayerState->GetPlayerName();
}

void UChatWidget::AddLine(const FChatMessage& Message)
{
	if (!MessageList) return;

	// 너무 많이 쌓이면 오래된 줄부터 지움 (기록 보관 수와 맞춤)
	while (MessageList->GetChildrenCount() >= UChatSubsystem::MaxHistory)
	{
		MessageList->RemoveChildAt(0);
	}

	const bool bMine = IsMyMessage(Message);

	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Line->GetFont();
	Font.Size = FontSize;
	Line->SetFont(Font);
	Line->SetAutoWrapText(true);
	Line->SetShadowOffset(FVector2D(1.f, 1.f));
	Line->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));

	if (Message.Kind == EChatMessageKind::System)
	{
		Line->SetText(FText::FromString(Message.Text));
		Line->SetColorAndOpacity(FSlateColor(SystemMessageColor));
	}
	else
	{
		Line->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"), *Message.Sender, *Message.Text)));
		Line->SetColorAndOpacity(FSlateColor(bMine ? MyMessageColor : OtherMessageColor));
	}

	MessageList->AddChild(Line);
}

// =====================================================================
// 입력
// =====================================================================

void UChatWidget::OpenInput()
{
	if (bInputOpen || !InputBox) return;
	bInputOpen = true;

	InputBox->SetText(FText::GetEmpty());
	InputBox->SetVisibility(ESlateVisibility::Visible);

	// 최신 대화를 보면서 입력하게
	if (MessageList)
	{
		MessageList->ScrollToEnd();
	}
	InputBox->SetUserFocus(GetOwningPlayer());
	InputBox->SetKeyboardFocus();

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &UChatWidget::CloseInput));
	}

	ApplyBackground();
}

void UChatWidget::CloseInput()
{
	if (!bInputOpen) return;
	bInputOpen = false;

	if (InputBox)
	{
		InputBox->SetText(FText::GetEmpty());
		InputBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	// 키보드 포커스를 게임 화면으로 돌려줌 (입력창이 숨었는데 포커스를 쥐고 있지 않게)
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	ApplyBackground();
}

void UChatWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter) return;   // 포커스를 잃은 건 무시 (ESC / 다른 곳 클릭)

	FString Message = Text.ToString().TrimStartAndEnd();
	if (!Message.IsEmpty())
	{
		Message.LeftInline(MaxMessageLength);
		if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
		{
			PC->Server_SendChat(Message);
		}
	}

	CloseInput();
}

void UChatWidget::ApplyBackground()
{
	if (Background)
	{
		Background->SetBrushColor(bInputOpen ? ActiveBackgroundColor : IdleBackgroundColor);
	}
}

// =====================================================================
// 드래그
// =====================================================================

FReply UChatWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DragHandle
		&& DragHandle->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDragging = true;
		DragOffset = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// 채팅창 안 클릭은 아래 화면(지도 등)으로 새지 않게
	return FReply::Handled();
}

FReply UChatWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);

	FVector2D PixelPosition, ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, InMouseEvent.GetScreenSpacePosition(), PixelPosition, ViewportPosition);
	MoveTo(ViewportPosition - DragOffset);
	return FReply::Handled();
}

FReply UChatWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UChatWidget::MoveTo(FVector2D Position)
{
	// 제목줄이 화면 밖으로 나가 다시 못 잡는 일이 없게 화면 안으로
	const FVector2D Viewport = GetViewportSizeInSlateUnits();
	if (Viewport.X > 0.f && Viewport.Y > 0.f)
	{
		Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, Viewport.X - ChatSize.X));
		Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, Viewport.Y - ChatSize.Y));
	}

	SetPositionInViewport(Position, false);

	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		Chat->SavedPosition = Position;
	}
}

FVector2D UChatWidget::GetViewportSizeInSlateUnits() const
{
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	return Scale > 0.f ? UWidgetLayoutLibrary::GetViewportSize(this) / Scale : FVector2D::ZeroVector;
}
