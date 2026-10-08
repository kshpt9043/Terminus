#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Map/MapManager.h"
#include "TerminusRunSubsystem.generated.h"

class UTerminusRunSave;

/**
 * 한 판(런) 동안 레벨 이동을 넘어 살아야 하는 서버 쪽 데이터.
 *
 * MapManager 는 레벨에 놓인 액터라 ServerTravel 하면 같이 죽고,
 * 새 레벨 BeginPlay 에서 지도를 또 뽑아버림 -> 전투 갔다 오면 지도가 바뀌는 문제.
 * GameInstance 는 트래블해도 안 죽어서 지도 원본을 여기 둠.
 *
 * 서버에서만 의미 있음. 클라는 MapManager 의 Rooms 복제로 받음
 * 플레이어별 진행도(현재 방, 층)는 PlayerState 의 RunState 가 들고 감 (CopyProperties)
 */
UCLASS()
class TERMINUS_API UTerminusRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 주점에서 출발할 때 호출. 이전 런의 지도를 버리고 인원을 확정. 세이브 슬롯도 새로
	void BeginNewRun(int32 InPartySize);

	// 이어하기로 출발할 때 호출. 세이브의 지도 / 층 / 인원 / 슬롯을 그대로
	void BeginLoadedRun(const UTerminusRunSave& Save);

	// 이 런의 세이브 슬롯. 방을 끝낼 때마다 여기에 덮어씀
	const FString& GetSaveSlot() const { return SaveSlot; }
	void SetSaveSlot(const FString& InSlot) { SaveSlot = InSlot; }

	// 이 런의 층 (세이브에서 불러온 값 / 층을 넘어갈 때). 0 이면 MapManager 디테일 값
	int32 GetFloor() const { return Floor; }
	void SetFloor(int32 InFloor) { Floor = InFloor; }

	// 이 런의 지금 테마 (UDungeonThemeData). 비어 있으면 MapManager 디테일 값
	const FSoftObjectPath& GetThemePath() const { return ThemePath; }
	void SetThemePath(const FSoftObjectPath& InPath) { ThemePath = InPath; }

	// 이번 런에 나온 가디언 (DT_Monster 행). 테마의 가디언이 전부 한 번씩 나오기 전엔 다시 안 나옴 (기획 10-07)
	const TArray<FName>& GetSeenGuardians() const { return SeenGuardians; }
	void AddSeenGuardian(FName Row) { SeenGuardians.AddUnique(Row); }
	void ForgetSeenGuardians(const TArray<FName>& Rows) { SeenGuardians.RemoveAll([&Rows](const FName& Row) { return Rows.Contains(Row); }); }

	// 방(던전) 이름. 처음 주점을 열 때(싱글은 시작할 때) 정한 이름이 런 끝까지 유지됨
	// 주점 목록 / 재합류 대기 / 세이브 목록 / 이어하기로 연 주점에 모두 이 이름
	const FString& GetRoomName() const { return RoomName; }
	void SetRoomName(const FString& InName);

	// 기본으로 넣어 줄 랜덤 이름 ("배고픈 슬라임 원정대")
	static FString MakeRandomRoomName();

	// 방 이름 최대 글자 수 (스팀 로비 광고 값 길이 때문)
	static constexpr int32 MaxRoomNameLength = 24;

	// 0 이면 주점을 안 거친 것 (던전 맵 바로 PIE). 이때는 MapManager 디테일 값을 씀
	int32 GetPartySize() const { return PartySize; }

	bool HasMap() const { return Rooms.Num() > 0; }
	const TArray<FRoomNode>& GetRooms() const { return Rooms; }
	void SetRooms(const TArray<FRoomNode>& InRooms) { Rooms = InRooms; }

private:
	int32 PartySize = 0;
	int32 Floor = 0;
	FSoftObjectPath ThemePath;
	FString SaveSlot;
	FString RoomName;

	UPROPERTY()
	TArray<FRoomNode> Rooms;

	UPROPERTY()
	TArray<FName> SeenGuardians;
};
