#include "Widgets/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Online/SessionSubsystem.h"
#include "Widgets/TerminusUIColors.h"
#include "Widgets/RoomListWidget.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Widgets/Storage/StorageWidget.h"
#include "Widgets/Common/EscapeStackSubsystem.h"
#include "Widgets/Save/RunSaveListWidget.h"
#include "Game/TerminusSaveSubsystem.h"
#include "Game/TerminusRunSubsystem.h"
#include "Widgets/Base/BaseWidget.h"
#include "Game/LoadingScreenSubsystem.h"
#include "Widgets/Common/ConfirmPopupWidget.h"

namespace
{
	const FLinearColor TextGold     = TerminusUI::Hex(TEXT("E6C47A"));
	const FLinearColor TextDim      = TerminusUI::Hex(TEXT("8D8778"));   // 게임 종료 기본색
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// NativeConstruct 는 여러 번 불릴 수 있어서 버튼 바인딩은 여기서 한 번만 (주점과 같은 규칙)
	Btn_Dungeon->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleDungeonClicked);
	Btn_Tavern->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleTavernClicked);
	Btn_Quit->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
	Btn_Quit->OnHovered.AddDynamic(this, &UMainMenuWidget::HandleQuitHovered);
	Btn_Quit->OnUnhovered.AddDynamic(this, &UMainMenuWidget::HandleQuitUnhovered);
	Btn_PopupOK->OnClicked.AddDynamic(this, &UMainMenuWidget::HandlePopupOKClicked);
	if (Btn_Storage) { Btn_Storage->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStorageClicked); }
	if (Btn_Continue) { Btn_Continue->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleContinueClicked); }

	if (Btn_Base) { Btn_Base->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBaseClicked); }
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	DisconnectPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (RoomBrowser) { RoomBrowser->Close(); }

	// 메뉴로 돌아왔으면 이어하기 대기는 버림 (안 그러면 다음 주점이 이어하기로 열림)
	ClearPendingContinue();

	// 메인 화면 준비 완료 -> 로딩 화면 걷기
	if (ULoadingScreenSubsystem* Loading = ULoadingScreenSubsystem::Get(this))
	{
		Loading->Hide();
	}

	// 이어하기 버튼: 세이브가 있을 때만. 목록에서 다 지우면 바로 숨김
	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		Save->OnRunSavesChanged.Remove(RunSavesChangedHandle);
		RunSavesChangedHandle = Save->OnRunSavesChanged.AddUObject(this, &UMainMenuWidget::RefreshContinueButton);
	}
	RefreshContinueButton();
	SetButtonTextColor(Btn_Quit, TextDim);

	// 골드 표시. 프로필은 위젯보다 오래 살아서 Construct 마다 붙이고 Destruct 에서 뗀다
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnGoldChanged.AddUniqueDynamic(this, &UMainMenuWidget::HandleGoldChanged);
	}
	RefreshGold();

	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	// 서브시스템은 위젯보다 오래 산다 -> Construct 마다 붙이고 Destruct 에서 뗀다
	Sessions->OnHostComplete.AddUniqueDynamic(this, &UMainMenuWidget::HandleHostComplete);
	Sessions->OnJoinComplete.AddUniqueDynamic(this, &UMainMenuWidget::HandleJoinComplete);

	// 세이브가 조작 / 손상돼서 복구했거나 새로 시작했으면 알림 (한 번만)
	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		const FText Notice = Profile->ConsumeLoadNotice();
		if (!Notice.IsEmpty())
		{
			ShowPopup(FText::FromString(TEXT("세이브 파일 문제")), Notice);
		}
	}

	// 끊겨서 돌아온 거면 이유를 보여준다. 꺼내면 비워지므로 한 번만 뜬다
	const FText Reason = Sessions->ConsumeDisconnectReason();
	if (!Reason.IsEmpty())
	{
		ShowPopup(Reason, FText::FromString(TEXT("메인 화면으로 돌아왔습니다. 진행 중이던 던전은 주점 목록의 '재합류 대기' 방으로 돌아갈 수 있습니다.")));
	}
}

void UMainMenuWidget::NativeDestruct()
{
	if (USessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostComplete.RemoveDynamic(this, &UMainMenuWidget::HandleHostComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UMainMenuWidget::HandleJoinComplete);
	}

	if (UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr)
	{
		Profile->OnGoldChanged.RemoveDynamic(this, &UMainMenuWidget::HandleGoldChanged);
	}

	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		Save->OnRunSavesChanged.Remove(RunSavesChangedHandle);
	}

	Super::NativeDestruct();
}

void UMainMenuWidget::HandleDungeonClicked()
{
	// 싱글은 세션 없이 주점 레벨을 연다 -> NM_Standalone.
	// 주점을 거쳐야 ServerTravel(CopyProperties)로 RunState 가 던전까지 간다
	ClearPendingContinue();

	// 던전 입장은 늘 새 게임. 싱글 세이브는 하나뿐이라, 진행 중인 게 있으면 지워도 되는지 먼저 물음 (이어하기는 '이어하기' 목록에서)
	UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this);
	FRunSaveSummary Single;
	if (Save && Save->FindSingleRunSave(Single))
	{
		UConfirmPopupWidget* Popup = CreateWidget<UConfirmPopupWidget>(GetOwningPlayer(), UConfirmPopupWidget::StaticClass());
		if (Popup)
		{
			Popup->Setup(FText::FromString(TEXT("새 던전")),
				FText::FromString(FString::Printf(TEXT("진행 중인 싱글 던전이 있습니다. (%s)\n새로 시작하면 기존 진행 상황이 삭제됩니다.\n이어서 하려면 '이어하기'에서 고르세요."),
					*UTerminusSaveSubsystem::DescribeTitle(Single).ToString())),
				FText::FromString(TEXT("삭제하고 시작")), FText::FromString(TEXT("취소")));
			Popup->OnConfirmedNative.BindUObject(this, &UMainMenuWidget::HandleOverwriteSingleConfirmed);
			Popup->AddToViewport(50);
			return;
		}
	}

	StartNewSolo();
}

void UMainMenuWidget::HandleOverwriteSingleConfirmed()
{
	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		Save->DeleteSingleRunSaves();
	}
	StartNewSolo();
}

void UMainMenuWidget::StartNewSolo()
{
	// 싱글은 이름을 묻지 않음 (내부용 랜덤 이름)
	if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
	{
		Run->SetRoomName(UTerminusRunSubsystem::MakeRandomRoomName());
	}
	if (ULoadingScreenSubsystem* Loading = ULoadingScreenSubsystem::Get(this))
	{
		Loading->Show(FText::FromString(TEXT("캐릭터 선택으로 가는 중...")));
	}
	UGameplayStatics::OpenLevel(this, FName(*TavernMapPath));
}

void UMainMenuWidget::HandleTavernClicked()
{
	ClearPendingContinue();

	if (RoomBrowser)
	{
		RoomBrowser->Open(MaxPartySize, TavernMapPath);
		return;
	}
	
	USessionSubsystem* Sessions = GetSessions();
	if (!Sessions) { return; }

	// 세션 생성은 비동기라 기다리는 동안 연타를 막는다
	SetMenuEnabled(false);
	Sessions->HostSession(MaxPartySize, TavernMapPath, FTerminusRoomOptions());
}

void UMainMenuWidget::HandleHostComplete(bool bWasSuccessful)
{
	// 성공이면 서브시스템이 주점으로 ServerTravel 한다. 이 위젯은 곧 사라짐
	if (bWasSuccessful) { return; }

	ClearPendingContinue();
	SetMenuEnabled(true);
	ShowPopup(FText::FromString(TEXT("주점을 열지 못했습니다.")),
		FText::FromString(TEXT("Steam이 켜져 있는지 확인한 뒤 다시 시도하세요.")));
}

void UMainMenuWidget::HandleJoinComplete(bool bWasSuccessful)
{
	if (bWasSuccessful) { return; }


	// 주점 목록이 열려 있으면 목록이 상태 줄에 사유를 띄운다. 여기선 초대로 들어가다 실패한 경우만
	if (RoomBrowser && RoomBrowser->IsVisible()) { return; }

	const USessionSubsystem* Sessions = GetSessions();
	const FText Reason = Sessions ? Sessions->GetLastJoinError() : FText::GetEmpty();

	ShowPopup(FText::FromString(TEXT("주점에 들어가지 못했습니다.")),
		Reason.IsEmpty() ? FText::FromString(TEXT("잠시 후 다시 시도하세요.")) : Reason);
}

void UMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::HandlePopupOKClicked()
{
	DisconnectPopup->SetVisibility(ESlateVisibility::Collapsed);

	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Remove(DisconnectPopup);
	}
}

void UMainMenuWidget::HandleStorageClicked()
{
	// WBP 안에 둔 창이 없으면 처음 열 때 만들어서 메뉴 위에 띄움
	if (!StorageWindow)
	{
		const TSubclassOf<UStorageWidget> Class = StorageWidgetClass ? StorageWidgetClass : TSubclassOf<UStorageWidget>(UStorageWidget::StaticClass());
		StorageWindow = CreateWidget<UStorageWidget>(GetOwningPlayer(), Class);
		if (StorageWindow)
		{
			StorageWindow->AddToViewport(40);
		}
	}

	if (StorageWindow)
	{
		StorageWindow->Open();
	}
}

void UMainMenuWidget::HandleContinueClicked()
{
	// 창고와 같은 방식. WBP 안에 둔 창이 없으면 처음 열 때 만들어서 메뉴 위에 띄움
	if (!SaveListWindow)
	{
		const TSubclassOf<URunSaveListWidget> Class = SaveListWidgetClass ? SaveListWidgetClass : TSubclassOf<URunSaveListWidget>(URunSaveListWidget::StaticClass());
		SaveListWindow = CreateWidget<URunSaveListWidget>(GetOwningPlayer(), Class);
		if (SaveListWindow)
		{
			SaveListWindow->AddToViewport(40);
		}
	}

	if (SaveListWindow)
	{
		SaveListWindow->Open(TavernMapPath);
	}
}

void UMainMenuWidget::RefreshContinueButton()
{
	if (!Btn_Continue) return;

	UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this);
	const bool bHasSaves = Save && Save->HasRunSaves();
	Btn_Continue->SetVisibility(bHasSaves ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// 마지막 세이브를 지웠으면 열려 있던 목록도 닫음
	if (!bHasSaves && SaveListWindow)
	{
		SaveListWindow->Close();
	}
}

void UMainMenuWidget::ClearPendingContinue()
{
	if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(this))
	{
		Save->ClearPendingLoad();
	}
}

void UMainMenuWidget::HandleBaseClicked()
{
	// 창고와 같은 방식. WBP 안에 둔 창이 없으면 처음 열 때 만들어서 메뉴 위에 띄움
	if (!BaseWindow)
	{
		const TSubclassOf<UBaseWidget> Class = BaseWidgetClass ? BaseWidgetClass : TSubclassOf<UBaseWidget>(UBaseWidget::StaticClass());
		BaseWindow = CreateWidget<UBaseWidget>(GetOwningPlayer(), Class);
		if (BaseWindow)
		{
			BaseWindow->AddToViewport(40);
		}
	}

	if (BaseWindow)
	{
		BaseWindow->Open();
	}
}

void UMainMenuWidget::HandleGoldChanged(int32 NewGold, int32 Delta)
{
	RefreshGold();
}

void UMainMenuWidget::RefreshGold()
{
	if (!GoldText) return;

	const UTerminusProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
	FFormatNamedArguments Args;
	Args.Add(TEXT("Gold"), FText::AsNumber(Profile ? Profile->GetGold() : 0));
	GoldText->SetText(FText::Format(GoldFormat, Args));
}

void UMainMenuWidget::HandleQuitHovered()   { SetButtonTextColor(Btn_Quit, TextGold); }
void UMainMenuWidget::HandleQuitUnhovered() { SetButtonTextColor(Btn_Quit, TextDim); }

void UMainMenuWidget::ShowPopup(const FText& Title, const FText& Body)
{
	if (PopupTitle) { PopupTitle->SetText(Title); }
	PopupBody->SetText(Body);

	// 팝업 뒤를 덮는 검은 이미지가 클릭을 막아준다
	DisconnectPopup->SetVisibility(ESlateVisibility::Visible);

	// ESC = 확인
	if (UEscapeStackSubsystem* Escape = UEscapeStackSubsystem::Get(this))
	{
		Escape->Push(DisconnectPopup, FSimpleDelegate::CreateUObject(this, &UMainMenuWidget::HandlePopupOKClicked));
	}
}

void UMainMenuWidget::SetMenuEnabled(bool bEnabled)
{
	Btn_Dungeon->SetIsEnabled(bEnabled);
	Btn_Tavern->SetIsEnabled(bEnabled);
}

void UMainMenuWidget::SetButtonTextColor(UButton* Button, const FLinearColor& Color)
{
	// 버튼 바로 아래 TextBlock 하나만 있는 구조라 GetContent 로 바로 꺼낸다
	if (UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr)
	{
		Label->SetColorAndOpacity(FSlateColor(Color));
	}
}

USessionSubsystem* UMainMenuWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USessionSubsystem>() : nullptr;
}