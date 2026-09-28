#include "Game/TerminusRunSubsystem.h"

void UTerminusRunSubsystem::BeginNewRun(int32 InPartySize)
{
	PartySize = FMath::Max(1, InPartySize);
	Rooms.Reset();

	UE_LOG(LogTemp, Log, TEXT("[Run] 새 런 시작. 인원 %d"), PartySize);
}
