// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/TavernWidget.h"

#include "Widgets/PlayerSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/ClassButton.h"
#include "Data/CharacterTypes.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"
#include "Data/TerminusDataSettings.h"
#include "Online/SessionSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusUI, Log, All);

namespace
{
	// 1d 팔레트. FColor 는 sRGB 라 FLinearColor 로 바꾸면 에디터 Hex 칸과 같은 색
	FLinearColor Hex(const TCHAR* InHex) { return FLinearColor(FColor::FromHex(InHex)); }
}

void UTavernWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	// NativeConstruct 는 여러 번 불릴 수 있어서 바인딩은 여기서 한 번만
	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddDynamic(this, &UTavernWidget::HandleReadyClicked);
	}
	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &UTavernWidget::HandleStartClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddDynamic(this, &UTavernWidget::HandleLeaveClicked);
	}
	if (InviteButton)
	{
		InviteButton->OnClicked.AddDynamic(this, &UTavernWidget::HandleInviteClicked);
	}
	
	if (ReadyButton)
	{
		// WBP 에서 잡은 모양은 그대로 두고 색만 회색으로 바꾼 사본을 만든다
		ReadyStyle  = ReadyButton->GetStyle();
		CancelStyle = ReadyStyle;
		CancelStyle.Normal.TintColor  = FSlateColor(Hex(TEXT("3A3F4A")));
		CancelStyle.Hovered.TintColor = FSlateColor(Hex(TEXT("454B57")));
		CancelStyle.Pressed.TintColor = FSlateColor(Hex(TEXT("2E333D")));
	}
}

void UTavernWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 슬롯과 캐릭터 정보는 서로 무관
	CreateSlots();
	CreateClassButtons();
	ApplySelection(Selected);

	// 타이머 폴링을 돌기 전에 리턴해야함
	if (IsSolo())
	{
		ApplySoloLayout();
		return;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimer,
			this,
			&UTavernWidget::RefreshSlots,
			0.5f,     // 주기
			true,     // 반복
			0.f);     // 첫 호출은 바로
	}
}

void UTavernWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}

	Super::NativeDestruct();
}

void UTavernWidget::CreateClassButtons()
{
	if (!ClassButtonClass)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: ClassButtonClass 가 비어 있음. BP 디폴트에서 WBP_ClassButton를 지정할 것"));
		return;
	}
	
	ClassButtons.Reset();
	ClassButtonBox->ClearChildren();
	
	for (int32 i = 0; i < (int32)ECharacterClass::MAX; ++i)
	{
		// 다시 열거형으로 형변환
		const ECharacterClass ThisClass = (ECharacterClass)i;
		
		const FCharacterClassRow* Row = FindClassRow(ThisClass);
		if (!Row)
		{
			UE_LOG(LogTerminusUI, Warning, TEXT("TavernWidget: %s 행이 없어 버튼을 건너뜀"),
				*UEnum::GetValueAsString(ThisClass));
			continue;
		}

		UClassButton* Btn = CreateWidget<UClassButton>(this, ClassButtonClass);
		if (!Btn) { continue; }

		Btn->Setup(ThisClass, Row->DisplayName);
		Btn->OnClicked.BindUObject(this, &UTavernWidget::HandleClassChosen);

		ClassButtonBox->AddChild(Btn);
		ClassButtons.Add(Btn);
	}
}

void UTavernWidget::HandleClassChosen(ECharacterClass InClass)
{
	if (InClass == Selected) { return; }
	
	// 준비 완료 상태면 못 바꿈. 여기서 안 막으면 서버는 거절하는데
	// 내 정보 패널만 바뀐 채로 남는다
	const ATerminusPlayerState* MyPS = GetOwningPlayerState<ATerminusPlayerState>();
	if (MyPS && MyPS->IsReady()) { return; }
	// 내 화면은 바로 바꾸고
	ApplySelection(InClass);
	
	// 이후에 서버를 거쳐서 실제 선택되는 로직
	if (ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>())
	{
		PC->Server_SelectCharacter(InClass);
	}
}

void UTavernWidget::HandleInviteClicked()
{
	if (USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>())
	{
		Sessions->ShowInviteUI();
	}
}

void UTavernWidget::HandleStartClicked()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC)
	{
		return;
	}
	
	// 새로고침 풀링 간격 동안 레디를 풀었을수도 있으므로 검사
	// 싱글은 검사 안함
	if (!IsSolo() && !AreAllPlayersReady())
	{
		return;
	}
	
	PC->Server_StartGame();
}

bool UTavernWidget::AreAllPlayersReady() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return false;
	}
	
	if (GS->PlayerArray.Num() == 0)
	{
		return false;
	}
	
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS);
		if (!TPS || !TPS->IsReady())
		{
			// 한명이라도 레디가 안되어있다면
			return false;
		}
	}
	
	return true;
}

void UTavernWidget::HandleReadyClicked()
{
	ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	if (!PC) { return; }
	
	// 내 PS 의 반대값을 서버로 보냄
	const ATerminusPlayerState* PS = PC->GetPlayerState<ATerminusPlayerState>();
	const bool bNext = PS ? !PS->IsReady() : true;
	
	PC->Server_SetReady(bNext);
}

void UTavernWidget::HandleLeaveClicked()
{
	USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>();
	if (!Sessions) { return; }

	// 파괴는 비동기라 그 사이 연타하면 LeaveSession 이 겹쳐 불림
	if (LeaveButton) { LeaveButton->SetIsEnabled(false); }

	Sessions->LeaveToMenu();
}

void UTavernWidget::CreateSlots()
{
	if (!SlotClass)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: SlotClass 가 비어 있음. BP 디폴트에서 WBP_PlayerSlot 을 지정할 것"));
		return;
	}

	// NativeConstruct 는 두 번 불릴 수 있으니 먼저 비우고 시작
	Slots.Reset();
	SlotPanel->ClearChildren();

	for (int32 i = 0; i < MaxSlots; ++i)
	{
		UPlayerSlot* SlotWidget = CreateWidget<UPlayerSlot>(this, SlotClass);
		if (!SlotWidget)
		{
			UE_LOG(LogTerminusUI, Warning, TEXT("TavernWidget: 슬롯 %d 생성 실패"), i);
			continue;
		}

		SlotPanel->AddChild(SlotWidget);
		Slots.Add(SlotWidget);
	}
}

const FCharacterClassRow* UTavernWidget::FindClassRow(ECharacterClass InClass) const
{
	// 실제 조회는 게시판(프로젝트 세팅)이 한다. 여기서 3곳이 부르고 있어서 이름만 남겨둠
	return UTerminusDataSettings::FindCharacterClassRow(InClass);
}

void UTavernWidget::ApplySelection(ECharacterClass InClass)
{
	Selected = InClass;

	for (UClassButton* Btn : ClassButtons)
	{
		if (Btn)
		{
			Btn->SetSelected(Btn->GetCharacterClass() == InClass);
		}
	}
	
	const FCharacterClassRow* Row = FindClassRow(InClass);
	if (!Row)
	{
		UE_LOG(LogTerminusUI, Warning,
			TEXT("TavernWidget: %s 행이 없음. Project Settings > Game > Terminus Data 지정을 확인할 것"),
			*UEnum::GetValueAsString(InClass));
		return;
	}

	if (ClassTitleText)
	{
		ClassTitleText->SetText(Row->DisplayName);
	}
	DescriptionText->SetText(Row->Description);
	PassiveText->SetText(Row->PassiveText);

	if (UTexture2D* Tex = Row->Illustration.LoadSynchronous())
	{
		Illustration->SetBrushFromTexture(Tex);
		Illustration->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// Hidden 으로 자리를 남겨야 옆의 설명 패널이 안 밀린다
		Illustration->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTavernWidget::RefreshSlots()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		// 접속 직후엔 아직 없음
		return;
	}

	TArray<APlayerState*> Players;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS) { Players.Add(PS); }
	}
	Players.Sort([](const APlayerState& A, const APlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		UPlayerSlot* SlotWidget = Slots[i];
		if (!SlotWidget) { continue; }

		APlayerState* PS = Players.IsValidIndex(i) ? Players[i] : nullptr;
		FText ClassName = FText::GetEmpty();
		bool bReady = false;
		if (const ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
		{
			bReady = TPS->IsReady();
			if (const FCharacterClassRow* Row = FindClassRow(TPS->GetCharacterClass()))
			{
				ClassName = Row->DisplayName;
			}
		}

		SlotWidget->Setup(PS, ClassName, bReady);
	}

	// 내 준비 상태 -> 버튼 라벨과 클래스 버튼 잠금에 같이 씀
	// 폴링으로 맞추니 OnRep 안 써도 호스트/클라 동일
	const ATerminusPlayerState* MyPS = GetOwningPlayerState<ATerminusPlayerState>();
	const bool bMyReady = MyPS && MyPS->IsReady();
	
	if (ReadyButtonText)
	{
		ReadyButtonText->SetText(FText::FromString(bMyReady ? TEXT("준비 취소") : TEXT("준비 완료")));
	}
	
	// 스타일은 준비 상태가 바뀐 순간에만. 매 폴링 넣으면 호버 중에 깜빡일 수 있다
	if (ReadyButton && LastReadyShown != (int8)bMyReady)
	{
		ReadyButton->SetStyle(bMyReady ? CancelStyle : ReadyStyle);
		LastReadyShown = (int8)bMyReady;
	}
	
	// 준비 완료면 캐릭터 버튼 잠금
	for (UClassButton* Btn : ClassButtons)
	{
		if (Btn) { Btn->SetIsEnabled(!bMyReady); }
	}

	const ATerminusPlayerController* PC = GetOwningPlayer<ATerminusPlayerController>();
	const bool bIsHost = PC && PC->HasAuthority();
	
	if (StartButton)
	{
		StartButton->SetVisibility(
			bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		
		StartButton->SetIsEnabled(AreAllPlayersReady());
	}
	
	if (WaitText)
	{
		WaitText->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		WaitText->SetText(FText::FromString(bMyReady
			? TEXT("방장의 출발을 기다리는 중")
			: TEXT("준비를 누르면 방장이 출발할 수 있습니다")));
	}
	
	// 인원이 바뀔 때 알려주는 로그
	if (Players.Num() != LastPlayerCount)
	{
		LastPlayerCount = Players.Num();
		UE_LOG(LogTerminusUI, Log, TEXT("TavernWidget: 인원 %d"), LastPlayerCount);
	}
}

bool UTavernWidget::IsSolo() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() == NM_Standalone;
}

void UTavernWidget::ApplySoloLayout()
{
	SlotPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (InviteButton) { InviteButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (ReadyButton)  { ReadyButton->SetVisibility(ESlateVisibility::Collapsed); }

	// 기획서: 싱글은 던전 입장 -> 캐릭터 선택 화면
	if (TitleText) { TitleText->SetText(FText::FromString(TEXT("캐릭터 선택"))); }

	// 폴링이 안 도니 출발 버튼 상태를 여기서 한 번 정해둔다
	if (StartButton)
	{
		StartButton->SetVisibility(ESlateVisibility::Visible);
		StartButton->SetIsEnabled(true);
	}
	
	if (WaitText) { WaitText->SetVisibility(ESlateVisibility::Collapsed); }
}
