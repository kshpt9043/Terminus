#include "Game/TerminusRunSubsystem.h"

#include "Game/TerminusSaveSubsystem.h"

void UTerminusRunSubsystem::BeginNewRun(int32 InPartySize)
{
	PartySize = FMath::Max(1, InPartySize);
	Floor = 0;
	Rooms.Reset();
	SaveSlot = UTerminusSaveSubsystem::MakeNewSlotName();

	// 이름은 주점을 열 때(싱글은 시작할 때) 이미 정해짐. 혹시 비었으면 랜덤
	if (RoomName.IsEmpty())
	{
		RoomName = MakeRandomRoomName();
	}

	UE_LOG(LogTemp, Log, TEXT("[Run] 새 런 시작 '%s'. 인원 %d, 세이브 %s"), *RoomName, PartySize, *SaveSlot);
}

void UTerminusRunSubsystem::SetRoomName(const FString& InName)
{
	RoomName = InName.TrimStartAndEnd().Left(MaxRoomNameLength);
}

FString UTerminusRunSubsystem::MakeRandomRoomName()
{
	static const TCHAR* Adjectives[] = {
		TEXT("녹슨"), TEXT("푸른"), TEXT("붉은"), TEXT("잠든"), TEXT("수상한"), TEXT("떠도는"), TEXT("용감한"), TEXT("배고픈"),
		TEXT("은빛"), TEXT("그을린"), TEXT("고요한"), TEXT("시끄러운"), TEXT("낡은"), TEXT("길 잃은"), TEXT("빛나는"), TEXT("겁 없는") };
	static const TCHAR* Nouns[] = {
		TEXT("방패"), TEXT("등불"), TEXT("고블린"), TEXT("모험가"), TEXT("슬라임"), TEXT("여관"), TEXT("검"), TEXT("지도"),
		TEXT("해골"), TEXT("망토"), TEXT("마도구"), TEXT("주사위"), TEXT("열쇠"), TEXT("촛대"), TEXT("용"), TEXT("곰") };
	static const TCHAR* Groups[] = { TEXT("원정대"), TEXT("탐험대"), TEXT("일행"), TEXT("파티") };

	return FString::Printf(TEXT("%s %s %s"),
		Adjectives[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Adjectives)) - 1)],
		Nouns[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Nouns)) - 1)],
		Groups[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Groups)) - 1)]).Left(MaxRoomNameLength);
}

void UTerminusRunSubsystem::BeginLoadedRun(const UTerminusRunSave& Save)
{
	PartySize = FMath::Max(1, Save.Players.Num());
	Floor = FMath::Max(1, Save.Floor);
	Rooms = Save.Rooms;
	SaveSlot = Save.Summary.SlotName;
	RoomName = Save.Summary.RoomName.IsEmpty() ? MakeRandomRoomName() : Save.Summary.RoomName;

	UE_LOG(LogTemp, Log, TEXT("[Run] 세이브 %s 이어하기. 인원 %d, %d층, 방 %d개"), *SaveSlot, PartySize, Floor, Rooms.Num());
}
