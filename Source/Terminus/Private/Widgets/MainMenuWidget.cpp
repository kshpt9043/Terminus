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
#include "Widgets/Common/TextInputPopupWidget.h"
#include "Widgets/Base/BaseWidget.h"
#include "Game/LoadingScreenSubsystem.h"

namespace
{
	const FLinearColor TextGold     = TerminusUI::Hex(TEXT("E6C47A"));
	const FLinearColor TextDim      = TerminusUI::Hex(TEXT("8D8778"));   // 게임 종료 기본색
	const FLinearColor TextDisabled = TerminusUI::Hex(TEXT("5D5A52"));
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

	// 거점: Btn_Base, 없으면 예전 Btn_Tower 를 거점 버튼으로
	if (UButton* BaseButton = Btn_Base ? Btn_Base.Get() : Btn_Tower.Get())
	{
		BaseButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleBaseClicked);
	}
	if (Btn_Base && Btn_Tower)
	{
		Btn_Tower->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 훈련소는 거점 안으로 들어감 (시안: 메인 메뉴엔 거점 하나)
	if (Btn_Training)
	{
		Btn_Training->SetVisibility(ESlateVisibility::Collapsed);
	}
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

	// 싱글도 던전 이름을 정함 (세이브 목록에 이 이름으로). 기본은 랜덤
	const TSubclassOf<UTextInputPopupWidget> Class = NameInputPopupClass ? NameInputPopupClass : TSubclassOf<UTextInputPopupWidget>(UTextInputPopupWidget::StaticClass());
	UTextInputPopupWidget* Popup = CreateWidget<UTextInputPopupWidget>(GetOwningPlayer(), Class);
	if (!Popup)
	{
		StartSoloWithName(UTerminusRunSubsystem::MakeRandomRoomName());
		return;
	}

	Popup->Setup(FText::FromString(TEXT("던전 이름")),
		FText::FromString(TEXT("이번 탐험의 이름을 정하세요. 세이브 목록에 이 이름으로 남습니다.")),
		UTerminusRunSubsystem::MakeRandomRoomName(), UTerminusRunSubsystem::MaxRoomNameLength);
	Popup->SetRandomProvider([]() { return UTerminusRunSubsystem::MakeRandomRoomName(); });
	Popup->OnConfirmedText.BindUObject(this, &UMainMenuWidget::StartSoloWithName);
	Popup->AddToViewport(50);
}

void UMainMenuWidget::StartSoloWithName(const FString& RoomName)
{
	if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
	{
		Run->SetRoomName(RoomName);
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