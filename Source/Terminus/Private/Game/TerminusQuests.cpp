#include "Game/TerminusQuests.h"

#include "Data/TerminusDataSettings.h"
#include "Dungeon/DungeonThemeData.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Game/TerminusRunSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TerminusPlayerController.h"
#include "Player/TerminusPlayerState.h"

namespace
{
	// 퀘스트 알림은 그 사람에게만 (멀티에서 남의 퀘스트는 몰라도 됨)
	void Tell(ATerminusPlayerState* PS, const FString& Text)
	{
		if (ATerminusPlayerController* PC = PS ? Cast<ATerminusPlayerController>(PS->GetOwner()) : nullptr)
		{
			FChatMessage Notice;
			Notice.Kind = EChatMessageKind::System;
			Notice.Text = Text;
			PC->Client_ReceiveChat(Notice);
		}
		UE_LOG(LogTemp, Log, TEXT("[Quest] %s: %s"), PS ? *PS->GetPlayerName() : TEXT("?"), *Text);
	}

	const FQuestRow* ActiveRow(const ATerminusPlayerState* PS)
	{
		const FQuestProgress& Q = PS->GetRunState().ActiveQuest;
		return Q.Quest.IsNone() || Q.bCompleted ? nullptr : UTerminusDataSettings::FindQuestRow(Q.Quest);
	}

	FString FactionOf(const ATerminusPlayerState* PS)
	{
		const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(PS->GetCharacterClass());
		return Row ? GetFactionName(Row->Faction).ToString() : FString();
	}

	// "2층" -> 2, "심층" -> 0. 1 / 2 는 테마 안의 층, 3 이상은 그 층 그대로 (몬스터 AppearFloor 와 같은 읽는 법)
	bool MatchesFloor(int32 N, int32 Floor)
	{
		const int32 ThemeFloor = AMapManager::IsThemeEndFloor(Floor) ? 2 : 1;
		return N > 0 && (N == Floor || N == ThemeFloor);
	}

	void GiveReward(ATerminusPlayerState* PS, EQuestRewardType Type, FName Target, int32 Value, TArray<FString>& OutLines)
	{
		switch (Type)
		{
		case EQuestRewardType::GiveCurrency:
			if (Target == TEXT("Gold"))
			{
				PS->ModifyRunState([Value](FRunState& S) { S.QuestGold += Value; });
				OutLines.Add(FString::Printf(TEXT("골드 %d (정산 때)"), Value));
			}
			else
			{
				PS->AddCurrency(Value);
				OutLines.Add(FString::Printf(TEXT("던전 재화 +%d"), Value));
			}
			break;

		case EQuestRewardType::GiveRelic:
			PS->MarkRelicsSeen({ Target });
			OutLines.Add(PS->GainRelic(Target)
				? FString::Printf(TEXT("유물 [%s]"), *UTerminusDataSettings::GetRelicName(Target))
				: FString::Printf(TEXT("유물 [%s] 은(는) 유물 칸이 가득 차 받지 못했습니다"), *UTerminusDataSettings::GetRelicName(Target)));
			break;

		case EQuestRewardType::GiveSkill:
			OutLines.Add(PS->EquipEnhanceSkill(Target)
				? FString::Printf(TEXT("스킬 [%s]"), *UTerminusDataSettings::GetSkillName(Target))
				: FString::Printf(TEXT("스킬 [%s] 은(는) 강화 칸이 가득 차 받지 못했습니다"), *UTerminusDataSettings::GetSkillName(Target)));
			break;

		case EQuestRewardType::UpgradeSkill:
			for (int32 i = 0; i < FMath::Max(1, Value); ++i)
			{
				TArray<FName> Candidates;
				for (const FName& Skill : PS->GetRunState().EnhanceSkills)
				{
					if (PS->GetPickupSkillLevel(Skill) < ATerminusPlayerState::MaxPickupSkillLevel) Candidates.AddUnique(Skill);
				}
				if (Candidates.Num() == 0)
				{
					OutLines.Add(TEXT("강화할 스킬이 없습니다"));
					break;
				}
				const FName Skill = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
				PS->UpgradePickupSkill(Skill);
				OutLines.Add(FString::Printf(TEXT("%s +%d"), *UTerminusDataSettings::GetSkillName(Skill), PS->GetPickupSkillLevel(Skill)));
			}
			break;

		default:
			break;
		}
	}

	// 진행도를 올리고(또는 정하고) 다 채우면 완료
	void SetProgress(ATerminusPlayerState* PS, int32 NewProgress)
	{
		const FQuestRow* Quest = ActiveRow(PS);
		if (!Quest) return;

		const int32 Goal = FMath::Max(1, Quest->ConditionValue);
		const int32 Clamped = FMath::Clamp(NewProgress, 0, Goal);
		if (Clamped == PS->GetRunState().ActiveQuest.Progress) return;

		const bool bDone = Clamped >= Goal;
		PS->ModifyRunState([Clamped, bDone](FRunState& S)
		{
			S.ActiveQuest.Progress = Clamped;
			S.ActiveQuest.bCompleted = bDone;
		});

		if (!bDone)
		{
			Tell(PS, FString::Printf(TEXT("[퀘스트] %s %d / %d"), *Quest->QuestName.ToString(), Clamped, Goal));
			return;
		}

		TArray<FString> Lines;
		GiveReward(PS, Quest->ClearRewardType, Quest->ClearRewardTargetID, Quest->ClearRewardValue, Lines);
		Tell(PS, FString::Printf(TEXT("[퀘스트 완료] %s — %s"), *Quest->QuestName.ToString(), *FString::Join(Lines, TEXT(", "))));
	}

	void AddProgress(ATerminusPlayerState* PS, int32 Delta)
	{
		if (Delta > 0) SetProgress(PS, PS->GetRunState().ActiveQuest.Progress + Delta);
	}

	FString RoomTypeKey(ERoomType Type)
	{
		switch (Type)
		{
		case ERoomType::MONSTER:  return TEXT("MonsterRoom");
		case ERoomType::GUARDIAN: return TEXT("GuardianRoom");
		case ERoomType::BOSS:     return TEXT("BossRoom");
		case ERoomType::EVENT:    return TEXT("EventRoom");
		case ERoomType::STORE:    return TEXT("StoreRoom");
		case ERoomType::BREAK:    return TEXT("RestRoom");
		default:                  return FString();
		}
	}
}

namespace TerminusQuests
{
	TArray<FName> MakeOffers(const ATerminusPlayerState* PS, int32 Floor, const FString& ThemeName, int32 Count)
	{
		TArray<FName> Out;
		const UDataTable* Table = UTerminusDataSettings::Get()->QuestTable.LoadSynchronous();
		if (!PS || !Table) return Out;

		const FString TierName = AMapManager::GetTierName(Floor).ToString();
		const FString MyFaction = FactionOf(PS);

		TArray<TPair<FName, int32>> Mine, Others;
		for (const FName& Row : Table->GetRowNames())
		{
			const FQuestRow* Quest = UTerminusDataSettings::FindQuestRow(Row);
			if (!Quest || Quest->AppearanceWeight <= 0) continue;
			if (Quest->Theme != ThemeName && Quest->Theme != TierName && Quest->Theme != TEXT("Any")) continue;
			(Quest->Force == MyFaction ? Mine : Others).Emplace(Row, Quest->AppearanceWeight);
		}

		auto PickWeighted = [](TArray<TPair<FName, int32>>& Pool) -> FName
		{
			int32 Total = 0;
			for (const TPair<FName, int32>& P : Pool) Total += P.Value;
			if (Total <= 0) return NAME_None;
			int32 Roll = FMath::RandRange(1, Total);
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				Roll -= Pool[i].Value;
				if (Roll <= 0)
				{
					const FName Picked = Pool[i].Key;
					Pool.RemoveAt(i);
					return Picked;
				}
			}
			return NAME_None;
		};

		// 1개는 내 세력 것 (있으면), 나머지는 남은 것 전부에서
		if (const FName Own = PickWeighted(Mine); !Own.IsNone()) Out.Add(Own);
		Others.Append(Mine);
		while (Out.Num() < Count)
		{
			const FName Next = PickWeighted(Others);
			if (Next.IsNone()) break;
			Out.Add(Next);
		}

		// 내 세력 것이 항상 첫 칸이면 티가 나서 섞음
		for (int32 i = Out.Num() - 1; i > 0; --i) Out.Swap(i, FMath::RandRange(0, i));
		return Out;
	}

	FString DescribeReward(EQuestRewardType Type, FName Target, int32 Value)
	{
		switch (Type)
		{
		case EQuestRewardType::GiveCurrency:
			return Target == TEXT("Gold") ? FString::Printf(TEXT("골드 %d (정산 때)"), Value) : FString::Printf(TEXT("던전 재화 %d"), Value);
		case EQuestRewardType::GiveRelic:    return FString::Printf(TEXT("유물 [%s]"), *UTerminusDataSettings::GetRelicName(Target));
		case EQuestRewardType::GiveSkill:    return FString::Printf(TEXT("스킬 [%s]"), *UTerminusDataSettings::GetSkillName(Target));
		case EQuestRewardType::UpgradeSkill: return FString::Printf(TEXT("픽업 스킬 %d회 강화"), FMath::Max(1, Value));
		default:                             return FString();
		}
	}

	FString DescribeOffer(FName QuestRow)
	{
		const FQuestRow* Quest = UTerminusDataSettings::FindQuestRow(QuestRow);
		if (!Quest) return FString();

		FString Text = Quest->QuestDesc.ToString();
		Text += FString::Printf(TEXT("\n\n%s · 보상: %s"), *Quest->Force, *DescribeReward(Quest->ClearRewardType, Quest->ClearRewardTargetID, Quest->ClearRewardValue));
		if (Quest->AcceptRewardType != EQuestRewardType::None)
		{
			Text += FString::Printf(TEXT("\n수락하면 바로: %s"), *DescribeReward(Quest->AcceptRewardType, Quest->AcceptRewardTargetID, Quest->AcceptRewardValue));
		}
		return Text;
	}

	FString DescribeActive(const FRunState& RunState)
	{
		const FQuestProgress& Q = RunState.ActiveQuest;
		const FQuestRow* Quest = Q.Quest.IsNone() ? nullptr : UTerminusDataSettings::FindQuestRow(Q.Quest);
		if (!Quest) return FString();

		return Q.bCompleted
			? FString::Printf(TEXT("[퀘스트] %s (완료)"), *Quest->QuestName.ToString())
			: FString::Printf(TEXT("[퀘스트] %s %d / %d"), *Quest->QuestName.ToString(), Q.Progress, FMath::Max(1, Quest->ConditionValue));
	}

	void Accept(ATerminusPlayerState* PS, FName QuestRow)
	{
		const FQuestRow* Quest = PS ? UTerminusDataSettings::FindQuestRow(QuestRow) : nullptr;
		if (!Quest || !PS->HasAuthority()) return;

		PS->ModifyRunState([QuestRow](FRunState& S)
		{
			S.ActiveQuest = FQuestProgress();
			S.ActiveQuest.Quest = QuestRow;
		});

		TArray<FString> Lines;
		GiveReward(PS, Quest->AcceptRewardType, Quest->AcceptRewardTargetID, Quest->AcceptRewardValue, Lines);
		Tell(PS, FString::Printf(TEXT("[퀘스트 수락] %s%s"), *Quest->QuestName.ToString(),
			Lines.Num() > 0 ? *FString::Printf(TEXT(" — 선지급: %s"), *FString::Join(Lines, TEXT(", "))) : TEXT("")));

		if (Quest->bInjectSpecialRoom)
		{
			InjectSpecialRooms(PS->GetWorld());
		}

		// 수락할 때 이미 이룬 조건 (지금 층이 목표 층 이상)
		if (Quest->ConditionType == EQuestConditionType::ReachFloor)
		{
			if (const AMapManager* Map = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(PS, AMapManager::StaticClass())))
			{
				OnFloorReached(PS, Map->CurrentFloor);
			}
		}
	}

	void ExpireForNewTheme(ATerminusPlayerState* PS)
	{
		if (!PS || !PS->HasAuthority()) return;

		const FQuestProgress Q = PS->GetRunState().ActiveQuest;
		if (Q.Quest.IsNone()) return;

		if (!Q.bCompleted)
		{
			if (const FQuestRow* Quest = UTerminusDataSettings::FindQuestRow(Q.Quest))
			{
				Tell(PS, FString::Printf(TEXT("[퀘스트 실패] %s — 테마가 바뀌어 사라졌습니다."), *Quest->QuestName.ToString()));
			}
		}
		PS->ModifyRunState([](FRunState& S) { S.ActiveQuest = FQuestProgress(); });
	}

	void OnMonstersKilled(ATerminusPlayerState* PS, const TArray<FName>& MonsterRows)
	{
		const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
		if (!Quest || Quest->ConditionType != EQuestConditionType::KillMonster) return;

		// 대상: MonsterID (M_SLI_B02) 또는 분류 (Guardian / Boss / Normal)
		const FString Target = Quest->ConditionTargetID.ToString();
		int32 Count = 0;
		for (const FName& Row : MonsterRows)
		{
			const FMonsterRow* Monster = UTerminusDataSettings::FindMonsterRow(Row);
			if (!Monster) continue;
			const FString Category = StaticEnum<EMonsterCategory>()->GetNameStringByValue(static_cast<int64>(Monster->MonsterCategory));
			if (Monster->MonsterID == Target || Category == Target || Row.ToString() == Target) ++Count;
		}
		AddProgress(PS, Count);
	}

	void OnRoomEntered(ATerminusPlayerState* PS, ERoomType Type)
	{
		if (!PS || !PS->HasAuthority()) return;

		// 한 층 안에서 방에 들어간 수 (BeginFloor 에서 0 으로)
		PS->ModifyRunState([Type](FRunState& S)
		{
			if (Type == ERoomType::MONSTER) ++S.FloorMonsterRooms;
			if (Type == ERoomType::GUARDIAN) ++S.FloorGuardianRooms;
		});

		const FQuestRow* Quest = ActiveRow(PS);
		if (!Quest || Quest->ConditionType != EQuestConditionType::EnterRoomInSingleFloor) return;
		if (Quest->ConditionTargetID.ToString() != RoomTypeKey(Type)) return;

		const FRunState& S = PS->GetRunState();
		SetProgress(PS, Type == ERoomType::GUARDIAN ? S.FloorGuardianRooms : S.FloorMonsterRooms);
	}

	void OnRelicGained(ATerminusPlayerState* PS)
	{
		const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
		if (Quest && Quest->ConditionType == EQuestConditionType::AcquireRelic) AddProgress(PS, 1);
	}

	void OnCurrencyEarned(ATerminusPlayerState* PS, int32 Amount)
	{
		const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
		if (Quest && Quest->ConditionType == EQuestConditionType::EarnCurrency) AddProgress(PS, Amount);
	}

	void OnFloorReached(ATerminusPlayerState* PS, int32 Floor)
	{
		const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
		if (!Quest || Quest->ConditionType != EQuestConditionType::ReachFloor) return;

		const int32 Target = FCString::Atoi(*Quest->ConditionTargetID.ToString());
		if (Target > 0 && Floor >= Target) SetProgress(PS, Quest->ConditionValue);
	}

	void OnSpecialRoomCleared(ATerminusPlayerState* PS, FName SpecialRoom)
	{
		const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
		if (!Quest || Quest->ConditionType != EQuestConditionType::CompleteSpecialRoom) return;
		if (Quest->ConditionTargetID == SpecialRoom) AddProgress(PS, 1);
	}

	bool InjectSpecialRooms(UWorld* World)
	{
		AMapManager* Map = World ? Cast<AMapManager>(UGameplayStatics::GetActorOfClass(World, AMapManager::StaticClass())) : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!Map || !GS || !Map->HasAuthority()) return false;

		const FString ThemeName = Map->FloorTheme ? Map->FloorTheme->DisplayName.ToString() : FString();
		const int32 Floor = Map->CurrentFloor;

		int32 LastRow = 0;
		for (const FRoomNode& Node : Map->Rooms) LastRow = FMath::Max(LastRow, Node.Row);
		const int32 FirstRow = AMapManager::HasQuestRoom(Floor) ? 1 : 0;

		bool bChanged = false;
		for (APlayerState* Base : GS->PlayerArray)
		{
			const ATerminusPlayerState* PS = Cast<ATerminusPlayerState>(Base);
			const FQuestRow* Quest = PS ? ActiveRow(PS) : nullptr;
			if (!Quest || !Quest->bInjectSpecialRoom || Quest->SpecialRoomID.IsNone()) continue;

			const FQuestRoomRow* Special = UTerminusDataSettings::FindQuestRoomRow(Quest->SpecialRoomID);
			if (!Special || Special->Theme != ThemeName || !MatchesFloor(Special->Floor, Floor)) continue;
			if (Map->Rooms.ContainsByPredicate([&Quest](const FRoomNode& N) { return N.SpecialRoom == Quest->SpecialRoomID; })) continue;

			// Depth = 12칸 지도 기준 레벨 (1 = 퀘스트 줄, 12 = 보스). 퀘스트 줄이 없는 층은 한 칸 당겨짐
			const int32 Row = Special->Depth >= 12 ? LastRow
				: FMath::Clamp(Special->Depth - (AMapManager::HasQuestRoom(Floor) ? 1 : 2), FirstRow, LastRow);

			TArray<int32> Candidates;
			for (int32 i = 0; i < Map->Rooms.Num(); ++i)
			{
				const FRoomNode& N = Map->Rooms[i];
				if (N.Row == Row && N.Type != ERoomType::QUEST && N.SpecialRoom.IsNone()) Candidates.Add(i);
			}
			if (Candidates.Num() == 0) continue;

			FRoomNode& Node = Map->Rooms[Candidates[FMath::RandRange(0, Candidates.Num() - 1)]];
			Node.SpecialRoom = Quest->SpecialRoomID;
			if (Node.Type != ERoomType::BOSS) Node.Type = ERoomType::GUARDIAN;   // 특수 방 = 지정 몬스터와 싸우는 방
			bChanged = true;

			UE_LOG(LogTemp, Log, TEXT("[Quest] 특수 방 %s -> %d층 %d줄 %d번 방"), *Quest->SpecialRoomID.ToString(), Floor, Row, Node.RoomId);
		}

		if (bChanged)
		{
			if (UTerminusRunSubsystem* Run = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>() : nullptr)
			{
				Run->SetRooms(Map->Rooms);
			}
			Map->OnMapGenerated.Broadcast(Map->Rooms);   // 리슨 서버 호스트 (손님은 Rooms 복제)
		}
		return bChanged;
	}

	TArray<FName> GetSpecialRoomMonsters(FName SpecialRoom)
	{
		TArray<FName> Out;
		const FQuestRoomRow* Special = UTerminusDataSettings::FindQuestRoomRow(SpecialRoom);
		const UDataTable* Table = UTerminusDataSettings::Get()->MonsterTable.LoadSynchronous();
		if (!Special || !Table) return Out;

		// SpawnMonster 는 MonsterID (M_SLI_G03) -> 행 이름으로
		FName Row = NAME_None;
		Table->ForeachRow<FMonsterRow>(TEXT("GetSpecialRoomMonsters"), [&](const FName& Key, const FMonsterRow& Monster)
		{
			if (Row.IsNone() && (Monster.MonsterID == Special->SpawnMonster.ToString() || Key == Special->SpawnMonster)) Row = Key;
		});
		for (int32 i = 0; i < FMath::Max(1, Special->SpawnValue) && !Row.IsNone(); ++i) Out.Add(Row);
		return Out;
	}
}
