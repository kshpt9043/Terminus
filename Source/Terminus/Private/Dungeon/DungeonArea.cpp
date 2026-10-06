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
#include "Kismet/GameplayStatics.h"
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
	//  - 이벤트: 각자 후보 3개 중 하나 고르기 (전원이 고르면 MarkCleared)
	//  - TODO 상점 / 퀘스트: 해당 UI. 지금은 DebugClearArea 로 넘김
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

// =====================================================================
// 이벤트
// 기획 '이벤트 방 밸런싱'의 보상 중 3개를 뽑아 각자 하나를 고름 (사용자 결정 2026-10-06)
// 쉬는 가디언 전투는 일단 뺌. 테마 유물은 유물 데이터에 테마 칸이 없어 아직 없음
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

	bool IsEventSwappableTier(ERelicTier Tier)
	{
		return Tier != ERelicTier::Basic && Tier != ERelicTier::Upgrade;
	}

	// OldRelic 을 바꿀 수 있는 유물: 같은 계층, 공용이거나 내 직업, 아직 없는 것
	TArray<FName> EventSwapPool(const ATerminusPlayerState* PS, FName OldRelic)
	{
		TArray<FName> Pool;
		const FRelicRow* Old = UTerminusDataSettings::FindRelicRow(OldRelic);
		const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous();
		if (!PS || !Old || !Table || !IsEventSwappableTier(Old->RelicTier)) return Pool;

		for (const FName& Row : Table->GetRowNames())
		{
			const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
			if (!Relic || Relic->RelicTier != Old->RelicTier) continue;
			if (!ATerminusPlayerState::CanClassHoldRelic(PS->GetCharacterClass(), *Relic)) continue;
			if (PS->GetRelics().Contains(Row)) continue;
			Pool.Add(Row);
		}
		return Pool;
	}

	// 바꿀 수 있는 내 유물 (기본 / 업그레이드 유물 제외, 바꿀 후보가 있는 것만)
	TArray<FName> EventSwappableRelics(const ATerminusPlayerState* PS)
	{
		TArray<FName> Result;
		if (!PS) return Result;

		for (const FName& Owned : PS->GetRelics())
		{
			if (EventSwapPool(PS, Owned).Num() > 0) Result.Add(Owned);
		}
		return Result;
	}

	FString EventRelicName(FName Row)
	{
		const FRelicRow* Relic = UTerminusDataSettings::FindRelicRow(Row);
		return Relic ? Relic->RelicName.ToString() : Row.ToString();
	}
}

void ADungeonArea::BeginEvent()
{
	bInEvent = true;
	EventOffers.Reset();
	EventChosen.Reset();

	for (ATerminusPlayerState* PS : Occupants)
	{
		if (!PS) continue;

		const TArray<FEventOption> Options = MakeEventOptions(PS);
		EventOffers.Add(PS, Options);

		// 받을 게 없으면 고른 걸로 (화면엔 '아무 일도 일어나지 않았습니다')
		if (Options.Num() == 0) EventChosen.Add(PS);

		if (ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(PS->GetOwner()))
		{
			PC->Client_ShowEvent(Options);
		}
	}

	CheckEventDone();
}

TArray<FEventOption> ADungeonArea::MakeEventOptions(const ATerminusPlayerState* PS) const
{
	TArray<FEventOption> Candidates;
	if (!PS) return Candidates;

	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(this, AMapManager::StaticClass()));
	const int32 Floor = MapMgr ? MapMgr->CurrentFloor : 1;
	const ESkillTier MaxTier = Floor >= 5 ? ESkillTier::Deep : (Floor >= 3 ? ESkillTier::Mid : ESkillTier::Surface);
	const ESkillOwner MyOwner = EventSkillOwnerOf(PS->GetCharacterClass());
	const TArray<FName> Equipped = PS->GetRunState().EnhanceSkills;

	// ---- 스킬 풀: 내 직업 / 공용 픽업, 다른 직업 픽업, 이벤트 (기본 / 몬스터 스킬, 지금 계층보다 높은 것, 이미 장착한 것 제외)
	TArray<FName> ClassPool, OtherPool, EventPool;
	if (const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous())
	{
		for (const FName& Row : Table->GetRowNames())
		{
			const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
			if (!Skill || !UTerminusProfileSubsystem::IsOwnableSkill(Row)) continue;
			if (Skill->SkillTier > MaxTier || Equipped.Contains(Row)) continue;

			if (Skill->SkillCategory == ESkillCategory::Event)
			{
				EventPool.Add(Row);
			}
			else if (Skill->SkillEnergyCost > 0)   // 픽업 스킬 = 스킬 에너지를 쓰는 스킬
			{
				if (Skill->SkillCategory == ESkillCategory::Personal && Skill->OwnerClass != MyOwner && Skill->OwnerClass != ESkillOwner::Shared)
				{
					OtherPool.Add(Row);
				}
				else if (UTerminusProfileSubsystem::IsEquippableSkill(Row, PS->GetCharacterClass()))
				{
					ClassPool.Add(Row);
				}
			}
		}
	}

	auto AddSkill = [&Candidates](EEventOptionType Type, const TArray<FName>& Pool, const FString& Guide)
	{
		if (Pool.Num() == 0) return;

		const FName Row = EventPickRandom(Pool);
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);

		FEventOption Option;
		Option.Type = Type;
		Option.SkillRow = Row;
		Option.Title = Skill->DisplayName_KR;
		Option.Description = FText::FromString(FString::Printf(TEXT("%s\n(강화 에너지 %d)"), *Guide, Skill->SkillEnergyCost));
		Candidates.Add(Option);
	};

	AddSkill(EEventOptionType::ClassSkill, ClassPool, TEXT("스킬을 배웁니다."));
	if (OtherPool.Num() > 0)
	{
		const FSkillRow* Sample = nullptr;   // 설명에 직업 이름을 넣으려고 미리 고름
		const FName Row = EventPickRandom(OtherPool);
		Sample = UTerminusDataSettings::FindSkillRow(Row);
		AddSkill(EEventOptionType::OtherClassSkill, TArray<FName>{ Row },
			FString::Printf(TEXT("다른 직업(%s)의 스킬을 배웁니다."), *UEnum::GetDisplayValueAsText(Sample->OwnerClass).ToString()));
	}
	AddSkill(EEventOptionType::EventSkill, EventPool, TEXT("이벤트 스킬을 배웁니다."));

	// ---- 던전 재화
	{
		FEventOption Option;
		Option.Type = EEventOptionType::Currency;
		Option.Amount = FMath::RandRange(FMath::Min(EventCurrency.X, EventCurrency.Y), FMath::Max(EventCurrency.X, EventCurrency.Y));
		Option.Title = FText::FromString(FString::Printf(TEXT("던전 재화 +%d"), Option.Amount));
		Option.Description = FText::FromString(TEXT("던전 재화를 얻습니다."));
		Candidates.Add(Option);
	}

	// ---- 이번 런 동안 스텟 (셋 중 하나)
	{
		FEventOption Option;
		Option.Type = EEventOptionType::PermanentStat;
		Option.StatKind = FMath::RandRange(0, 2);
		Option.Amount = Option.StatKind == 0 ? EventPermanentHealth : (Option.StatKind == 1 ? EventPermanentAttack : EventPermanentDefense);
		const TCHAR* StatName = Option.StatKind == 0 ? TEXT("최대 체력") : (Option.StatKind == 1 ? TEXT("공격력") : TEXT("방어력"));
		Option.Title = FText::FromString(FString::Printf(TEXT("%s +%d"), StatName, Option.Amount));
		Option.Description = FText::FromString(TEXT("이번 던전이 끝날 때까지 유지됩니다."));
		if (Option.Amount > 0) Candidates.Add(Option);
	}

	// ---- 일시 버프 (공용: 대가 있음 / 테마: 대가 없음)
	{
		FEventOption Option;
		Option.Type = EEventOptionType::TempStat;
		Option.Amount = EventTempAttack;
		Option.Penalty = EventTempDefensePenalty;
		Option.Battles = EventTempBattles;
		Option.Title = FText::FromString(FString::Printf(TEXT("공격력 +%d (전투 %d번)"), Option.Amount, Option.Battles));
		Option.Description = FText::FromString(Option.Penalty > 0
			? FString::Printf(TEXT("다음 전투 %d번 동안 공격력 +%d, 방어력 -%d."), Option.Battles, Option.Amount, Option.Penalty)
			: FString::Printf(TEXT("다음 전투 %d번 동안 공격력 +%d."), Option.Battles, Option.Amount));
		if (Option.Amount > 0) Candidates.Add(Option);
	}
	{
		const FString ThemeName = MapMgr && MapMgr->FloorTheme && !MapMgr->FloorTheme->DisplayName.IsEmpty()
			? MapMgr->FloorTheme->DisplayName.ToString()
			: FString(TEXT("이곳"));

		FEventOption Option;
		Option.Type = EEventOptionType::ThemeTempStat;
		Option.Amount = EventThemeAttack;
		Option.Battles = EventTempBattles;
		Option.Title = FText::FromString(FString::Printf(TEXT("%s의 기운"), *ThemeName));
		Option.Description = FText::FromString(FString::Printf(TEXT("다음 전투 %d번 동안 공격력 +%d."), Option.Battles, Option.Amount));
		if (Option.Amount > 0) Candidates.Add(Option);
	}

	// ---- 유물 변화 (바꿀 수 있는 유물이 있을 때만)
	if (EventSwappableRelics(PS).Num() > 0)
	{
		FEventOption Random;
		Random.Type = EEventOptionType::RelicSwapRandom;
		Random.Title = FText::FromString(TEXT("유물 변화 (무작위)"));
		Random.Description = FText::FromString(TEXT("내 유물 하나가 무작위로\n같은 계층의 다른 유물로 바뀝니다."));
		Candidates.Add(Random);

		FEventOption Chosen;
		Chosen.Type = EEventOptionType::RelicSwapChosen;
		Chosen.Title = FText::FromString(TEXT("유물 변화 (선택)"));
		Chosen.Description = FText::FromString(TEXT("고른 유물 하나를\n같은 계층의 다른 유물로 바꿉니다."));
		Candidates.Add(Chosen);
	}

	// 섞어서 앞에서부터 (종류가 겹치지 않음)
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, FMath::RandRange(0, i));
	}
	Candidates.SetNum(FMath::Min(Candidates.Num(), FMath::Max(1, EventOptionCount)));
	return Candidates;
}

bool ADungeonArea::ApplyEventOption(ATerminusPlayerState* PS, const FEventOption& Option, int32 ReplaceSlot, FName RelicRow, FString& OutResult)
{
	switch (Option.Type)
	{
	case EEventOptionType::ClassSkill:
	case EEventOptionType::OtherClassSkill:
	case EEventOptionType::EventSkill:
	{
		if (!PS->EquipEnhanceSkill(Option.SkillRow, ReplaceSlot)) return false;
		OutResult = FString::Printf(TEXT("스킬 획득: %s"), *Option.Title.ToString());
		return true;
	}

	case EEventOptionType::Currency:
		PS->AddCurrency(Option.Amount);
		OutResult = FString::Printf(TEXT("던전 재화 +%d"), Option.Amount);
		return true;

	case EEventOptionType::PermanentStat:
		PS->ApplyPermanentStat(Option.StatKind, Option.Amount);
		OutResult = Option.Title.ToString();
		return true;

	case EEventOptionType::TempStat:
	case EEventOptionType::ThemeTempStat:
	{
		FTempStatBuff Buff;
		Buff.Attack = Option.Amount;
		Buff.Defense = -Option.Penalty;
		Buff.BattlesLeft = Option.Battles;
		Buff.Source = Option.Title.ToString();
		PS->AddTempBuff(Buff);
		OutResult = Option.Description.ToString();
		return true;
	}

	case EEventOptionType::RelicSwapRandom:
	case EEventOptionType::RelicSwapChosen:
	{
		const TArray<FName> Swappable = EventSwappableRelics(PS);
		if (Swappable.Num() == 0) return false;

		const FName Old = Option.Type == EEventOptionType::RelicSwapChosen ? RelicRow : EventPickRandom(Swappable);
		if (!Swappable.Contains(Old)) return false;

		const FName New = EventPickRandom(EventSwapPool(PS, Old));
		if (!PS->SwapRelic(Old, New)) return false;

		OutResult = FString::Printf(TEXT("유물 변화: %s → %s"), *EventRelicName(Old), *EventRelicName(New));
		return true;
	}
	}

	return false;
}

void ADungeonArea::HandleEventChoice(ATerminusPlayerState* Chooser, int32 Index, int32 ReplaceSlot, FName RelicRow)
{
	if (!HasAuthority() || !bInEvent || !Chooser) return;
	if (!Occupants.Contains(Chooser) || EventChosen.Contains(Chooser)) return;

	const TArray<FEventOption>* Offers = EventOffers.Find(Chooser);
	if (!Offers) return;

	ATerminusPlayerController* PC = Cast<ATerminusPlayerController>(Chooser->GetOwner());

	FString Result;
	if (!Offers->IsValidIndex(Index) || !ApplyEventOption(Chooser, (*Offers)[Index], ReplaceSlot, RelicRow, Result))
	{
		// 잘못 골랐거나(칸 / 유물이 그새 바뀜) 적용 실패: 화면을 처음으로 돌려 다시 고르게
		UE_LOG(LogTemp, Warning, TEXT("[Area %d] 이벤트: %s 의 선택(%d) 적용 실패. 다시 고르게 함"), AreaIndex, *Chooser->GetPlayerName(), Index);
		if (PC) PC->Client_ShowEvent(*Offers);
		return;
	}

	EventChosen.Add(Chooser);
	if (PC) PC->Client_EventResult(FText::FromString(Result));

	if (Occupants.Num() > 1)
	{
		FChatMessage Notice;
		Notice.Kind = EChatMessageKind::System;
		Notice.Text = FString::Printf(TEXT("[이벤트] %s 님: %s"), *Chooser->GetPlayerName(), *Result);
		ATerminusPlayerController::BroadcastChat(GetWorld(), Notice);
	}

	UE_LOG(LogTemp, Log, TEXT("[Area %d] 이벤트: %s -> %s"), AreaIndex, *Chooser->GetPlayerName(), *Result);

	CheckEventDone();
}

void ADungeonArea::CheckEventDone()
{
	for (ATerminusPlayerState* PS : Occupants)
	{
		if (PS && !EventChosen.Contains(PS)) return;
	}

	// 전원이 골랐으면 결과를 잠깐 보여주고 끝
	GetWorldTimerManager().SetTimer(EventTimer, this, &ADungeonArea::FinishEvent, 1.5f, false);
}

void ADungeonArea::FinishEvent()
{
	if (!bInEvent) return;
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
