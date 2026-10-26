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
#include "Data/RelicTypes.h"
#include "Data/SkillTypes.h"
#include "Data/TerminusDataSettings.h"
#include "Game/TerminusProfileSubsystem.h"
#include "Game/TerminusRunSubsystem.h"
#include "Game/TerminusQuests.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

FName ADungeonArea::DebugForcedEvent;

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
	CurrentTheme = Theme;
	bInUse = true;
	bCleared = false;
	bWiped = false;
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
	//  - 이벤트: DT_Event 하나, 각자 선택지 중 하나 고르기 (전원이 고르면 MarkCleared, 쉬는 가디언이면 전투)
	//  - 상점: 각자 사고팔기, 전원이 나가면 MarkCleared
	//  - 퀘스트: 각자 퀘스트 3개 중 1개 수락 / 거절 (전원이 고르면 MarkCleared)
	//  - 퀘스트 특수 방: 지정 몬스터와 전투

	// 퀘스트 '한 층에서 몬스터 / 가디언 방 N 번'
	for (ATerminusPlayerState* PS : Occupants)
	{
		TerminusQuests::OnRoomEntered(PS, Room.Type);
	}

	if (Room.Type == ERoomType::QUEST)
	{
		BeginQuestRoom();
		return;
	}
	if (!Room.SpecialRoom.IsNone() && Combat)
	{
		const TArray<FName> Special = TerminusQuests::GetSpecialRoomMonsters(Room.SpecialRoom);
		UE_LOG(LogTemp, Log, TEXT("[Area %d] 퀘스트 특수 방 %s: 몬스터 %d"), AreaIndex, *Room.SpecialRoom.ToString(), Special.Num());
		Combat->SetNextForcedMonsters(Special);
	}

	if (Room.Type == ERoomType::BREAK)
	{
		BeginRest();
		return;
	}
	if (Room.Type == ERoomType::EVENT)
	{
		BeginEvent();
		return;
	}
	if (Room.Type == ERoomType::STORE)
	{
		BeginShop();
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
			PC->Client_ShowRest(Present, RestHealRatio, RestExploreCurrency, RestExploreRelicChance);
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

		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(Chooser->GetOwner()))
		{
			PC->Client_RestResult(FText::FromString(Chooser == Target
				? FString::Printf(TEXT("푹 쉬었습니다. 체력을 %d 회복했습니다."), Amount)
				: FString::Printf(TEXT("%s 님의 체력을 %d 회복시켰습니다."), *Target->GetPlayerName(), Amount)));
		}

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

	CheckRestDone();
}

void ADungeonArea::HandleRestExplore(ATerminusPlayerState* Chooser)
{
	if (!HasAuthority() || !bResting || !Chooser) return;
	if (!Occupants.Contains(Chooser) || RestChosen.Contains(Chooser)) return;   // 휴식 / 탐색 중 하나만, 한 번

	RestChosen.Add(Chooser);

	const int32 Currency = FMath::RandRange(FMath::Min(RestExploreCurrency.X, RestExploreCurrency.Y), FMath::Max(RestExploreCurrency.X, RestExploreCurrency.Y));
	Chooser->AddCurrency(Currency);
	FString Result = FString::Printf(TEXT("주변을 뒤져 던전 재화 %d 을(를) 찾았습니다."), Currency);

	if (FMath::FRand() < RestExploreRelicChance)
	{
		const TArray<FName> Found = Combat ? Combat->PickRewardRelics(Chooser, 1) : TArray<FName>();
		if (Found.Num() > 0)
		{
			const FString RelicName = UTerminusDataSettings::GetRelicName(Found[0]);

			Chooser->MarkRelicsSeen(Found);   // 못 가져가도 이번 런엔 다시 안 나옴
			Result += Chooser->GainRelic(Found[0])
				? FString::Printf(TEXT("\n그리고 유물 [%s] 을(를) 발견했습니다!"), *RelicName)
				: FString::Printf(TEXT("\n유물 [%s] 을(를) 발견했지만 유물 칸이 가득 차 가져가지 못했습니다."), *RelicName);
		}
	}

	if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(Chooser->GetOwner()))
	{
		PC->Client_RestResult(FText::FromString(Result));
	}

	if (Occupants.Num() > 1)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("%s 님이 휴식터를 탐색했습니다. %s"), *Chooser->GetPlayerName(), *Result.Replace(TEXT("\n"), TEXT(" ")));
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 탐색: %s -> %s"), AreaIndex, *Chooser->GetPlayerName(), *Result.Replace(TEXT("\n"), TEXT(" / ")));

	CheckRestDone();
}

void ADungeonArea::CheckRestDone()
{
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && !RestChosen.Contains(PS)) return;
	}

	// 전원이 골랐으면 결과를 잠깐 보여주고 끝
	GetWorldTimerManager().SetTimer(RestTimer, this, &ADungeonArea::FinishRest, 1.5f, false);
}

void ADungeonArea::FinishRest()
{
	if (!bResting) return;
	MarkCleared();
}

// =====================================================================
// 이벤트
// 10-24 기획 엑셀: 방 하나에 이벤트 하나(DT_Event), 선택지 3개(DT_EventChoice) 중 하나를 고름
//  - 공용 이벤트 + 지금 테마 이벤트 중 가중치(AppearanceWeight)로 하나. 한 런 한 번 이벤트는 다시 안 나옴
//  - 멀티: 같은 이벤트를 보고 각자 고름 (받을 스킬 / 유물은 사람마다 따로 미리 정함). 전원이 고르면 끝
//  - 대가는 툴팁 대괄호 안 문구를 따름 (CostType / CostValue 칸과 문구가 다른 행이 있어서). 문구가 없으면 칸 값
//  - '쉬는 가디언' 을 누가 고르면 전원이 고른 뒤 그 방 전원이 약해진 가디언과 싸움
// =====================================================================

namespace
{
	ESkillOwner EventSkillOwnerOf(ECharacterClass InClass)
	{
		switch (InClass)
		{
		case ECharacterClass::Fighter:  return ESkillOwner::Fighter;
		case ECharacterClass::Engineer: return ESkillOwner::Engineer;
		case ECharacterClass::Paladin:  return ESkillOwner::Paladin;
		case ECharacterClass::Assassin: return ESkillOwner::Assassin;
		default:                        return ESkillOwner::Shared;
		}
	}

	template <typename T>
	const T& EventPickRandom(const TArray<T>& Array)
	{
		return Array[FMath::RandRange(0, Array.Num() - 1)];
	}

	// 1~2층 표층, 3~4층 중층, 5층부터 심층
	ESkillTier EventSkillTierOf(int32 Floor)
	{
		return Floor >= 5 ? ESkillTier::Deep : (Floor >= 3 ? ESkillTier::Mid : ESkillTier::Surface);
	}

	ESkillTier EventNextSkillTier(ESkillTier Tier)
	{
		return Tier == ESkillTier::Surface ? ESkillTier::Mid : ESkillTier::Deep;
	}

	ERelicTier EventRelicTierOf(ESkillTier Tier)
	{
		return Tier == ESkillTier::Deep ? ERelicTier::Deep : (Tier == ESkillTier::Mid ? ERelicTier::Mid : ERelicTier::Surface);
	}

	ERelicTier EventNextRelicTier(ERelicTier Tier)
	{
		return Tier == ERelicTier::Surface ? ERelicTier::Mid : ERelicTier::Deep;
	}

	int32 EventCurrentFloor(const UObject* WorldContext)
	{
		const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(WorldContext, AMapManager::StaticClass()));
		return MapMgr ? MapMgr->CurrentFloor : 1;
	}

	bool IsEventSwappableTier(ERelicTier Tier)
	{
		return Tier != ERelicTier::Basic && Tier != ERelicTier::Upgrade;
	}

	// 이벤트가 줄 수 있는 유물인가: 공용 / 내 직업 / 이벤트 / 퀘스트 유물, 안 가진 것, 이번 런에 안 나온 것
	bool IsEventGivableRelic(const ATerminusPlayerState* PS, FName Row, const FRelicRow& Relic)
	{
		if (!IsEventSwappableTier(Relic.RelicTier) || Relic.OwnerClass == ESkillOwner::Monster) return false;
		const bool bGivenOnly = Relic.OwnerClass == ESkillOwner::Event || Relic.OwnerClass == ESkillOwner::Quest;
		if (!bGivenOnly && !ATerminusPlayerState::CanClassHoldRelic(PS->GetCharacterClass(), Relic)) return false;
		return !PS->GetRelics().Contains(Row) && !PS->HasSeenRelic(Row);
	}

	// 무작위 유물 후보 (Tier 계층의 보상용 유물: 공용 / 내 직업. 이벤트 / 퀘스트 전용은 뺌)
	TArray<FName> EventRelicPool(const ATerminusPlayerState* PS, ERelicTier Tier)
	{
		TArray<FName> Pool;
		const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous();
		if (!PS || !Table) return Pool;

		for (const FName& Row : Table->GetRowNames())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (!Relic || Relic->RelicTier != Tier) continue;
			if (Relic->OwnerClass == ESkillOwner::Event || Relic->OwnerClass == ESkillOwner::Quest) continue;
			if (IsEventGivableRelic(PS, Row, *Relic)) Pool.Add(Row);
		}
		return Pool;
	}

	// 테마 / 퀘스트 유물 후보. 유물 데이터에 테마 칸이 없어서 '지금 계층의 퀘스트 유물' 로 봄
	// 정령계 'KingCore' 풀은 이름에 "코어" 가 있는 것(정령왕 코어), 'Lesser' 는 나머지
	TArray<FName> EventThemeRelicPool(const ATerminusPlayerState* PS, ERelicTier Tier, const FString& PoolName)
	{
		TArray<FName> Pool;
		const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous();
		if (!PS || !Table) return Pool;

		const bool bKingCore = PoolName.Contains(TEXT("KingCore"));
		const bool bLesser = PoolName.Contains(TEXT("Lesser"));
		for (const FName& Row : Table->GetRowNames())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (!Relic || Relic->RelicTier != Tier || Relic->OwnerClass != ESkillOwner::Quest) continue;
			const bool bCore = Relic->RelicName.ToString().Contains(TEXT("코어"));
			if ((bKingCore && !bCore) || (bLesser && bCore)) continue;
			if (IsEventGivableRelic(PS, Row, *Relic)) Pool.Add(Row);
		}
		return Pool;
	}

	// OldRelic 을 바꿀 수 있는 유물: Tier 계층, 공용이거나 내 직업, 이번 런에 안 나왔던 것
	TArray<FName> EventSwapPool(const ATerminusPlayerState* PS, FName OldRelic, bool bHigherTier)
	{
		const FRelicRow* Old = UTerminusDataSettings::FindRelicRow(OldRelic);
		if (!PS || !Old || !IsEventSwappableTier(Old->RelicTier)) return TArray<FName>();
		if (Old->OwnerClass == ESkillOwner::Event || Old->OwnerClass == ESkillOwner::Quest) return TArray<FName>();   // 이벤트 / 퀘스트 유물은 계층이 없는 셈이라 안 바꿈
		return EventRelicPool(PS, bHigherTier ? EventNextRelicTier(Old->RelicTier) : Old->RelicTier);
	}

	// 바꿀 수 있는 내 유물 (기본 / 업그레이드 / 이벤트 유물 제외, 바꿀 후보가 있는 것만)
	TArray<FName> EventSwappableRelics(const ATerminusPlayerState* PS, bool bHigherTier)
	{
		TArray<FName> Result;
		if (!PS) return Result;

		for (const FName& Owned : PS->GetRelics())
		{
			if (EventSwapPool(PS, Owned, bHigherTier).Num() > 0) Result.Add(Owned);
		}
		return Result;
	}

	// 대가로 잃을 수 있는 내 유물 (기본 / 업그레이드 유물은 안 잃음)
	TArray<FName> EventLosableRelics(const ATerminusPlayerState* PS)
	{
		TArray<FName> Result;
		for (const FName& Owned : PS->GetRelics())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Owned);
			if (Relic && IsEventSwappableTier(Relic->RelicTier)) Result.Add(Owned);
		}
		return Result;
	}

	// 장착한 픽업 스킬 (강화 칸)
	TArray<FName> EventEquippedSkills(const ATerminusPlayerState* PS, bool bAttackOnly, bool bUpgradableOnly)
	{
		TArray<FName> Result;
		for (const FName& Row : PS->GetRunState().EnhanceSkills)
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
			if (!Skill) continue;
			if (bAttackOnly && Skill->SkillType != ESkillType::Attack) continue;
			if (bUpgradableOnly && PS->GetPickupSkillLevel(Row) >= ATerminusPlayerState::MaxPickupSkillLevel) continue;
			Result.AddUnique(Row);
		}
		return Result;
	}

	// 스킬 풀 (OutcomeTargetPool 이름으로 조건을 정함)
	//  Pool_Skill_OwnClass_* / OtherClass_* / Shared_Common_* / Event_All / Event_Offensive / Event_HighTier_*
	//  CurrentTier = 지금 계층, NextTier = 한 계층 위. 이미 장착한 스킬은 뺌
	TArray<FName> EventSkillPool(const ATerminusPlayerState* PS, const FString& PoolName, ESkillTier CurrentTier)
	{
		TArray<FName> Pool;
		const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous();
		if (!PS || !Table) return Pool;

		const ESkillOwner MyOwner = EventSkillOwnerOf(PS->GetCharacterClass());
		const TArray<FName>& Equipped = PS->GetRunState().EnhanceSkills;
		const bool bEvent = PoolName.Contains(TEXT("_Event_"));
		const bool bNextTier = PoolName.Contains(TEXT("NextTier"));
		const ESkillTier WantTier = bNextTier ? EventNextSkillTier(CurrentTier) : CurrentTier;

		TArray<FName> EventAll, OtherTier;
		for (const FName& Row : Table->GetRowNames())
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
			if (!Skill || !UTerminusProfileSubsystem::IsOwnableSkill(Row) || Equipped.Contains(Row)) continue;

			if (bEvent)
			{
				if (Skill->SkillCategory != ESkillCategory::Event) continue;
				if (PoolName.Contains(TEXT("Offensive")) && Skill->SkillType != ESkillType::Attack) continue;
				EventAll.Add(Row);
				continue;
			}

			if (Skill->SkillEnergyCost <= 0) continue;   // 픽업 스킬 = 스킬 에너지를 쓰는 스킬
			if (PoolName.Contains(TEXT("OwnClass")))
			{
				if (Skill->SkillCategory != ESkillCategory::Personal || Skill->OwnerClass != MyOwner) continue;
			}
			else if (PoolName.Contains(TEXT("OtherClass")))
			{
				if (Skill->SkillCategory != ESkillCategory::Personal || Skill->OwnerClass == MyOwner || Skill->OwnerClass == ESkillOwner::Shared) continue;
			}
			else if (PoolName.Contains(TEXT("Common")))
			{
				if (Skill->SkillCategory != ESkillCategory::Common || !UTerminusProfileSubsystem::IsEquippableSkill(Row, PS->GetCharacterClass())) continue;
			}
			else
			{
				continue;
			}
			(Skill->SkillTier == WantTier ? Pool : OtherTier).Add(Row);
		}

		// 그 계층에 맞는 게 없으면 다른 계층에서라도 (공용 픽업 스킬은 10-24 데이터에 중층 1개뿐)
		if (!bEvent && Pool.Num() == 0)
		{
			Pool = OtherTier;
		}

		if (bEvent)
		{
			// HighTier: 이벤트 스킬 중 가장 높은 계층 것만
			if (PoolName.Contains(TEXT("HighTier")) && EventAll.Num() > 0)
			{
				ESkillTier Highest = ESkillTier::Surface;
				for (const FName& Row : EventAll) Highest = FMath::Max(Highest, UTerminusDataSettings::FindSkillRow(Row)->SkillTier);
				EventAll.RemoveAll([Highest](const FName& Row) { return UTerminusDataSettings::FindSkillRow(Row)->SkillTier != Highest; });
			}
			return EventAll;
		}
		return Pool;
	}

	// 툴팁 대괄호 안에서 "체력 4 감소", "던전 재화 30 소모", "스킬 1개", "유물 1개" 같은 수치를 읽음
	int32 EventFindNumber(const FString& Text, const FString& Pattern)
	{
		FRegexMatcher Matcher(FRegexPattern(Pattern), Text);
		return Matcher.FindNext() ? FCString::Atoi(*Matcher.GetCaptureGroup(1)) : 0;
	}

	void EventParseCost(const FEventChoiceRow& Choice, FEventOption& Out)
	{
		const FString Tooltip = Choice.ChoiceTooltip_KR.ToString();
		FString Bracket;
		int32 Open = INDEX_NONE, Close = INDEX_NONE;
		if (Tooltip.FindChar(TEXT('['), Open) && Tooltip.FindChar(TEXT(']'), Close) && Close > Open)
		{
			Bracket = Tooltip.Mid(Open + 1, Close - Open - 1);
		}

		Out.CostMaxHealth = EventFindNumber(Bracket, TEXT("최대 체력 (\\d+)"));
		Out.CostHealth = EventFindNumber(Bracket.Replace(TEXT("최대 체력"), TEXT("")), TEXT("체력 (\\d+) 감소"));
		Out.CostCurrency = EventFindNumber(Bracket, TEXT("재화 (\\d+) 소모"));
		Out.CostSkills = EventFindNumber(Bracket, TEXT("스킬 (\\d+)개"));
		Out.CostRelics = EventFindNumber(Bracket, TEXT("유물 (\\d+)개"));

		const bool bParsed = Out.CostMaxHealth + Out.CostHealth + Out.CostCurrency + Out.CostSkills + Out.CostRelics > 0;
		if (!bParsed)
		{
			switch (Choice.CostType)
			{
			case EEventCostType::LoseHP:               Out.CostHealth = Choice.CostValue; break;
			case EEventCostType::LoseMaxHP:            Out.CostMaxHealth = Choice.CostValue; break;
			case EEventCostType::SpendCurrency:        Out.CostCurrency = Choice.CostValue; break;
			case EEventCostType::LoseRandomSkill:      Out.CostSkills = FMath::Max(1, Choice.CostValue); break;
			case EEventCostType::LoseRandomRelic:
			case EEventCostType::LoseRandomRelicAndHP: Out.CostRelics = 1; break;
			default: break;
			}
		}

		// 3전투 디버프 유물 (Enum_Reference: RLC_Event_Temp_006)
		if (Choice.CostType == EEventCostType::GainDebuffRelic)
		{
			Out.CostRelic = TEXT("RLC_Event_Temp_006");
		}
	}

	FText EventReason(const TCHAR* Text) { return FText::FromString(Text); }
}

// =====================================================================
// 퀘스트 방 (사용자 결정 2026-10-26: 3개 중 1개, 1개는 내 세력, 사람마다 따로, 그 테마 동안)
// =====================================================================

void ADungeonArea::BeginQuestRoom()
{
	bInQuest = true;
	QuestOffers.Reset();
	EventChosen.Reset();

	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
	const FString ThemeName = CurrentTheme.IsValid() ? CurrentTheme->DisplayName.ToString() : FString();

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (!PS) continue;

		const TArray<FName> Offers = TerminusQuests::MakeOffers(PS, Floor, ThemeName);
		QuestOffers.Add(PS, Offers);

		// 카드: 퀘스트들 + 맨 끝에 거절
		TArray<FEventOption> Options;
		for (const FName& Row : Offers)
		{
			const FQuestRow* Quest = UTerminusDataSettings::FindQuestRow(Row);
			FEventOption Option;
			Option.ChoiceRow = Row;
			Option.Outcome = EEventOutcomeType::Leave;
			Option.Title = Quest->QuestName;
			Option.Description = FText::FromString(TerminusQuests::DescribeOffer(Row));
			Options.Add(Option);
		}
		FEventOption Decline;
		Decline.Outcome = EEventOutcomeType::Leave;
		Decline.Title = FText::FromString(TEXT("의뢰를 받지 않는다"));
		Decline.Description = FText::FromString(TEXT("이번 테마에서는 퀘스트 없이 진행합니다."));
		Options.Add(Decline);

		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
		{
			PC->Client_ShowEvent(FText::FromString(TEXT("세력의 의뢰")),
				FText::FromString(FString::Printf(TEXT("%s에 들어서자 각 세력의 의뢰서가 도착했습니다. 하나를 수락하면 이 테마가 끝날 때까지 진행됩니다."),
					ThemeName.IsEmpty() ? TEXT("새 테마") : *ThemeName)),
				Options);
		}
	}

	CheckEventDone();
}

void ADungeonArea::HandleQuestChoice(ATerminusPlayerState* Chooser, int32 Index)
{
	if (!Chooser || !Occupants.Contains(Chooser) || EventChosen.Contains(Chooser)) return;

	const TArray<FName>* Offers = QuestOffers.Find(Chooser);
	if (!Offers) return;

	FString Result;
	if (Offers->IsValidIndex(Index))
	{
		const FName Row = (*Offers)[Index];
		TerminusQuests::Accept(Chooser, Row);
		const FQuestRow* Quest = UTerminusDataSettings::FindQuestRow(Row);
		Result = FString::Printf(TEXT("[%s] 의뢰를 수락했습니다.\n\n%s"), Quest ? *Quest->QuestName.ToString() : *Row.ToString(), *TerminusQuests::DescribeOffer(Row));
	}
	else
	{
		Result = TEXT("의뢰를 받지 않고 길을 나섭니다.");
	}

	EventChosen.Add(Chooser);
	if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(Chooser->GetOwner()))
	{
		PC->Client_EventResult(FText::FromString(Result));
	}
	UE_LOG(LogTemp, Log, TEXT("[Area %d] 퀘스트: %s -> %s"), AreaIndex, *Chooser->GetPlayerName(), Offers->IsValidIndex(Index) ? *(*Offers)[Index].ToString() : TEXT("거절"));

	CheckEventDone();
}

void ADungeonArea::BeginEvent()
{
	bInEvent = true;
	EventOffers.Reset();
	EventChosen.Reset();
	EventGuardianHealth = 0.f;
	EventGuardianBonus.Reset();
	CurrentEvent = PickEventRow();

	const FEventRow* Event = CurrentEvent.IsNone() ? nullptr : UTerminusDataSettings::FindEventRow(CurrentEvent);
	if (Event)
	{
		if (UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
		{
			Run->AddSeenEvent(CurrentEvent);
		}
		UE_LOG(LogTemp, Log, TEXT("[Area %d] 이벤트: %s (%s)"), AreaIndex, *Event->EventName_KR.ToString(), *CurrentEvent.ToString());
	}

	const FText Title = Event ? Event->EventName_KR : FText::FromString(TEXT("이벤트"));
	const FText Situation = Event ? Event->SituationDesc_KR : EventFallbackText;

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (!PS) continue;

		TArray<FEventOption> Options;
		if (Event)
		{
			TArray<FName> Seen;
			for (const FName& ChoiceRow : { Event->Choice1_ID, Event->Choice2_ID, Event->Choice3_ID })
			{
				if (ChoiceRow.IsNone() || !UTerminusDataSettings::FindEventChoiceRow(ChoiceRow)) continue;
				const FEventOption Option = MakeEventOption(PS, ChoiceRow);
				if (!Option.RelicRow.IsNone()) Seen.Add(Option.RelicRow);
				Options.Add(Option);
			}
			PS->MarkRelicsSeen(Seen);   // 카드에 보인 유물은 이번 런에 다시 안 나옴 (사용자 결정 10-06)
		}
		EventOffers.Add(PS, Options);

		// 고를 수 있는 게 없으면 고른 걸로 (화면엔 '아무 일도 일어나지 않았습니다')
		if (!Options.ContainsByPredicate([](const FEventOption& O) { return O.bAvailable; }))
		{
			EventChosen.Add(PS);
		}

		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
		{
			PC->Client_ShowEvent(Title, Situation, Options);
		}
	}

	CheckEventDone();
}

FName ADungeonArea::PickEventRow() const
{
	if (!DebugForcedEvent.IsNone() && UTerminusDataSettings::FindEventRow(DebugForcedEvent))
	{
		return DebugForcedEvent;
	}

	const UDataTable* Table = UTerminusDataSettings::Get()->EventTable.LoadSynchronous();
	if (!Table) return NAME_None;

	const ESkillTier Tier = EventSkillTierOf(EventCurrentFloor(this));
	const EEventAppearTier WantTier = Tier == ESkillTier::Deep ? EEventAppearTier::Deep : (Tier == ESkillTier::Mid ? EEventAppearTier::Mid : EEventAppearTier::Surface);
	const FString ThemeName = CurrentTheme.IsValid() ? CurrentTheme->DisplayName.ToString() : FString();

	const UTerminusRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr;

	TArray<TPair<FName, int32>> Candidates;
	int32 Total = 0;
	for (const FName& Row : Table->GetRowNames())
	{
		const FEventRow* Event = UTerminusDataSettings::FindEventRow(Row);
		if (!Event || Event->AppearanceWeight <= 0) continue;

		// 공용 이벤트 또는 지금 테마 이벤트, 계층은 Any 이거나 지금 계층
		const bool bCommon = Event->EventScope.IsEmpty() || Event->EventScope == TEXT("공용");
		if (!bCommon && Event->EventScope != ThemeName) continue;
		if (Event->AppearTier != EEventAppearTier::Any && Event->AppearTier != WantTier) continue;
		if (Event->bOneTimePerRun && Run && Run->GetSeenEvents().Contains(Row)) continue;

		Candidates.Emplace(Row, Event->AppearanceWeight);
		Total += Event->AppearanceWeight;
	}
	if (Total <= 0) return NAME_None;

	int32 Roll = FMath::RandRange(1, Total);
	for (const TPair<FName, int32>& C : Candidates)
	{
		Roll -= C.Value;
		if (Roll <= 0) return C.Key;
	}
	return Candidates.Last().Key;
}

FEventOption ADungeonArea::MakeEventOption(const ATerminusPlayerState* PS, FName ChoiceRow) const
{
	FEventOption Option;
	const FEventChoiceRow* Choice = UTerminusDataSettings::FindEventChoiceRow(ChoiceRow);
	if (!PS || !Choice)
	{
		Option.bAvailable = false;
		return Option;
	}

	Option.ChoiceRow = ChoiceRow;
	Option.Outcome = Choice->OutcomeType;
	Option.Title = Choice->ChoiceTitle_KR;
	Option.Description = Choice->ChoiceTooltip_KR;
	EventParseCost(*Choice, Option);

	const ESkillTier Tier = EventSkillTierOf(EventCurrentFloor(this));
	const FString Pool = Choice->OutcomeTargetPool.ToString();
	const int32 Min = FMath::Min(Choice->OutcomeValueMin, Choice->OutcomeValueMax);
	const int32 Max = FMath::Max(Choice->OutcomeValueMin, Choice->OutcomeValueMax);

	auto Unavailable = [&Option](const TCHAR* Reason)
	{
		Option.bAvailable = false;
		Option.UnavailableReason = EventReason(Reason);
	};

	// ---- 결과 미리 정하기
	switch (Choice->OutcomeType)
	{
	case EEventOutcomeType::GiveOwnClassSkill:
	case EEventOutcomeType::GiveCommonSkill:
	case EEventOutcomeType::GiveOtherClassSkill:
	case EEventOutcomeType::GiveEventSkill:
	case EEventOutcomeType::GiveHigherTierSkill:
	{
		const TArray<FName> Skills = EventSkillPool(PS, Pool, Tier);
		if (Skills.Num() > 0) Option.SkillRow = EventPickRandom(Skills);
		else Unavailable(TEXT("받을 수 있는 스킬이 없습니다."));
		break;
	}

	case EEventOutcomeType::UpgradeRandomSkill:
		Option.Amount = FMath::Max(1, Min);
		if (EventEquippedSkills(PS, Pool.Contains(TEXT("Attack")), true).Num() == 0) Unavailable(TEXT("강화할 스킬이 없습니다."));
		break;

	case EEventOutcomeType::GiveTempStatRelic:
	case EEventOutcomeType::GivePermStatRelic:
		Option.RelicRow = Choice->OutcomeTargetPool;
		if (!UTerminusDataSettings::FindRelicRow(Option.RelicRow)) Unavailable(TEXT("유물 데이터가 없습니다."));
		else if (PS->GetRelics().Contains(Option.RelicRow)) Unavailable(TEXT("이미 가진 유물입니다."));
		break;

	case EEventOutcomeType::GiveThemeQuestRelic:
	case EEventOutcomeType::GiveHigherTierRelic:
	{
		const ERelicTier Current = EventRelicTierOf(Tier);
		TArray<FName> Relics;
		if (Pool.Contains(TEXT("Theme"))) Relics = EventThemeRelicPool(PS, Current, Pool);
		else if (Pool.Contains(TEXT("DeepTier"))) Relics = EventRelicPool(PS, ERelicTier::Deep);
		else if (Pool.Contains(TEXT("MidTier"))) Relics = EventRelicPool(PS, ERelicTier::Mid);
		else Relics = EventRelicPool(PS, EventNextRelicTier(Current));
		if (Relics.Num() > 0) Option.RelicRow = EventPickRandom(Relics);
		else Unavailable(TEXT("받을 수 있는 유물이 없습니다."));
		break;
	}

	case EEventOutcomeType::SwapRelicSameTierRandom:
	case EEventOutcomeType::SwapRelicHigherTierRandom:
		// 바뀌는 유물이 곧 대가 (LoseRandomRelic). 따로 또 잃지 않음
		Option.CostRelics = 0;
		if (EventSwappableRelics(PS, Choice->OutcomeType == EEventOutcomeType::SwapRelicHigherTierRandom).Num() == 0)
		{
			Unavailable(TEXT("바꿀 수 있는 유물이 없습니다."));
		}
		// PlusGold: 툴팁의 "던전 재화 20" 만큼 더
		if (Pool.Contains(TEXT("PlusGold")))
		{
			const FString Tooltip = Choice->ChoiceTooltip_KR.ToString();
			int32 Close = INDEX_NONE;
			Tooltip.FindChar(TEXT(']'), Close);
			Option.Amount = EventFindNumber(Tooltip.Mid(Close + 1), TEXT("재화 (\\d+)"));
		}
		break;

	case EEventOutcomeType::GiveCurrencyRange:
		Option.Amount = FMath::RandRange(Min, Max);
		break;

	case EEventOutcomeType::HealHP:
		Option.Amount = Max;
		break;

	case EEventOutcomeType::StartWeakGuardianBattle:
		Option.Amount = Min > 0 ? Min : 70;   // 체력 %
		break;

	default:
		break;
	}

	// ---- 툴팁에만 적힌 추가 결과 (대괄호 뒤 문구). 결과 칸이 하나뿐이라 데이터로는 안 들어옴
	{
		const FString Tooltip = Choice->ChoiceTooltip_KR.ToString();
		int32 Close = INDEX_NONE;
		const FString After = Tooltip.FindChar(TEXT(']'), Close) ? Tooltip.Mid(Close + 1) : Tooltip;

		if (Choice->OutcomeType != EEventOutcomeType::HealHP)
		{
			Option.BonusHeal = EventFindNumber(After, TEXT("체력을 (\\d+) 회복"));
		}
		if (Choice->OutcomeType != EEventOutcomeType::UpgradeRandomSkill && After.Contains(TEXT("+1강 강화")))
		{
			Option.BonusUpgrades = 1;
		}
		if (Choice->OutcomeType == EEventOutcomeType::StartWeakGuardianBattle && After.Contains(TEXT("승리 시")))
		{
			Option.BonusWinCurrency = EventFindNumber(After, TEXT("재화 (\\d+) 획득"));
			Option.bBonusWinRelic = After.Contains(TEXT("유물"));
		}
	}

	// ---- 대가를 치를 수 있나
	const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
	const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
	const int32 Health = Stats ? Stats->GetCombatState().Health : 0;
	const int32 MaxHealth = Stats ? Stats->GetStats().MaxHealth : 0;

	if (Option.bAvailable && Option.CostHealth > 0 && Health <= Option.CostHealth) Unavailable(TEXT("체력이 부족합니다."));
	if (Option.bAvailable && Option.CostMaxHealth > 0 && MaxHealth <= Option.CostMaxHealth) Unavailable(TEXT("최대 체력이 부족합니다."));
	if (Option.bAvailable && Option.CostCurrency > 0 && PS->GetRunState().Currency < Option.CostCurrency) Unavailable(TEXT("던전 재화가 부족합니다."));
	if (Option.bAvailable && Option.CostSkills > 0 && EventEquippedSkills(PS, false, false).Num() < Option.CostSkills) Unavailable(TEXT("잃을 스킬이 없습니다."));
	if (Option.bAvailable && Option.CostRelics > 0 && EventLosableRelics(PS).Num() < Option.CostRelics) Unavailable(TEXT("잃을 유물이 없습니다."));
	if (Option.bAvailable && !Option.CostRelic.IsNone())
	{
		// 디버프 유물은 칸 하나를 차지함. 꽉 찼거나 이미 있으면 못 고름 (버릴 유물을 고르게 하면 대가가 없어지는 셈이라)
		const bool bFull = PS->GetRelics().Num() >= PS->GetRelicCapacity();
		if (!UTerminusDataSettings::FindRelicRow(Option.CostRelic)) Unavailable(TEXT("유물 데이터가 없습니다."));
		else if (PS->GetRelics().Contains(Option.CostRelic)) Unavailable(TEXT("이미 같은 저주를 받고 있습니다."));
		else if (bFull) Unavailable(TEXT("유물 칸이 가득 차 고를 수 없습니다."));
	}

	return Option;
}

bool ADungeonArea::ApplyEventOption(ATerminusPlayerState* PS, const FEventOption& Option, int32 ReplaceSlot, FName RelicRow, FString& OutResult)
{
	if (!PS || !Option.bAvailable) return false;

	const FEventChoiceRow* Choice = UTerminusDataSettings::FindEventChoiceRow(Option.ChoiceRow);
	ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
	UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
	if (!Choice) return false;

	// ---- 먼저 다 되는지 확인 (대가만 치르고 못 받는 일이 없게)
	if (Option.CostHealth > 0 && (!Stats || Stats->GetCombatState().Health <= Option.CostHealth)) return false;
	if (Option.CostCurrency > 0 && PS->GetRunState().Currency < Option.CostCurrency) return false;
	if (Option.GivesSkill() && PS->GetRunState().EnhanceSkills.Num() >= PS->GetEnhanceSlotCount()
		&& !PS->GetRunState().EnhanceSkills.IsValidIndex(ReplaceSlot)) return false;

	const bool bRelicFull = PS->GetRelics().Num() >= PS->GetRelicCapacity();
	if (Option.GivesRelic() && bRelicFull && !EventLosableRelics(PS).Contains(RelicRow)) return false;

	const bool bHigher = Option.Outcome == EEventOutcomeType::SwapRelicHigherTierRandom;
	const bool bSwap = bHigher || Option.Outcome == EEventOutcomeType::SwapRelicSameTierRandom;
	const TArray<FName> Swappable = bSwap ? EventSwappableRelics(PS, bHigher) : TArray<FName>();
	if (bSwap && Swappable.Num() == 0) return false;

	TArray<FString> Lines;

	// ---- 대가
	if (Option.CostCurrency > 0)
	{
		PS->AddCurrency(-Option.CostCurrency);
		Lines.Add(FString::Printf(TEXT("던전 재화 -%d"), Option.CostCurrency));
	}
	if (Option.CostHealth > 0 && Stats)
	{
		Stats->ApplyDamage(Option.CostHealth);
		Lines.Add(FString::Printf(TEXT("체력 -%d"), Option.CostHealth));
	}
	if (Option.CostMaxHealth > 0)
	{
		PS->ApplyPermanentStat(0, -Option.CostMaxHealth);
		Lines.Add(FString::Printf(TEXT("최대 체력 -%d"), Option.CostMaxHealth));
	}
	for (int32 i = 0; i < Option.CostSkills; ++i)
	{
		const TArray<FName> Equipped = EventEquippedSkills(PS, false, false);
		if (Equipped.Num() == 0) break;
		const FName Lost = EventPickRandom(Equipped);
		PS->UnequipEnhanceSkill(Lost);
		Lines.Add(FString::Printf(TEXT("스킬 소실: %s"), *UTerminusDataSettings::GetSkillName(Lost)));
	}
	for (int32 i = 0; i < Option.CostRelics; ++i)
	{
		const TArray<FName> Losable = EventLosableRelics(PS);
		if (Losable.Num() == 0) break;
		const FName Lost = EventPickRandom(Losable);
		PS->RemoveRelic(Lost);
		Lines.Add(FString::Printf(TEXT("유물 소실: %s"), *UTerminusDataSettings::GetRelicName(Lost)));
	}
	if (!Option.CostRelic.IsNone() && PS->GainRelic(Option.CostRelic))
	{
		Lines.Add(FString::Printf(TEXT("저주 유물: %s"), *UTerminusDataSettings::GetRelicName(Option.CostRelic)));
	}

	// ---- 결과
	switch (Option.Outcome)
	{
	case EEventOutcomeType::GiveOwnClassSkill:
	case EEventOutcomeType::GiveCommonSkill:
	case EEventOutcomeType::GiveOtherClassSkill:
	case EEventOutcomeType::GiveEventSkill:
	case EEventOutcomeType::GiveHigherTierSkill:
		if (PS->EquipEnhanceSkill(Option.SkillRow, ReplaceSlot))
		{
			Lines.Add(FString::Printf(TEXT("스킬 획득: %s"), *UTerminusDataSettings::GetSkillName(Option.SkillRow)));
		}
		break;

	case EEventOutcomeType::UpgradeRandomSkill:
		for (int32 i = 0; i < Option.Amount; ++i)
		{
			const TArray<FName> Candidates = EventEquippedSkills(PS, Choice->OutcomeTargetPool.ToString().Contains(TEXT("Attack")), true);
			if (Candidates.Num() == 0) break;
			const FName Skill = EventPickRandom(Candidates);
			if (PS->UpgradePickupSkill(Skill))
			{
				Lines.Add(FString::Printf(TEXT("스킬 강화: %s +%d"), *UTerminusDataSettings::GetSkillName(Skill), PS->GetPickupSkillLevel(Skill)));
			}
		}
		break;

	case EEventOutcomeType::GiveTempStatRelic:
	case EEventOutcomeType::GivePermStatRelic:
	case EEventOutcomeType::GiveThemeQuestRelic:
	case EEventOutcomeType::GiveHigherTierRelic:
		if (bRelicFull && PS->RemoveRelic(RelicRow))
		{
			Lines.Add(FString::Printf(TEXT("유물 버림: %s"), *UTerminusDataSettings::GetRelicName(RelicRow)));
		}
		if (PS->GainRelic(Option.RelicRow))
		{
			const int32 Battles = PS->GetTempRelicBattlesLeft(Option.RelicRow);
			Lines.Add(Battles > 0
				? FString::Printf(TEXT("유물 획득: %s (전투 %d번)"), *UTerminusDataSettings::GetRelicName(Option.RelicRow), Battles)
				: FString::Printf(TEXT("유물 획득: %s"), *UTerminusDataSettings::GetRelicName(Option.RelicRow)));
		}
		break;

	case EEventOutcomeType::SwapRelicSameTierRandom:
	case EEventOutcomeType::SwapRelicHigherTierRandom:
	{
		const FName Old = EventPickRandom(Swappable);
		const FName New = EventPickRandom(EventSwapPool(PS, Old, bHigher));
		if (PS->SwapRelic(Old, New))
		{
			Lines.Add(FString::Printf(TEXT("유물 변화: %s → %s"), *UTerminusDataSettings::GetRelicName(Old), *UTerminusDataSettings::GetRelicName(New)));
		}
		if (Option.Amount > 0)
		{
			PS->AddCurrency(Option.Amount);
			Lines.Add(FString::Printf(TEXT("던전 재화 +%d"), Option.Amount));
		}
		break;
	}

	case EEventOutcomeType::GiveCurrencyRange:
		PS->AddCurrency(Option.Amount);
		Lines.Add(FString::Printf(TEXT("던전 재화 +%d"), Option.Amount));
		break;

	case EEventOutcomeType::HealHP:
		if (Stats)
		{
			Stats->Heal(Option.Amount);
			Lines.Add(FString::Printf(TEXT("체력 +%d"), Option.Amount));
		}
		break;

	case EEventOutcomeType::StartWeakGuardianBattle:
		EventGuardianHealth = FMath::Max(EventGuardianHealth, Option.Amount / 100.f);
		Lines.Add(TEXT("모두가 고르면 약해진 가디언과 전투합니다."));
		if (Option.BonusWinCurrency > 0 || Option.bBonusWinRelic)
		{
			FGuardianWinBonus& Bonus = EventGuardianBonus.FindOrAdd(PS);
			Bonus.Currency = Option.BonusWinCurrency;
			Bonus.bThemeRelic = Option.bBonusWinRelic;
		}
		break;

	default:
		break;
	}

	// 툴팁에만 적힌 추가 결과
	if (Option.BonusHeal > 0 && Stats)
	{
		Stats->Heal(Option.BonusHeal);
		Lines.Add(FString::Printf(TEXT("체력 +%d"), Option.BonusHeal));
	}
	for (int32 i = 0; i < Option.BonusUpgrades; ++i)
	{
		const TArray<FName> Candidates = EventEquippedSkills(PS, false, true);
		if (Candidates.Num() == 0) break;
		const FName Skill = EventPickRandom(Candidates);
		if (PS->UpgradePickupSkill(Skill))
		{
			Lines.Add(FString::Printf(TEXT("스킬 강화: %s +%d"), *UTerminusDataSettings::GetSkillName(Skill), PS->GetPickupSkillLevel(Skill)));
		}
	}

	// 기획 결과 문구 + 실제로 바뀐 것
	const FString Story = Choice->ResultText_KR.ToString();
	OutResult = Story.IsEmpty() ? FString::Join(Lines, TEXT("\n")) : Story + (Lines.Num() > 0 ? TEXT("\n\n") + FString::Join(Lines, TEXT("\n")) : FString());
	return true;
}

void ADungeonArea::HandleEventChoice(ATerminusPlayerState* Chooser, int32 Index, int32 ReplaceSlot, FName RelicRow)
{
	if (HasAuthority() && bInQuest)
	{
		HandleQuestChoice(Chooser, Index);
		return;
	}
	if (!HasAuthority() || !bInEvent || !Chooser) return;
	if (!Occupants.Contains(Chooser) || EventChosen.Contains(Chooser)) return;

	const TArray<FEventOption>* Offers = EventOffers.Find(Chooser);
	if (!Offers) return;

	ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(Chooser->GetOwner());

	FString Result;
	if (!Offers->IsValidIndex(Index) || !ApplyEventOption(Chooser, (*Offers)[Index], ReplaceSlot, RelicRow, Result))
	{
		// 잘못 골랐거나(칸 / 유물 / 재화가 그새 바뀜) 적용 실패: 선택지를 지금 상태로 다시 풀어서 보내 다시 고르게
		UE_LOG(LogTemp, Warning, TEXT("[Area %d] 이벤트: %s 의 선택(%d) 적용 실패. 다시 고르게 함"), AreaIndex, *Chooser->GetPlayerName(), Index);
		TArray<FEventOption> Fresh;
		for (const FEventOption& Old : *Offers)
		{
			FEventOption Again = MakeEventOption(Chooser, Old.ChoiceRow);
			// 미리 보여 준 스킬 / 유물 / 재화는 그대로 (다시 굴려서 바뀌지 않게)
			Again.SkillRow = Old.SkillRow;
			Again.RelicRow = Old.RelicRow;
			Again.Amount = Old.Amount;
			Fresh.Add(Again);
		}
		EventOffers.Add(Chooser, Fresh);

		const FEventRow* Event = UTerminusDataSettings::FindEventRow(CurrentEvent);
		if (PC) PC->Client_ShowEvent(Event ? Event->EventName_KR : FText::GetEmpty(), Event ? Event->SituationDesc_KR : EventFallbackText, Fresh);
		return;
	}

	EventChosen.Add(Chooser);
	if (PC) PC->Client_EventResult(FText::FromString(Result));

	const FString Summary = (*Offers)[Index].Title.ToString();
	if (Occupants.Num() > 1)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("[이벤트] %s 님: %s"), *Chooser->GetPlayerName(), *Summary);
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 이벤트: %s -> %s / %s"), AreaIndex, *Chooser->GetPlayerName(), *(*Offers)[Index].ChoiceRow.ToString(), *Result.Replace(TEXT("\n"), TEXT(" / ")));

	CheckEventDone();
}

void ADungeonArea::CheckEventDone()
{
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && !EventChosen.Contains(PS)) return;
	}

	// 전원이 골랐으면 결과를 잠깐 보여주고 끝 (전투가 있으면 조금 더 길게)
	GetWorldTimerManager().SetTimer(EventTimer, this, &ADungeonArea::FinishEvent, EventGuardianHealth > 0.f ? 2.5f : 1.5f, false);
}

void ADungeonArea::GiveGuardianWinBonus()
{
	// 이벤트 '쉬는 가디언' 을 이김: 고른 사람에게 툴팁의 테마 유물 + 던전 재화 (일반 가디언 보상에 더해서)
	for (const TPair<TWeakObjectPtr<ATerminusPlayerState>, FGuardianWinBonus>& Pair : EventGuardianBonus)
	{
		ATerminusPlayerState* PS = Pair.Key.Get();
		if (!PS || !Occupants.Contains(PS)) continue;

		TArray<FString> Lines;
		if (Pair.Value.Currency > 0)
		{
			PS->AddCurrency(Pair.Value.Currency);
			Lines.Add(FString::Printf(TEXT("던전 재화 +%d"), Pair.Value.Currency));
		}
		if (Pair.Value.bThemeRelic)
		{
			const TArray<FName> Pool = EventThemeRelicPool(PS, EventRelicTierOf(EventSkillTierOf(EventCurrentFloor(this))), FString());
			if (Pool.Num() > 0)
			{
				const FName Relic = EventPickRandom(Pool);
				PS->MarkRelicsSeen({ Relic });
				Lines.Add(PS->GainRelic(Relic)
					? FString::Printf(TEXT("유물 획득: %s"), *UTerminusDataSettings::GetRelicName(Relic))
					: FString::Printf(TEXT("유물 [%s] 을(를) 찾았지만 유물 칸이 가득 차 가져가지 못했습니다."), *UTerminusDataSettings::GetRelicName(Relic)));
			}
		}
		if (Lines.Num() == 0) continue;

		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("[이벤트] %s 님 가디언 기습 성공: %s"), *PS->GetPlayerName(), *FString::Join(Lines, TEXT(", ")));
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
		UE_LOG(LogTemp, Log, TEXT("[Area %d] %s"), AreaIndex, *Notice.Text);
	}
	EventGuardianBonus.Reset();
}

void ADungeonArea::FinishEvent()
{
	if (bInQuest)
	{
		MarkCleared();
		return;
	}
	if (!bInEvent) return;

	if (EventGuardianHealth <= 0.f || !Combat)
	{
		MarkCleared();
		return;
	}

	// 쉬는 가디언: 이벤트 화면을 닫고 이 방 전원이 가디언과 싸움. 끝나면 전투 쪽이 가디언 보상 후 MarkCleared
	GetWorldTimerManager().ClearTimer(EventTimer);
	TArray<ATerminusPlayerState*> Players;
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (!PS) continue;
		Players.Add(PS);
		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
		{
			PC->Client_CloseEvent();
		}
	}
	bInEvent = false;
	EventOffers.Reset();
	EventChosen.Reset();

	FRoomNode GuardianRoom = Room;
	GuardianRoom.Type = ERoomType::GUARDIAN;
	Combat->SetNextSpawnHealthScale(EventGuardianHealth);
	EventGuardianHealth = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 이벤트: 쉬는 가디언 전투 시작"), AreaIndex);
	Combat->StartCombat(GuardianRoom, Players, CurrentTheme.Get());
}

// =====================================================================
// 상점 (사용자 결정 2026-10-09: 진열은 미정이라 도감(데이터 전체)에서 아무거나)
// =====================================================================

namespace
{
	template <typename T>
	void ShopShuffle(TArray<T>& Array)
	{
		for (int32 i = Array.Num() - 1; i > 0; --i)
		{
			Array.Swap(i, FMath::RandRange(0, i));
		}
	}

}

void ADungeonArea::BeginShop()
{
	bShopping = true;
	ShopStates.Reset();
	ShopDone.Reset();

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (!PS) continue;
		ShopStates.Add(PS, MakeShopState(PS));
		SendShop(PS, FString());
	}

	CheckShopDone();
}

FShopState ADungeonArea::MakeShopState(ATerminusPlayerState* PS) const
{
	FShopState State;
	State.UpgradeCostPerLevel = ShopUpgradeCostPerLevel;
	if (!PS) return State;

	// 노션 방 생성 메커니즘(10-09): 같은 계층 + 같은 직업 또는 shared 인 유물 / 픽업 스킬
	const ESkillTier Tier = EventSkillTierOf(EventCurrentFloor(this));
	const ERelicTier RelicTier = EventRelicTierOf(Tier);

	// 유물: 상점에 나올 수 있고(가격 있음, 기본 / 업그레이드 아님) 지금 계층, 이 직업이 가질 수 있는 것, 가진 것 / 이번 런에 나온 것 빼고
	TArray<FName> Relics;
	if (const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous())
	{
		for (const FName& Row : Table->GetRowNames())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (!Relic || !Relic->CanBuyInShop() || Relic->RelicTier != RelicTier) continue;
			if (!ATerminusPlayerState::CanClassHoldRelic(PS->GetCharacterClass(), *Relic)) continue;
			if (PS->GetRelics().Contains(Row) || PS->HasSeenRelic(Row)) continue;
			Relics.Add(Row);
		}
	}
	ShopShuffle(Relics);
	Relics.SetNum(FMath::Min(Relics.Num(), ShopRelicCount));
	for (const FName& Row : Relics)
	{
		State.RelicOffers.Add(Row);
		State.RelicPrices.Add(FMath::Max(0, UTerminusDataSettings::FindRelicRow(Row)->BuyPrice_Dungeon));
		State.RelicBought.Add(false);
	}
	PS->MarkRelicsSeen(State.RelicOffers);   // 상점에 나온 유물은 안 사도 이번 런엔 다시 안 나옴 (기획 유물 특이사항)

	// 픽업 스킬: 지금 계층, 이 직업이 장착할 수 있는, 스킬 에너지를 쓰는 스킬 (이벤트 전용 제외). 이미 장착한 것 빼고
	TArray<FName> Skills;
	const TArray<FName> Equipped = PS->GetRunState().EnhanceSkills;
	if (const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous())
	{
		for (const FName& Row : Table->GetRowNames())
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
			if (!Skill || Skill->SkillEnergyCost <= 0 || Equipped.Contains(Row)) continue;
			if (Skill->SkillTier != Tier || Skill->SkillCategory == ESkillCategory::Event) continue;
			if (!UTerminusProfileSubsystem::IsEquippableSkill(Row, PS->GetCharacterClass())) continue;
			Skills.Add(Row);
		}
	}
	ShopShuffle(Skills);
	Skills.SetNum(FMath::Min(Skills.Num(), ShopSkillCount));
	for (const FName& Row : Skills)
	{
		State.SkillOffers.Add(Row);
		// 10-24 엑셀 스킬 구매가. 가격이 비어 있는(0) 스킬만 임시 고정가
		const int32 DataPrice = UTerminusDataSettings::FindSkillRow(Row)->BuyPrice_Dungeon;
		State.SkillPrices.Add(DataPrice > 0 ? DataPrice : ShopSkillPrice);
		State.SkillBought.Add(false);
	}
	return State;
}

void ADungeonArea::SendShop(ATerminusPlayerState* PS, const FString& Message)
{
	FShopState* State = PS ? ShopStates.Find(PS) : nullptr;
	ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr;
	if (!State || !PC) return;

	State->Message = FText::FromString(Message);
	PC->Client_ShowShop(*State);
}

void ADungeonArea::HandleShopBuyRelic(ATerminusPlayerState* PS, int32 Index)
{
	FShopState* State = bShopping && PS && !ShopDone.Contains(PS) ? ShopStates.Find(PS) : nullptr;
	if (!State || !State->RelicOffers.IsValidIndex(Index) || State->RelicBought[Index]) return;

	const FName Row = State->RelicOffers[Index];
	const int32 Price = State->RelicPrices[Index];
	if (PS->GetRunState().Currency < Price)
	{
		SendShop(PS, TEXT("던전 재화가 부족합니다."));
		return;
	}
	if (PS->GetRelics().Num() >= PS->GetRelicCapacity())
	{
		SendShop(PS, TEXT("유물 칸이 가득 찼습니다. 먼저 유물을 판매하세요."));
		return;
	}
	if (!PS->GainRelic(Row))
	{
		SendShop(PS, TEXT("이 유물은 가질 수 없습니다."));
		return;
	}

	PS->AddCurrency(-Price);
	State->RelicBought[Index] = true;
	SendShop(PS, FString::Printf(TEXT("구매: %s"), *UTerminusDataSettings::GetRelicName(Row)));
}

void ADungeonArea::HandleShopBuySkill(ATerminusPlayerState* PS, int32 Index, int32 ReplaceSlot)
{
	FShopState* State = bShopping && PS && !ShopDone.Contains(PS) ? ShopStates.Find(PS) : nullptr;
	if (!State || !State->SkillOffers.IsValidIndex(Index) || State->SkillBought[Index]) return;

	const FName Row = State->SkillOffers[Index];
	const int32 Price = State->SkillPrices[Index];
	if (PS->GetRunState().Currency < Price)
	{
		SendShop(PS, TEXT("던전 재화가 부족합니다."));
		return;
	}
	if (!PS->EquipEnhanceSkill(Row, ReplaceSlot))
	{
		SendShop(PS, TEXT("강화 스킬 칸이 가득 찼습니다. 바꿀 칸을 고르세요."));
		return;
	}

	PS->AddCurrency(-Price);
	State->SkillBought[Index] = true;
	SendShop(PS, FString::Printf(TEXT("구매: %s"), *UTerminusDataSettings::GetSkillName(Row)));
}

void ADungeonArea::HandleShopSellRelic(ATerminusPlayerState* PS, FName RelicRow)
{
	if (!bShopping || !PS || ShopDone.Contains(PS) || !ShopStates.Contains(PS)) return;

	const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(RelicRow);
	if (!Relic || !PS->GetRelics().Contains(RelicRow)) return;
	if (Relic->RelicTier == ERelicTier::Basic || !Relic->CanSellInShop())
	{
		SendShop(PS, TEXT("이 유물은 팔 수 없습니다."));
		return;
	}

	PS->RemoveRelic(RelicRow);
	PS->AddCurrency(Relic->SellPrice_Dungeon);
	PS->MarkRelicsSeen({ RelicRow });   // 판 유물은 이번 런엔 다시 안 나옴
	SendShop(PS, FString::Printf(TEXT("판매: %s (+%d)"), *Relic->RelicName.ToString(), Relic->SellPrice_Dungeon));
}

void ADungeonArea::HandleShopUpgradeSkill(ATerminusPlayerState* PS, FName SkillRow)
{
	if (!bShopping || !PS || ShopDone.Contains(PS) || !ShopStates.Contains(PS)) return;
	if (!PS->GetRunState().EnhanceSkills.Contains(SkillRow)) return;

	// 노션(10-09): 같은 계층 + 같은 직업 또는 shared 인 픽업 스킬만 강화
	const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(SkillRow);
	const ESkillOwner MyOwner = EventSkillOwnerOf(PS->GetCharacterClass());
	if (!Skill || Skill->SkillTier != EventSkillTierOf(EventCurrentFloor(this))
		|| (Skill->OwnerClass != MyOwner && Skill->OwnerClass != ESkillOwner::Shared))
	{
		SendShop(PS, TEXT("이 상점에서는 지금 계층의 내 직업 / 공용 스킬만 강화할 수 있습니다."));
		return;
	}

	const int32 Level = PS->GetPickupSkillLevel(SkillRow);
	if (Level >= ATerminusPlayerState::MaxPickupSkillLevel)
	{
		SendShop(PS, TEXT("이미 최대로 강화했습니다."));
		return;
	}

	const int32 Cost = ShopUpgradeCostPerLevel * (Level + 1);
	if (PS->GetRunState().Currency < Cost)
	{
		SendShop(PS, TEXT("던전 재화가 부족합니다."));
		return;
	}

	PS->UpgradePickupSkill(SkillRow);
	PS->AddCurrency(-Cost);
	SendShop(PS, FString::Printf(TEXT("강화: %s +%d"), *UTerminusDataSettings::GetSkillName(SkillRow), Level + 1));
}

void ADungeonArea::HandleShopLeave(ATerminusPlayerState* PS)
{
	if (!bShopping || !PS || !Occupants.Contains(PS) || ShopDone.Contains(PS)) return;

	ShopDone.Add(PS);
	if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
	{
		PC->Client_CloseShop();
	}
	CheckShopDone();
}

void ADungeonArea::CheckShopDone()
{
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && !ShopDone.Contains(PS)) return;
	}
	GetWorldTimerManager().SetTimer(ShopTimer, this, &ADungeonArea::FinishShop, 0.5f, false);
}

void ADungeonArea::FinishShop()
{
	if (!bShopping) return;
	MarkCleared();
}

void ADungeonArea::MarkCleared()
{
	if (!HasAuthority() || !bInUse || bCleared) return;

	bCleared = true;
	GiveGuardianWinBonus();

	if (!Room.SpecialRoom.IsNone())
	{
		for (ATerminusPlayerState* PS : Occupants)
		{
			TerminusQuests::OnSpecialRoomCleared(PS, Room.SpecialRoom);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] %d번 방 클리어"), AreaIndex, Room.RoomId);

	if (UDungeonAreaSubsystem* Subsystem = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
	{
		Subsystem->NotifyAreaCleared(this);
	}
}

bool ADungeonArea::IsFightOver() const
{
	if (bCleared) return true;
	if (bWiped) return false;

	const ECombatPhase Phase = Combat ? Combat->GetPhase() : ECombatPhase::None;
	return Phase == ECombatPhase::Victory;
}

void ADungeonArea::MarkWiped()
{
	if (!HasAuthority() || !bInUse || bCleared || bWiped) return;

	bWiped = true;
	UE_LOG(LogTemp, Log, TEXT("[Area %d] %d번 방 전멸"), AreaIndex, Room.RoomId);

	if (UDungeonAreaSubsystem* Subsystem = GetWorld()->GetSubsystem<UDungeonAreaSubsystem>())
	{
		Subsystem->NotifyAreaWiped(this);
	}
}

void ADungeonArea::BeginBetrayal(const TArray<ATerminusPlayerState*>& Team, ATerminusPlayerState* Betrayer, const UDungeonThemeData* Theme, float VictimHealthCut, int32 StatPerOpponent)
{
	if (!HasAuthority() || !Betrayer) return;

	Room = FRoomNode();
	Room.RoomId = INDEX_NONE;
	Room.Row = INDEX_NONE;
	Room.Type = ERoomType::BOSS;   // 무대는 보스방 것
	bInUse = true;
	bCleared = false;
	bWiped = false;
	Occupants.Reset();
	ReturnLocations.Reset();

	if (Theme)
	{
		if (TSubclassOf<ADungeonAreaSet> SetClass = Theme->PickAreaSet(ERoomType::BOSS))
		{
			CurrentSetClass = SetClass;
			OnRep_CurrentSetClass();
		}
	}

	// 순서 = 전투 Players 순서 = 대상 인덱스: 팀 먼저, 배신자 마지막
	auto Place = [this](ATerminusPlayerState* PS, const FVector& Location)
	{
		Occupants.Add(PS);
		if (APawn* Pawn = PS->GetPawn())
		{
			ReturnLocations.Add(Pawn, Pawn->GetActorLocation());
			Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		PS->SetCurrentArea(this);
	};

	int32 SlotIndex = 0;
	for (ATerminusPlayerState* PS : Team)
	{
		if (PS && PS != Betrayer) Place(PS, GetPlayerSlotLocation(SlotIndex++));
	}
	Place(Betrayer, GetMonsterSlotLocation(0));   // 배신자는 맞은편 (몬스터 자리)

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 배신 전투: %s vs %d명"), AreaIndex, *Betrayer->GetPlayerName(), Occupants.Num() - 1);

	if (Combat)
	{
		TArray<ATerminusPlayerState*> Players;
		for (ATerminusPlayerState* PS : Occupants) Players.Add(PS);
		Combat->StartBetrayal(Players, Betrayer, VictimHealthCut, StatPerOpponent);
	}
}

void ADungeonArea::BeginIntervention(const TArray<ATerminusPlayerState*>& Joiners)
{
	if (!HasAuthority() || !bInUse || !bWiped) return;

	bWiped = false;

	for (ATerminusPlayerState* PS : Joiners)
	{
		if (!PS || Occupants.Contains(PS)) continue;

		const int32 SlotIndex = Occupants.Num();
		Occupants.Add(PS);

		if (APawn* Pawn = PS->GetPawn())
		{
			ReturnLocations.Add(Pawn, Pawn->GetActorLocation());
			Pawn->SetActorLocation(GetPlayerSlotLocation(SlotIndex), false, nullptr, ETeleportType::TeleportPhysics);
		}
		PS->SetCurrentArea(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 난입: %d명 합류"), AreaIndex, Joiners.Num());

	if (Combat)
	{
		Combat->ResumeWithPlayers(Joiners);
	}
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

	// 상점 화면 닫기
	if (bShopping)
	{
		GetWorldTimerManager().ClearTimer(ShopTimer);
		for (ATerminusPlayerState* PS : Occupants)
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_CloseShop();
			}
		}
		bShopping = false;
		ShopStates.Reset();
		ShopDone.Reset();
	}

	// 이벤트 화면 닫기
	if (bInEvent)
	{
		GetWorldTimerManager().ClearTimer(EventTimer);
		for (ATerminusPlayerState* PS : Occupants)
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_CloseEvent();
			}
		}
		bInEvent = false;
		EventOffers.Reset();
		EventChosen.Reset();
	}
	CurrentEvent = NAME_None;
	EventGuardianHealth = 0.f;
	EventGuardianBonus.Reset();

	// 퀘스트 방 화면 닫기
	if (bInQuest)
	{
		for (ATerminusPlayerState* PS : Occupants)
		{
			if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
			{
				PC->Client_CloseEvent();
			}
		}
		bInQuest = false;
		QuestOffers.Reset();
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
	bWiped = false;
}
