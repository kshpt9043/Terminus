#include "Dungeon/DungeonArea.h"

#include "Camera/CameraComponent.h"
#include "Dungeon/DungeonAreaSubsystem.h"
#include "Player/TerminusPlayerState.h"
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
	AreaCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("AreaCamera"));
	AreaCamera->SetupAttachment(Root);
	AreaCamera->SetRelativeLocation(FVector(0.f, 1000.f, 150.f));
	AreaCamera->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// 기본 자리. 플레이어는 왼쪽, 몬스터는 오른쪽. 뷰포트에서 옮기면 됨
	PlayerSlots = {
		FVector(-600.f, 0.f, 0.f),
		FVector(-450.f, 0.f, 0.f),
		FVector(-300.f, 0.f, 0.f),
		FVector(-150.f, 0.f, 0.f)
	};

	MonsterSlots = {
		FVector(200.f, 0.f, 0.f),
		FVector(400.f, 0.f, 0.f),
		FVector(600.f, 0.f, 0.f)
	};
}

void ADungeonArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADungeonArea, bInUse);
	DOREPLIFETIME(ADungeonArea, bCleared);
	DOREPLIFETIME(ADungeonArea, Room);
	DOREPLIFETIME(ADungeonArea, Occupants);
}

void ADungeonArea::BeginPlay()
{
	Super::BeginPlay();

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

void ADungeonArea::BeginRoom(const FRoomNode& InRoom, const TArray<ATerminusPlayerState*>& InPlayers)
{
	if (!HasAuthority()) return;

	Room = InRoom;
	bInUse = true;
	bCleared = false;
	Occupants.Reset();
	ReturnLocations.Reset();

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

	// TODO: 방 타입별 콘텐츠 시작
	//  - 몬스터 / 가디언 / 보스: MonsterSlots 에 몬스터 스폰 (인원 보정은 이 구역 인원 수 기준) + 이 구역의 턴 진행 시작
	//  - 상점 / 휴식터 / 이벤트: 해당 UI
	UE_LOG(LogTemp, Log, TEXT("[Area %d] %d번 방(Row %d, %s) 시작. 인원 %d"),
		AreaIndex, Room.RoomId, Room.Row, *UEnum::GetValueAsString(Room.Type), Occupants.Num());
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

void ADungeonArea::Release()
{
	if (!HasAuthority()) return;

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
