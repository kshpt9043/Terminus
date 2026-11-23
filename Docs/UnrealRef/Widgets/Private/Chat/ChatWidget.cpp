#include "Widgets/Chat/ChatWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Comms/PartyCommsTypes.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/ChatSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/Common/TerminusUIBuilder.h"
#include "Widgets/TerminusUIColors.h"

namespace
{
	const TCHAR* PingPrefix = TEXT("[핑] ");

	// 핑 안내 줄이면 핑 종류 (문구 끝으로 판단). 아니면 false
	bool FindPingKind(const FString& Text, EPingKind& OutKind)
	{
		if (!Text.StartsWith(PingPrefix)) return false;
		for (uint8 i = 0; i < static_cast<uint8>(EPingKind::MAX); ++i)
		{
			const EPingKind Kind = static_cast<EPingKind>(i);
			const FString PingText = PartyComms::GetPingText(Kind).ToString();
			if (!PingText.IsEmpty() && Text.EndsWith(PingText))
			{
				OutKind = Kind;
				return true;
			}
		}
		OutKind = EPingKind::Focus;
		return true;
	}

	// 퀵챗 문구와 같은 말인가 (말풍선 표시)
	bool IsQuickChatText(const FString& Text)
	{
		for (uint8 i = 0; i < static_cast<uint8>(EQuickChat::MAX); ++i)
		{
			if (PartyComms::GetQuickChatText(static_cast<EQuickChat>(i)).ToString() == Text) return true;
		}
		return false;
	}
}

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
		InputBox->OnTextChanged.AddDynamic(this, &UChatWidget::HandleTextChanged);
	}
	if (InputFrame) InputFrame->SetVisibility(ESlateVisibility::Collapsed);
	else if (InputBox) InputBox->SetVisibility(ESlateVisibility::Collapsed);

	// 제목줄(드래그)만 클릭을 받고, 대화 목록은 입력 중일 때만 (평소엔 아래 전투 대상 버튼이 눌리게). ApplyBackground 참고
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ApplyBackground();
}

void UChatWidget::BuildDefaultLayout()
{
	Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetPadding(FMargin(0.f));
	WidgetTree->RootWidget = Background;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Column);

	// 제목줄 22px: [잡는 점] 채팅  (여기를 잡고 끔)
	USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>();
	HeaderSize->SetHeightOverride(22.f);
	UBorder* Header = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragHandle"));
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	Header->SetBrush(NoBrush);
	Header->SetPadding(FMargin(10.f, 0.f));
	Header->SetVerticalAlignment(VAlign_Center);
	HeaderSize->SetContent(Header);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Header->SetContent(HeaderRow);
	UImage* Grip = WidgetTree->ConstructWidget<UImage>();
	if (UTexture2D* GripTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/Common/T_Chat_Grip.T_Chat_Grip")))
	{
		Grip->SetBrushFromTexture(GripTexture);
	}
	Grip->SetDesiredSizeOverride(FVector2D(8.f, 13.f));
	Grip->SetColorAndOpacity(OtherMessageColor);
	HeaderRow->AddChildToHorizontalBox(Grip)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Title = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT("채팅"), 12, OtherMessageColor, ETextJustify::Left);
	TerminusUI::SetLetterSpacing(Title, 80);
	if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Title))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
	Column->AddChildToVerticalBox(HeaderSize);
	DragHandle = Header;

	UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HeaderLine"));
	Line->SetDesiredSizeOverride(FVector2D(8.f, 1.f));
	Line->SetColorAndOpacity(TerminusUI::Hex(TEXT("ECE3CF"), 0.12f));
	Column->AddChildToVerticalBox(Line);
	HeaderLine = Line;

	MessageList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MessageList"));
	MessageList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MessageList))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(10.f, 4.f, 10.f, 8.f));
	}

	// 입력칸 30px: [입력 ............ 9/200 · Enter 보내기 · Esc 닫기]
	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InputFrame"));
	Frame->SetHeightOverride(30.f);
	UBorder* InputBorder = WidgetTree->ConstructWidget<UBorder>();
	InputBorder->SetBrush(TerminusUI::MakeBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f), TerminusUI::Hex(TEXT("ECE3CF"), 0.22f), 2.f));
	InputBorder->SetPadding(FMargin(10.f, 0.f));
	InputBorder->SetVerticalAlignment(VAlign_Center);
	Frame->SetContent(InputBorder);
	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	InputBorder->SetContent(InputRow);

	InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("InputBox"));
	if (UHorizontalBoxSlot* S = InputRow->AddChildToHorizontalBox(InputBox))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	CounterText = TerminusUI::MakeText(WidgetTree, NAME_None, TEXT(""), 11, TimeColor, ETextJustify::Right);
	if (UHorizontalBoxSlot* S = InputRow->AddChildToHorizontalBox(CounterText))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Frame))
	{
		S->SetPadding(FMargin(8.f, 0.f, 8.f, 8.f));
	}
	InputFrame = Frame;
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

	// 지난 대화 (레벨 이동 전 것 포함) 다시 그리고, 새 메시지 / 기록 교체 받기
	RebuildFromHistory();
	if (Chat)
	{
		MessageAddedHandle = Chat->OnMessageAdded.AddUObject(this, &UChatWidget::HandleMessageAdded);
		HistoryReplacedHandle = Chat->OnHistoryReplaced.AddUObject(this, &UChatWidget::RebuildFromHistory);
		Chat->SetActiveWidget(this);
	}

	ApplyBackground();
}

void UChatWidget::RebuildFromHistory()
{
	if (!MessageList) return;

	MessageList->ClearChildren();
	FlashLine.Reset();

	// 맨 위 빈칸: 줄이 적을 때 아래부터 쌓이게 (높이는 NativeTick 이 맞춤)
	TopSpacer = WidgetTree->ConstructWidget<USpacer>();
	TopSpacer->SetSize(FVector2D(1.f, 0.f));
	MessageList->AddChild(TopSpacer);
	if (const UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		for (const FChatMessage& Message : Chat->GetHistory())
		{
			AddLine(Message);
		}
	}
	ApplyLineStyles();
	MessageList->ScrollToEnd();
}

void UChatWidget::NativeDestruct()
{
	if (UChatSubsystem* Chat = UChatSubsystem::Get(this))
	{
		Chat->OnMessageAdded.Remove(MessageAddedHandle);
		Chat->OnHistoryReplaced.Remove(HistoryReplacedHandle);
		Chat->ClearActiveWidget(this);
	}
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	Super::NativeDestruct();
}

void UChatWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 제목줄에 마우스가 올라가면 진하게
	const bool bHover = DragHandle && DragHandle->IsHovered();
	if (bHover != bHeaderHovered)
	{
		bHeaderHovered = bHover;
		ApplyBackground();
	}

	// 줄이 적으면 위를 빈칸으로 채워 아래부터 쌓이게 (시안: 최근 줄이 맨 아래)
	if (MessageList && TopSpacer)
	{
		const float ViewHeight = MessageList->GetCachedGeometry().GetLocalSize().Y;
		float ContentHeight = 0.f;
		for (UWidget* Child : MessageList->GetAllChildren())
		{
			if (Child && Child != TopSpacer) ContentHeight += Child->GetDesiredSize().Y + 2.f;   // 줄 위아래 여백 1px
		}
		const float Wanted = FMath::Max(0.f, ViewHeight - ContentHeight - 1.f);
		if (!FMath::IsNearlyEqual(TopSpacer->GetSize().Y, Wanted, 0.5f))
		{
			TopSpacer->SetSize(FVector2D(1.f, Wanted));
		}
	}

	// 새 메시지 줄 금빛: 잠깐 켜졌다가 서서히 사라짐
	if (UBorder* Line = FlashLine.Get())
	{
		const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - FlashStartTime);
		const float Fade = Elapsed <= FlashHold ? 1.f : 1.f - FMath::Clamp((Elapsed - FlashHold) / FMath::Max(FlashFade, 0.01f), 0.f, 1.f);
		FLinearColor Color = FlashColor;
		Color.A *= Fade;
		Line->SetBrushColor(Color);
		if (Fade <= 0.f)
		{
			FlashLine.Reset();
		}
	}
}

// =====================================================================
// 메시지
// =====================================================================

void UChatWidget::HandleMessageAdded(const FChatMessage& Message)
{
	// 내가 보낸 건 항상 맨 아래로. 남이 보낸 건 맨 아래를 보고 있을 때만 따라 내려감
	// (위로 올려 지난 대화를 읽는 중이면 화면이 튀지 않게 그대로)
	const bool bWasAtEnd = !MessageList || MessageList->GetScrollOffset() >= MessageList->GetScrollOffsetOfEnd() - 1.f;

	// 이전에 깜빡이던 줄은 바로 끔
	if (UBorder* Old = FlashLine.Get())
	{
		Old->SetBrushColor(FLinearColor::Transparent);
	}

	if (UBorder* Line = AddLine(Message))
	{
		FlashLine = Line;
		FlashStartTime = FPlatformTime::Seconds();
	}
	ApplyLineStyles();

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

UBorder* UChatWidget::AddLine(const FChatMessage& Message)
{
	if (!MessageList) return nullptr;

	// 너무 많이 쌓이면 오래된 줄부터 지움 (기록 보관 수와 맞춤)
	const int32 FirstLine = TopSpacer ? 1 : 0;
	while (MessageList->GetChildrenCount() - FirstLine >= UChatSubsystem::MaxHistory)
	{
		MessageList->RemoveChildAt(FirstLine);
	}

	const bool bSystem = Message.Kind == EChatMessageKind::System;
	EPingKind PingKind = EPingKind::Focus;
	const bool bPing = bSystem && FindPingKind(Message.Text, PingKind);
	const bool bQuick = !bSystem && IsQuickChatText(Message.Text);

	// 줄 색: 내 말 금 / 안내 하늘 / 남의 말과 핑 본문색
	const FLinearColor LineColor = bPing ? OtherMessageColor : bSystem ? SystemMessageColor : IsMyMessage(Message) ? MyMessageColor : OtherMessageColor;

	// 줄 하나 = 금빛 깜빡임용 바탕 + [시간][태그][이름: ][내용]
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
	Line->SetBrush(TerminusUI::MakeBoxBrush(FLinearColor::White, FLinearColor::Transparent, 2.f, 0.f));
	Line->SetBrushColor(FLinearColor::Transparent);
	Line->SetPadding(FMargin(4.f, 0.f));
	Line->SetVisibility(ESlateVisibility::HitTestInvisible);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Line->SetContent(Row);

	auto AddText = [&](const FString& Text, int32 Size, const FLinearColor& Color, bool bBold, float RightPad, bool bFill = false)
	{
		UTextBlock* Block = TerminusUI::MakeText(WidgetTree, NAME_None, Text, Size, Color, ETextJustify::Left);
		TerminusUI::SetBold(Block, bBold);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Block))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, RightPad, 0.f));
			if (bFill)
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Block->SetClipping(EWidgetClipping::ClipToBounds);
				Block->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			}
		}
		return Block;
	};

	if (!Message.Time.IsEmpty())
	{
		AddText(FString::Printf(TEXT("[%s]"), *Message.Time), TimeFontSize, TimeColor, false, 6.f);
	}

	if (bPing)
	{
		AddText(TEXT("[핑]"), FontSize, PartyComms::GetPingColor(PingKind), true, 5.f);
		AddText(Message.Text.RightChop(FCString::Strlen(PingPrefix)), FontSize, LineColor, false, 0.f, true);
	}
	else if (bSystem)
	{
		AddText(TEXT("· ") + Message.Text, FontSize, LineColor, false, 0.f, true);
	}
	else
	{
		// 퀵챗은 이름 앞에 작은 말풍선
		if (bQuick)
		{
			UImage* Bubble = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush BubbleBrush = TerminusUI::MakeBoxBrush(FLinearColor::Transparent, LineColor, 3.f, 1.5f);
			BubbleBrush.ImageSize = FVector2D(11.f, 8.f);
			Bubble->SetBrush(BubbleBrush);
			Bubble->SetRenderOpacity(0.8f);
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Bubble))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 1.f, 5.f, 0.f));
			}
		}
		AddText(Message.Sender, FontSize, LineColor, true, 0.f);
		AddText(TEXT(": ") + Message.Text, FontSize, LineColor, false, 0.f, true);
	}

	if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(MessageList->AddChild(Line)))
	{
		S->SetPadding(FMargin(-4.f, 1.f, -4.f, 1.f));
	}
	return Line;
}

void UChatWidget::ApplyLineStyles()
{
	if (!MessageList) return;

	// 평소엔 아래 줄일수록 또렷 (한 줄 위로 갈 때마다 12% 흐리게, 최소 25%) + 그림자. 입력 중엔 모두 또렷, 그림자 없음
	const int32 Count = MessageList->GetChildrenCount();
	for (int32 i = 0; i < Count; ++i)
	{
		UWidget* Line = MessageList->GetChildAt(i);
		if (!Line || Line == TopSpacer) continue;

		const int32 Age = Count - 1 - i;
		Line->SetRenderOpacity(bInputOpen ? 1.f : FMath::Max(0.25f, 1.f - Age * 0.12f));

		const UBorder* LineBorder = Cast<UBorder>(Line);
		const UHorizontalBox* Row = LineBorder ? Cast<UHorizontalBox>(LineBorder->GetContent()) : nullptr;
		if (!Row) continue;
		for (UWidget* Child : Row->GetAllChildren())
		{
			if (UTextBlock* Block = Cast<UTextBlock>(Child))
			{
				Block->SetShadowOffset(FVector2D(0.f, 1.f));
				Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, bInputOpen ? 0.f : 0.95f));
			}
		}
	}
}

// =====================================================================
// 입력
// =====================================================================

void UChatWidget::OpenInput()
{
	if (bInputOpen || !InputBox) return;
	bInputOpen = true;

	InputBox->SetText(FText::GetEmpty());
	if (InputFrame) InputFrame->SetVisibility(ESlateVisibility::Visible);
	InputBox->SetVisibility(ESlateVisibility::Visible);
	UpdateCounter();

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
		if (!InputFrame) InputBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InputFrame) InputFrame->SetVisibility(ESlateVisibility::Collapsed);

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

void UChatWidget::HandleTextChanged(const FText& Text)
{
	// 글자 수 제한을 넘으면 잘라서 다시 넣음
	if (InputBox && Text.ToString().Len() > MaxMessageLength)
	{
		InputBox->SetText(FText::FromString(Text.ToString().Left(MaxMessageLength)));
	}
	UpdateCounter();
}

void UChatWidget::UpdateCounter()
{
	const int32 Length = InputBox ? InputBox->GetText().ToString().Len() : 0;
	TerminusUI::SetTextIfChanged(CounterText, FString::Printf(TEXT("%d/%d · Enter 보내기 · Esc 닫기"), Length, MaxMessageLength));
}

void UChatWidget::ApplyBackground()
{
	if (Background)
	{
		// 둥근 판 (3px). 상태마다 바탕 / 테두리
		FSlateBrush Brush = Background->Background;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(bInputOpen ? ActiveBackgroundColor : IdleBackgroundColor);
		Brush.OutlineSettings.CornerRadii = FVector4(3.f, 3.f, 3.f, 3.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 1.f;
		Brush.OutlineSettings.Color = FSlateColor(bInputOpen ? ActiveOutlineColor : IdleOutlineColor);
		Background->SetBrush(Brush);
		Background->SetBrushColor(FLinearColor::White);
		Background->SetVisibility(bInputOpen ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
	}

	// 제목줄: 평소 흐림, 마우스 / 입력 중 진함 + 구분선
	if (DragHandle)
	{
		DragHandle->SetRenderOpacity(bInputOpen ? HeaderOpacity.Z : bHeaderHovered ? HeaderOpacity.Y : HeaderOpacity.X);
	}
	if (HeaderLine)
	{
		HeaderLine->SetVisibility(bInputOpen || bHeaderHovered ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 입력 중일 때만 대화 목록이 클릭 / 스크롤을 받음
	if (MessageList)
	{
		MessageList->SetVisibility(bInputOpen ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	}

	ApplyLineStyles();
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
