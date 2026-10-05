#include "Widgets/RoomListWidget.h"

#include "Widgets/RoomEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Engine/GameInstance.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Game/TerminusRunSubsystem.h"

void URoomListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼은 위젯 안의 부품이라 한 번만 바인딩
	Btn_Refresh->OnClicked.AddDynamic(this, &URoomListWidget::HandleRefreshClicked);
	Btn_OpenCreate->OnClicked.AddDynamic(this, &URoomListWidget::HandleOpenCreateClicked);
	Btn_Close->OnClicked.AddDynamic(this, &URoomListWidget::HandleCloseClicked);
	Btn_CreateConfirm->OnClicked.AddDynamic(this, &URoomListWidget::HandleCreateConfirmClicked);
	Btn_CreateCancel->OnClicked.AddDynamic(this, &URoomListWidget::HandleCreateCancelClicked);
	Btn_JoinConfirm->OnClicked.AddDynamic(this, &URoomListWidget::HandleJoinConfirmClicked);
	Btn_JoinCancel->OnClicked.AddDynamic(this, &URoomListWidget::HandleJoinCancelClicked);

	// 기본 스타일 글자가 회색이라 안 보임 -> 세 칸을 한 번에 칠한다
	PaintInputText(RoomNameInput);
	PaintInputText(PasswordInput);
	PaintInputText(JoinPasswordInput);
}

void URoomListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 서브시스템은 위젯보다 오래 산다 -> Construct 에서 붙이고 Destruct 에서 뗀다 (메인 메뉴와 같은 규칙)
	if (USessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnFindComplete.AddUniqueDynamic(this, &URoomListWidget::HandleFindComplete);
		Sessions->OnJoinComplete.AddUniqueDynamic(this, &URoomListWidget::HandleJoinComplete);
		Sessions->OnHostComplete.AddUniqueDynamic(this, &URoomListWidget::HandleHostComplete);
	}

	ShowPanel(CreatePanel, false);
	ShowPanel(PasswordPanel, false);
}

void URoomListWidget::NativeDestruct()
{
	if (USessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnFindComplete.RemoveDynamic(this, &URoomListWidget::HandleFindComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &URoomListWidget::HandleJoinComplete);
		Sessions->OnHostComplete.RemoveDynamic(this, &URoomListWidget::HandleHostComplete);
	}

	Super::NativeDestruct();
}

// ---------- 열고 닫기

void URoomListWidget::Open(int32 InMaxPlayers, const FString& InMapPath)
{
	MaxPlayers = InMaxPlayers;
	MapPath = InMapPath;

	SetBusy(false);
	SetVisibility(ESlateVisibility::Visible);

	// ESC: 안쪽 창(비밀번호 / 주점 열기)부터 닫고, 없으면 리스트를 닫음
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(this, FSimpleDelegate::CreateUObject(this, &URoomListWidget::HandleEscape));
	}

	// 열자마자 한 번 찾는다
	HandleRefreshClicked();
}

void URoomListWidget::HandleEscape()
{
	// 참가 / 주점 열기를 기다리는 중이면 닫지 않음 (닫기 버튼도 막혀 있는 상태)
	if (!Btn_Close->GetIsEnabled()) return;

	if (PasswordPanel->IsVisible())
	{
		HandleJoinCancelClicked();
	}
	else if (CreatePanel->IsVisible())
	{
		HandleCreateCancelClicked();
	}
	else
	{
		Close();
	}
}

void URoomListWidget::Close()
{
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(this);
	}

	ShowPanel(CreatePanel, false);
	ShowPanel(PasswordPanel, false);
	PendingJoinIndex = INDEX_NONE;
	SetVisibility(ESlateVisibility::Collapsed);
}

void URoomListWidget::HandleCloseClicked()
{
	Close();
}

// ---------- 목록

void URoomListWidget::HandleRefreshClicked()
{
	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	ListBox->ClearChildren();
	SetStatus(TEXT("주점을 찾는 중…"));

	// 검색은 비동기. 끝날 때까지 다시 못 누르게
	Btn_Refresh->SetIsEnabled(false);
	Sessions->FindSessions();
}

void URoomListWidget::HandleFindComplete(bool bWasSuccessful, const TArray<FTerminusSessionInfo>& Sessions)
{
	Btn_Refresh->SetIsEnabled(true);
	ListBox->ClearChildren();

	if (!bWasSuccessful)
	{
		SetStatus(TEXT("주점 목록을 불러오지 못했습니다."));
		return;
	}
	if (Sessions.Num() == 0)
	{
		SetStatus(TEXT("열린 주점이 없습니다. 직접 열어 보세요."));
		return;
	}
	if (!EntryClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoomList: EntryClass 가 비어 있음. BP 디폴트에서 WBP_RoomEntry 를 지정할 것"));
		return;
	}

	SetStatus(FString());
	for (const FTerminusSessionInfo& Info : Sessions)
	{
		URoomEntryWidget* Entry = CreateWidget<URoomEntryWidget>(this, EntryClass);
		if (!Entry) { continue; }

		Entry->Setup(Info);
		Entry->OnClicked.BindUObject(this, &URoomListWidget::HandleEntryClicked);
		ListBox->AddChild(Entry);
	}
}

void URoomListWidget::HandleEntryClicked(const FTerminusSessionInfo& Info)
{
	if (!Info.bLocked)
	{
		Join(Info.Index, FString());
		return;
	}

	// 잠긴 방이면 비밀번호부터
	PendingJoinIndex = Info.Index;
	JoinPasswordInput->SetText(FText::GetEmpty());
	if (JoinErrorText) { JoinErrorText->SetText(FText::GetEmpty()); }
	ShowPanel(PasswordPanel, true);
}

// ---------- 비밀번호 입력

void URoomListWidget::HandleJoinConfirmClicked()
{
	const FString Password = JoinPasswordInput->GetText().ToString();

	// 형식이 틀린 건 호스트까지 보낼 것도 없이 여기서 거른다. 맞고 틀림은 호스트가 판정
	if (!USessionSubsystem::IsValidRoomPassword(Password))
	{
		if (JoinErrorText) { JoinErrorText->SetText(FText::FromString(TEXT("영문과 숫자 1~16자로 입력하세요."))); }
		return;
	}

	ShowPanel(PasswordPanel, false);
	Join(PendingJoinIndex, Password);
	PendingJoinIndex = INDEX_NONE;
}

void URoomListWidget::HandleJoinCancelClicked()
{
	ShowPanel(PasswordPanel, false);
	PendingJoinIndex = INDEX_NONE;
}

void URoomListWidget::Join(int32 Index, const FString& Password)
{
	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	SetBusy(true);
	SetStatus(TEXT("주점에 들어가는 중…"));
	Sessions->JoinSessionByIndex(Index, Password);
}

void URoomListWidget::HandleJoinComplete(bool bWasSuccessful)
{
	// 성공이면 곧 주점으로 이동한다. 비밀번호가 틀리면 호스트가 거절 -> 메인이 다시 뜨며 팝업
	if (bWasSuccessful) { return; }

	// 실패 사유(가득 참, 응답 없음 등)는 서브시스템이 남겨 둔다
	const USessionSubsystem* Sessions = GetSessions();
	const FText Reason = Sessions ? Sessions->GetLastJoinError() : FText::GetEmpty();

	SetBusy(false);
	SetStatus(Reason.IsEmpty() ? FString(TEXT("주점에 들어가지 못했습니다. 목록을 새로고침해 보세요.")) : Reason.ToString());
}

// ---------- 주점 열기

void URoomListWidget::HandleOpenCreateClicked()
{
	// 기본은 랜덤 이름 (지우면 열 때 다시 랜덤)
	RoomNameInput->SetText(FText::FromString(UTerminusRunSubsystem::MakeRandomRoomName()));
	PasswordInput->SetText(FText::GetEmpty());
	if (PrivateCheck)    { PrivateCheck->SetIsChecked(false); }
	if (CreateErrorText) { CreateErrorText->SetText(FText::GetEmpty()); }
	ShowPanel(CreatePanel, true);
}

void URoomListWidget::HandleCreateCancelClicked()
{
	ShowPanel(CreatePanel, false);
}

void URoomListWidget::HandleCreateConfirmClicked()
{
	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	FTerminusRoomOptions Options;
	Options.RoomName = RoomNameInput->GetText().ToString().TrimStartAndEnd();
	Options.Password = PasswordInput->GetText().ToString();
	Options.bListed  = !(PrivateCheck && PrivateCheck->IsChecked());

	// 비밀번호는 선택. 넣었다면 규칙에 맞아야 한다
	if (!Options.Password.IsEmpty() && !USessionSubsystem::IsValidRoomPassword(Options.Password))
	{
		if (CreateErrorText) { CreateErrorText->SetText(FText::FromString(TEXT("비밀번호는 영문과 숫자 1~16자입니다."))); }
		return;
	}

	SetBusy(true);
	Btn_CreateConfirm->SetIsEnabled(false);
	Sessions->HostSession(MaxPlayers, MapPath, Options);
}

void URoomListWidget::HandleHostComplete(bool bWasSuccessful)
{
	// 성공이면 주점으로 이동. 실패 팝업은 메인 메뉴가 띄우니 여기서는 버튼만 되살린다
	if (bWasSuccessful) { return; }

	SetBusy(false);
	Btn_CreateConfirm->SetIsEnabled(true);
}

// ---------- 도우미

void URoomListWidget::SetStatus(const FString& InText)
{
	if (!StatusText) { return; }

	StatusText->SetText(FText::FromString(InText));
	StatusText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void URoomListWidget::SetBusy(bool bBusy)
{
	// 참가나 주점 열기를 기다리는 동안 다른 걸 누르지 못하게
	ListBox->SetIsEnabled(!bBusy);
	Btn_Refresh->SetIsEnabled(!bBusy);
	Btn_OpenCreate->SetIsEnabled(!bBusy);
	Btn_Close->SetIsEnabled(!bBusy);
}

void URoomListWidget::ShowPanel(UWidget* Panel, bool bShow)
{
	if (Panel)
	{
		Panel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void URoomListWidget::PaintInputText(UEditableTextBox* Box) const
{
	if (!Box) { return; }

	// 입력 중엔 FocusedForegroundColor 를 써서 SetForegroundColor 만으론 타이핑하는 동안 회색 그대로
	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetForegroundColor(InputTextColor);
	Style.SetFocusedForegroundColor(InputTextColor);
	Style.SetReadOnlyForegroundColor(InputTextColor);
	// WBP 에서 글자색을 직접 박아 두면 위 색이 안 먹으니 위 상태별 색을 따라가게
	Style.TextStyle.SetColorAndOpacity(FSlateColor::UseForeground());

	// SetWidgetStyle 은 넘긴 구조체의 주소를 Slate 에 그대로 넘긴다 -> 지역 변수면 함수가 끝나면 허공을 가리킴
	// 한 번 더 불러서 위젯 멤버(GetWidgetStyle)를 가리키게 해 둔다
	Box->SetWidgetStyle(Style);
	Box->SetWidgetStyle(Box->GetWidgetStyle());
}

USessionSubsystem* URoomListWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USessionSubsystem>() : nullptr;
}