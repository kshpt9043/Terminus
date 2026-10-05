#include "Game/TerminusRunSubsystem.h"

#include "Game/TerminusSaveSubsystem.h"

void UTerminusRunSubsystem::BeginNewRun(int32 InPartySize)
{
	PartySize = FMath::Max(1, InPartySize);
	Floor = 0;
	Rooms.Reset();
	SaveSlot = UTerminusSaveSubsystem::MakeNewSlotName();

	UE_LOG(LogTemp, Log, TEXT("[Run] 새 런 시작. 인원 %d, 세이브 %s"), PartySize, *SaveSlot);
}

void UTerminusRunSubsystem::BeginLoadedRun(const UTerminusRunSave& Save)
{
	PartySize = FMath::Max(1, Save.Players.Num());
	Floor = FMath::Max(1, Save.Floor);
	Rooms = Save.Rooms;
	SaveSlot = Save.Summary.SlotName;

	UE_LOG(LogTemp, Log, TEXT("[Run] 세이브 %s 이어하기. 인원 %d, %d층, 방 %d개"), *SaveSlot, PartySize, Floor, Rooms.Num());
}
