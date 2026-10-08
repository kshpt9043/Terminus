#include "Game/TerminusSaveSubsystem.h"

#include "Character/TerminusBattler.h"
#include "Combat/CombatStatsComponent.h"
#include "Data/TerminusDataSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/DungeonGameMode.h"
#include "Dungeon/DungeonThemeData.h"
#include "Game/LoadingScreenSubsystem.h"
#include "Game/TerminusSaveCrypto.h"
#include "Game/TerminusRunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Online/SessionSubsystem.h"
#include "Player/TerminusPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerminusSave, Log, All);

namespace
{
	const FString RunSaveIndexSlot = TEXT("RunSaveIndex");
	constexpr int32 RunSaveUserIndex = 0;
}

const FString UTerminusSaveSubsystem::SingleRunSlot = TEXT("SingleRun");

// [테스트] 싱글 세이브 지우기. 정산 / 사망이 아직 없어서 싱글 런을 끝낼 방법이 없을 때
static FAutoConsoleCommandWithWorld GDeleteSingleSaveCommand(
	TEXT("Terminus.DeleteSingleSave"), TEXT("진행 중인 싱글 세이브 삭제 (새 싱글 게임을 시작할 수 있게)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UTerminusSaveSubsystem* Save = UTerminusSaveSubsystem::Get(World))
		{
			Save->DeleteSingleRunSaves();
		}
	}));

UTerminusSaveSubsystem* UTerminusSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UTerminusSaveSubsystem>() : nullptr;
}

FString UTerminusSaveSubsystem::MakeNewSlotName()
{
	return FString::Printf(TEXT("Run_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
}

// =====================================================================
// 목록 (인덱스)
// =====================================================================

UTerminusRunSaveIndex* UTerminusSaveSubsystem::LoadIndex() const
{
	if (UGameplayStatics::DoesSaveGameExist(RunSaveIndexSlot, RunSaveUserIndex))
	{
		TerminusSaveCrypto::ELoadResult Result = TerminusSaveCrypto::ELoadResult::NotFound;
		if (UTerminusRunSaveIndex* Index = Cast<UTerminusRunSaveIndex>(TerminusSaveCrypto::LoadFromSlot(RunSaveIndexSlot, RunSaveUserIndex, &Result)))
		{
			// 예전 평문이면 바로 암호화해서 다시 씀
			if (Result == TerminusSaveCrypto::ELoadResult::Legacy)
			{
				TerminusSaveCrypto::SaveToSlot(Index, RunSaveIndexSlot, RunSaveUserIndex);
			}
			return Index;
		}
	}
	return Cast<UTerminusRunSaveIndex>(UGameplayStatics::CreateSaveGameObject(UTerminusRunSaveIndex::StaticClass()));
}

void UTerminusSaveSubsystem::SaveIndex(UTerminusRunSaveIndex* Index) const
{
	if (Index)
	{
		TerminusSaveCrypto::SaveToSlot(Index, RunSaveIndexSlot, RunSaveUserIndex);
	}
}

TArray<FRunSaveSummary> UTerminusSaveSubsystem::GetRunSaves(bool bMultiplayerOnly)
{
	UTerminusRunSaveIndex* Index = LoadIndex();
	if (!Index) return {};

	// 파일을 직접 지운 경우 등 -> 목록에서도 뺌
	const int32 Removed = Index->Entries.RemoveAll([](const FRunSaveSummary& Entry)
	{
		return !UGameplayStatics::DoesSaveGameExist(Entry.SlotName, RunSaveUserIndex);
	});
	if (Removed > 0)
	{
		SaveIndex(Index);
	}

	TArray<FRunSaveSummary> Result = Index->Entries;
	if (bMultiplayerOnly)
	{
		Result.RemoveAll([](const FRunSaveSummary& Entry) { return !Entry.bMultiplayer; });
	}
	Result.Sort([](const FRunSaveSummary& A, const FRunSaveSummary& B) { return A.SavedAt > B.SavedAt; });
	return Result;
}

void UTerminusSaveSubsystem::DeleteSingleRunSaves()
{
	FRunSaveSummary Summary;
	while (FindSingleRunSave(Summary))
	{
		DeleteRunSave(Summary.SlotName);
	}
}

bool UTerminusSaveSubsystem::FindSingleRunSave(FRunSaveSummary& OutSummary)
{
	// 가장 최근 싱글 세이브 (예전 방식으로 여러 개 남아 있어도 최신 하나)
	for (const FRunSaveSummary& Entry : GetRunSaves())
	{
		if (!Entry.bMultiplayer)
		{
			OutSummary = Entry;
			return true;
		}
	}
	return false;
}

// =====================================================================
// 저장 / 읽기 / 삭제
// =====================================================================

bool UTerminusSaveSubsystem::SaveCurrentRun(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return false;

	UTerminusRunSubsystem* Run = GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>();
	const AGameStateBase* GS = World->GetGameState();
	if (!Run || !GS) return false;

	UTerminusRunSave* Save = Cast<UTerminusRunSave>(UGameplayStatics::CreateSaveGameObject(UTerminusRunSave::StaticClass()));
	if (!Save) return false;

	// 지도 / 층. 레벨의 MapManager 가 진짜 (런 서브시스템은 그 원본 사본)
	const AMapManager* MapMgr = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(World, AMapManager::StaticClass()));
	Save->Rooms = MapMgr ? MapMgr->Rooms : Run->GetRooms();
	Save->Floor = MapMgr ? MapMgr->CurrentFloor : Run->GetFloor();
	Save->Theme = MapMgr && MapMgr->FloorTheme ? FSoftObjectPath(MapMgr->FloorTheme.Get()) : Run->GetThemePath();
	Save->SeenGuardians = Run->GetSeenGuardians();

	// 플레이어. 순서가 매번 같게 PlayerId 순
	TArray<ATerminusPlayerState*> Players;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ATerminusPlayerState* TPS = Cast<ATerminusPlayerState>(PS))
		{
			Players.Add(TPS);
		}
	}
	Players.Sort([](const ATerminusPlayerState& A, const ATerminusPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });

	for (ATerminusPlayerState* PS : Players)
	{
		FRunSavePlayer& Entry = Save->Players.AddDefaulted_GetRef();
		const FUniqueNetIdRepl& NetId = PS->GetUniqueId();
		Entry.PlayerId = NetId.IsValid() ? NetId->ToString() : FString();
		Entry.PlayerName = PS->GetPlayerName();
		Entry.RunState = PS->GetRunState();
		Entry.RunState.SelectedRoomId = -1;

		// 지금 체력. 0 이면(임시 부활 전 등) 1 로
		const ATerminusBattler* Battler = Cast<ATerminusBattler>(PS->GetPawn());
		const UCombatStatsComponent* Stats = Battler ? Battler->GetCombatStats() : nullptr;
		Entry.RunState.SavedHealth = Stats ? FMath::Max(1, Stats->GetCombatState().Health) : -1;

		Save->Summary.PlayerNames.Add(Entry.PlayerName);
		Save->Summary.PlayerClasses.Add(Entry.RunState.CharacterClass);
		Save->Summary.RoomsCleared = FMath::Max(Save->Summary.RoomsCleared, Entry.RunState.VisitedRoomIds.Num());
		Save->Summary.bHardMode |= Entry.RunState.bHardMode;
	}

	// 진행 중에 나갔다가 아직 안 돌아온 사람도 세이브에 (이어하기 때 그 사람도 와야 출발)
	if (const ADungeonGameMode* GM = World->GetAuthGameMode<ADungeonGameMode>())
	{
		for (const FDepartedPlayer& Departed : GM->GetDepartedPlayers())
		{
			FRunSavePlayer& Entry = Save->Players.AddDefaulted_GetRef();
			Entry.PlayerId = Departed.PlayerId;
			Entry.PlayerName = Departed.PlayerName;
			Entry.RunState = Departed.RunState;

			Save->Summary.PlayerNames.Add(Entry.PlayerName);
			Save->Summary.PlayerClasses.Add(Entry.RunState.CharacterClass);
			Save->Summary.RoomsCleared = FMath::Max(Save->Summary.RoomsCleared, Entry.RunState.VisitedRoomIds.Num());
		}
	}

	if (Save->Players.Num() == 0) return false;

	// 슬롯: 싱글은 언제나 하나 (SingleRunSlot). 멀티는 이 런의 슬롯을 계속 덮어씀 (주점을 안 거친 PIE 처럼 없으면 새로)
	const bool bMultiplayer = World->GetNetMode() != NM_Standalone;
	FString Slot = bMultiplayer ? Run->GetSaveSlot() : SingleRunSlot;
	if (Slot.IsEmpty())
	{
		Slot = MakeNewSlotName();
	}
	Run->SetSaveSlot(Slot);

	Save->Summary.SlotName = Slot;
	Save->Summary.RoomName = Run->GetRoomName();
	Save->Summary.SavedAt = FDateTime::Now();
	Save->Summary.bMultiplayer = bMultiplayer;
	Save->Summary.Floor = Save->Floor;

	if (!TerminusSaveCrypto::SaveToSlot(Save, Slot, RunSaveUserIndex))
	{
		UE_LOG(LogTerminusSave, Warning, TEXT("[Save] %s 저장 실패"), *Slot);
		return false;
	}

	// 목록 갱신 (같은 슬롯이면 교체). 싱글이면 다른 싱글 세이브(예전 방식으로 남은 것)는 지움 -> 싱글은 하나만
	if (UTerminusRunSaveIndex* Index = LoadIndex())
	{
		Index->Entries.RemoveAll([&Slot, bMultiplayer](const FRunSaveSummary& Entry)
		{
			if (Entry.SlotName == Slot) return true;
			if (!bMultiplayer && !Entry.bMultiplayer)
			{
				UGameplayStatics::DeleteGameInSlot(Entry.SlotName, RunSaveUserIndex);
				return true;
			}
			return false;
		});
		Index->Entries.Add(Save->Summary);
		SaveIndex(Index);
	}
	OnRunSavesChanged.Broadcast();

	UE_LOG(LogTerminusSave, Log, TEXT("[Save] %s 저장 (플레이어 %d명, 방 %d개 지남)"), *Slot, Save->Players.Num(), Save->Summary.RoomsCleared);
	return true;
}

UTerminusRunSave* UTerminusSaveSubsystem::LoadRunSave(const FString& SlotName) const
{
	if (SlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SlotName, RunSaveUserIndex)) return nullptr;
	// 조작 / 손상이면 nullptr (이어하기 실패로 안내). 예전 평문이면 바로 암호화해서 다시 씀
	TerminusSaveCrypto::ELoadResult Result = TerminusSaveCrypto::ELoadResult::NotFound;
	UTerminusRunSave* Save = Cast<UTerminusRunSave>(TerminusSaveCrypto::LoadFromSlot(SlotName, RunSaveUserIndex, &Result));
	if (Save && Result == TerminusSaveCrypto::ELoadResult::Legacy)
	{
		TerminusSaveCrypto::SaveToSlot(Save, SlotName, RunSaveUserIndex);
	}
	return Save;
}

bool UTerminusSaveSubsystem::DeleteRunSave(const FString& SlotName)
{
	if (SlotName.IsEmpty()) return false;

	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, RunSaveUserIndex);

	if (UTerminusRunSaveIndex* Index = LoadIndex())
	{
		if (Index->Entries.RemoveAll([&SlotName](const FRunSaveSummary& Entry) { return Entry.SlotName == SlotName; }) > 0)
		{
			SaveIndex(Index);
		}
	}

	if (PendingLoad && PendingLoad->Summary.SlotName == SlotName)
	{
		PendingLoad = nullptr;
	}

	UE_LOG(LogTerminusSave, Log, TEXT("[Save] %s 삭제"), *SlotName);
	OnRunSavesChanged.Broadcast();
	return bDeleted;
}

// =====================================================================
// 이어하기
// =====================================================================

bool UTerminusSaveSubsystem::ContinueRun(const UObject* WorldContext, const FString& SlotName, const FString& TavernMapPath, const FString& DungeonMapPath, FText& OutError)
{
	UTerminusRunSave* Save = LoadRunSave(SlotName);
	if (!Save || Save->Players.Num() == 0 || Save->Rooms.Num() == 0)
	{
		OutError = FText::FromString(TEXT("세이브 파일을 읽을 수 없습니다. 손상되었거나 변조된 파일입니다."));
		return false;
	}

	PendingLoad = Save;

	if (ULoadingScreenSubsystem* Loading = GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>())
	{
		Loading->Show(FText::FromString(TEXT("세이브를 불러오는 중...")));
	}

	// 이름은 세이브 당시 그대로
	if (UTerminusRunSubsystem* Run = GetGameInstance()->GetSubsystem<UTerminusRunSubsystem>())
	{
		Run->SetRoomName(Save->Summary.RoomName);
	}

	if (!Save->Summary.bMultiplayer)
	{
		// 싱글: 세션 없이 주점 -> 주점 게임모드가 바로 출발시킴
		UGameplayStatics::OpenLevel(WorldContext, FName(*TavernMapPath));
		return true;
	}

	// 멀티: 세이브 인원만큼만 들어올 수 있는 세션을 열고 던전으로 바로 -> 다 모일 때까지 이공간
	USessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USessionSubsystem>();
	if (!Sessions)
	{
		PendingLoad = nullptr;
		OutError = FText::FromString(TEXT("주점을 열 수 없습니다."));
		return false;
	}

	FTerminusRoomOptions Options;
	Options.RoomName = Save->Summary.RoomName;   // 비어 있으면 HostSession 이 랜덤
	Sessions->HostSession(Save->Players.Num(), DungeonMapPath, Options);
	return true;
}

// =====================================================================
// 표시
// =====================================================================

FText UTerminusSaveSubsystem::DescribeTitle(const FRunSaveSummary& Summary)
{
	const FString Mode = Summary.bMultiplayer
		? FString::Printf(TEXT("멀티 %d인"), Summary.PlayerNames.Num())
		: FString(TEXT("싱글"));

	FString Title = FString::Printf(TEXT("%s · %d층 %s · 방 %d개 지남"),
		*Mode, Summary.Floor, *AMapManager::GetTierName(Summary.Floor).ToString(), Summary.RoomsCleared);

	if (Summary.bHardMode)
	{
		Title += TEXT(" · 하드");
	}
	return FText::FromString(Title);
}

FText UTerminusSaveSubsystem::DescribePlayers(const FRunSaveSummary& Summary)
{
	TArray<FString> Parts;
	for (int32 i = 0; i < Summary.PlayerNames.Num(); ++i)
	{
		FString ClassName;
		if (Summary.PlayerClasses.IsValidIndex(i))
		{
			const FCharacterClassRow* Row = UTerminusDataSettings::FindCharacterClassRow(Summary.PlayerClasses[i]);
			ClassName = Row ? Row->DisplayName.ToString() : UEnum::GetDisplayValueAsText(Summary.PlayerClasses[i]).ToString();
		}
		Parts.Add(FString::Printf(TEXT("%s %s"), *ClassName, *Summary.PlayerNames[i]));
	}
	return FText::FromString(FString::Join(Parts, TEXT(", ")));
}
