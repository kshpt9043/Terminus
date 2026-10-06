#include "Dungeon/DungeonArea.h"

#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Dungeon/DungeonAreaSet.h"
#include "Dungeon/DungeonAreaSubsystem.h"
#include "Dungeon/DungeonCombatComponent.h"
#include "Dungeon/DungeonThemeData.h"
#include "Player/TerminusPlayerState.h"
#include "Player/TerminusPlayerController.h"
#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ADungeonArea::ADungeonArea()
{
	PrimaryActorTick.bCanEverTick = false;

	// 진행 상태를 클라 HUD / 관전이 봐야 해서 복제. 구역은 몇 개 안 되니 항상 전송
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 2D 사이드뷰 기준: +Y 쪽에서 -Y 를 봄 -> 화면 오른쪽 = +X
	// (배틀러 줄 세우기가 "X 작은 쪽이 왼쪽" 이라 여기에 맞춤). 구도는 레벨에서 조정
	// 거리 650: 화각 90 기준 캐릭터 줄(Y = 0)에서 가로 ±650 이 화면에 들어옴. 슬롯은 그 안쪽에 둠
	// 더 크게 보이게 하려면 Y 를 줄이고 슬롯 간격도 같이 좁힐 것 (안 그러면 양 끝 슬롯이 화면 밖)
	AreaCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AreaCamera"));
	AreaCamera->SetupAttachment(Root);
	AreaCamera->SetRelativeLocation(FVector(0.f, 650.f, 120.f));
	AreaCamera->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// 무대 자리. 구역 원점에 붙음 -> 세트 BP 안의 좌표가 곧 구역 기준 좌표
	SetDisplay = CreateDefaultSubobject<UChildActorComponent>(TEXT("SetDisplay"));
	SetDisplay->SetupAttachment(Root);

	Combat = CreateDefaultSubobject<UDungeonCombatComponent>(TEXT("Combat"));

	// 기본 자리. 플레이어는 왼쪽, 몬스터는 오른쪽. 뷰포트에서 옮기면 됨
	// 양 끝(±520)이 카메라 화면 폭(±650) 안쪽에 여유 있게 들어오도록
	PlayerSlots = {
		FVector(-520.f, 0.f, 0.f),
		FVector(-390.f, 0.f, 0.f),
		FVector(-260.f, 0.f, 0.f),
		FVector(-130.f, 0.f, 0.f)
	};

	MonsterSlots = {
		FVector(170.f, 0.f, 0.f),
		FVector(340.f, 0.f, 0.f),
		FVector(510.f, 0.f, 0.f)
	};
}

void ADungeonArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADungeonArea, bInUse);
	DOREPLIFETIME(ADungeonArea, bCleared);
	DOREPLIFETIME(ADungeonArea, Room);
	DOREPLIFETIME(ADungeonArea, Occupants);
	DOREPLIFETIME(ADungeonArea, CurrentSetClass);
}

void ADungeonArea::OnRep_CurrentSetClass()
{
	// 비어 있으면 지금 떠 있는 무대(레벨 미리보기) 유지
	if (!SetDisplay || !CurrentSetClass) return;

	if (SetDisplay->GetChildActorClass() != CurrentSetClass)
	{
		SetDisplay->SetChildActorClass(CurrentSetClass);
	}
}

void ADungeonArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	PinSetDisplayToOrigin();
}

void ADungeonArea::PinSetDisplayToOrigin()
{
	if (!SetDisplay || SetDisplay->GetRelativeTransform().Equals(FTransform::Identity)) return;

	// 예전에 BP / 레벨에서 SetDisplay 를 옮겨 둔 경우. 옮겨진 만큼 배경이 캐릭터 앞으로 넘어가서 가린 적 있음
	UE_LOG(LogTemp, Warning, TEXT("[Area %d] %s: SetDisplay 가 원점에서 옮겨져 있어서(%s) 원점으로 되돌림. 무대 위치는 세트 BP 안에서 조정할 것"),
		AreaIndex, *GetName(), *SetDisplay->GetRelativeLocation().ToCompactString());

	SetDisplay->SetRelativeTransform(FTransform::Identity);
}

void ADungeonArea::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에서 로드된 구역은 게임 중 구성 스크립트가 다시 안 돌 수 있어서 한 번 더
	PinSetDisplayToOrigin();

	if (UDungeonAreaSubsystem* Subsystem = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
	{
		Subsystem->RegisterArea(this);
	}
}

void ADungeonArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UDungeonAreaSubsystem* Subsystem = World->GetSubsystem<UDungeonAreaSubsystem>())
		{
			Subsystem->UnregisterArea(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

const UCameraComponent* ADungeonArea::GetAreaCamera() const
{
	return AreaCamera;
}

FVector ADungeonArea::GetPlayerSlotLocation(int32 SlotIndex) const
{
	return PlayerSlots.IsValidIndex(SlotIndex)
		? GetActorTransform().TransformPosition(PlayerSlots[SlotIndex])
		: GetActorLocation();
}

FVector ADungeonArea::GetMonsterSlotLocation(int32 SlotIndex) const
{
	return MonsterSlots.IsValidIndex(SlotIndex)
		? GetActorTransform().TransformPosition(MonsterSlots[SlotIndex])
		: GetActorLocation();
}

void ADungeonArea::BeginRoom(const FRoomNode& InRoom, const TArray<ATerminusPlayerState*>& InPlayers, const UDungeonThemeData* Theme)
{
	if (!HasAuthority()) return;

	Room = InRoom;
	bInUse = true;
	bCleared = false;
	Occupants.Reset();
	ReturnLocations.Reset();

	// 테마에서 이 방 타입에 맞는 무대를 고름. 세트가 여러 개면 방마다 랜덤
	if (Theme)
	{
		if (TSubclassOf<ADungeonAreaSet> SetClass = Theme->PickAreaSet(Room.Type))
		{
			CurrentSetClass = SetClass;
			OnRep_CurrentSetClass();    // 리슨 서버 호스트는 OnRep 이 안 불려서 직접
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Area %d] 테마 %s 에 %s 방에 쓸 무대가 없음"),
				AreaIndex, *Theme->GetName(), *UEnum::GetValueAsString(Room.Type));
		}
	}

	for (int32 i = 0; i < InPlayers.Num(); ++i)
	{
		ATerminusPlayerState* PS = InPlayers[i];
		if (!PS) continue;

		Occupants.Add(PS);

		// 배틀러를 슬롯으로. 폰은 위치가 복제되므로 서버에서 옮기면 클라에도 반영됨
		if (APawn* Pawn = PS->GetPawn())
		{
			if (!PlayerSlots.IsValidIndex(i))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Area %d] 플레이어 슬롯 부족 (%d번째 플레이어). 구역 중심에 세움"), AreaIndex, i);
			}

			ReturnLocations.Add(Pawn, Pawn->GetActorLocation());
			Pawn->SetActorLocation(GetPlayerSlotLocation(i), false, nullptr, ETeleportType::TeleportPhysics);
		}

		// 시점 전환은 각 클라가 이 값의 복제를 받고 스스로 함
		PS->SetCurrentArea(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] %d번 방(Row %d, %s) 시작. 인원 %d"),
		AreaIndex, Room.RoomId, Room.Row, *UEnum::GetValueAsString(Room.Type), Occupants.Num());

	// 방 타입별 콘텐츠
	//  - 몬스터 / 가디언 / 보스: 전투 (끝나면 전투 쪽이 MarkCleared)
	//  - 휴식터: 회복 대상 고르기 (전원이 고르면 MarkCleared)
	//  - TODO 상점 / 이벤트 / 퀘스트: 해당 UI. 지금은 DebugClearArea 로 넘김
	if (Room.Type == ERoomType::BREAK)
	{
		BeginRest();
		return;
	}

	if (Combat)
	{
		Combat->StartCombat(Room, InPlayers, Theme);
	}
}

// =====================================================================
// 휴식터
// =====================================================================

void ADungeonArea::BeginRest()
{
	bResting = true;
	RestChosen.Reset();

	TArray<APlayerState*> Present;
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS) Present.Add(PS);
	}

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
		{
			PC->Client_ShowRest(Present, RestHealRatio);
		}
	}

	if (Present.Num() == 0)
	{
		MarkCleared();
	}
}

void ADungeonArea::HandleRestChoice(ATerminusPlayerState* Chooser, ATerminusPlayerState* Target)
{
	if (!HasAuthority() || !bResting || !Chooser || !Target) return;
	if (!Occupants.Contains(Chooser) || !Occupants.Contains(Target)) return;   // 같은 휴식터 사람만
	if (RestChosen.Contains(Chooser)) return;                                  // 한 번만

	RestChosen.Add(Chooser);

	const ATerminusBattler* Battler = Cast<ATerminusBattler>(Target->GetPawn());
	if (UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr)
	{
		const int32 Amount = FMath::Max(1, FMath::RoundToInt(Stats->GetStats().MaxHealth * RestHealRatio));
		Stats->Heal(Amount);

		if (Occupants.Num() > 1)
		{
			FChatMessage Notice;
			Notice.Kind = EChatMessageKind::System;
			Notice.Text = Chooser == Target
				? FString::Printf(TEXT("%s 님이 휴식으로 체력을 %d 회복했습니다."), *Chooser->GetPlayerName(), Amount)
				: FString::Printf(TEXT("%s 님이 %s 님의 체력을 %d 회복시켰습니다."), *Chooser->GetPlayerName(), *Target->GetPlayerName(), Amount);
			ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
		}

		UE_LOG(LogTemp, Log, TEXT("[Area %d] 휴식: %s -> %s +%d"), AreaIndex, *Chooser->GetPlayerName(), *Target->GetPlayerName(), Amount);
	}

	// 전원이 골랐으면 잠깐 보여주고 끝
	bool bAllChosen = true;
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && !RestChosen.Contains(PS)) bAllChosen = false;
	}
	if (bAllChosen)
	{
		GetWorldTimerManager().SetTimer(RestTimer, this, &ADungeonArea::FinishRest, 1.0f, false);
	}
}

void ADungeonArea::FinishRest()
{
	if (!bResting) return;
	MarkCleared();
}

void ADungeonArea::MarkCleared()
{
	if (!HasAuthority() || !bInUse || bCleared) return;

	bCleared = true;

	UE_LOG(LogTemp, Log, TEXT("[Area %d] %d번 방 클리어"), AreaIndex, Room.RoomId);

	if (UDungeonAreaSubsystem* Subsystem = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
	{
		Subsystem->NotifyAreaCleared(this);
	}
}

bool ADungeonArea::IsFightOver() const
{
	if (bCleared) return true;

	const ECombatPhase Phase = Combat ? Combat->GetPhase() : ECombatPhase::None;
	return Phase == ECombatPhase::Victory || Phase == ECombatPhase::Defeat;
}

void ADungeonArea::Release()
{
	if (!HasAuthority()) return;

	// 휴식터 화면 닫기 (전원 고르기 전에 닫히는 경우 포함: 누가 나가서 방 무효 등)
	if (bResting)
	{
		GetWorldTimerManager().ClearTimer(RestTimer);
		for (ATerminusPlayerState* PS : Occupants)
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_CloseRest();
			}
		}
		bResting = false;
		RestChosen.Reset();
	}

	// 몬스터 치우고 전투 상태 초기화
	if (Combat)
	{
		Combat->EndCombat();
	}

	// 배틀러를 지도 화면의 원래 자리로
	for (const TPair<TWeakObjectPtr<APawn>, FVector>& Pair : ReturnLocations)
	{
		if (APawn* Pawn = Pair.Key.Get())
		{
			Pawn->SetActorLocation(Pair.Value, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	ReturnLocations.Reset();

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && PS->GetCurrentArea() == this)
		{
			PS->SetCurrentArea(nullptr);
		}
	}

	Occupants.Reset();
	Room = FRoomNode();
	bInUse = false;
	bCleared = false;
}
