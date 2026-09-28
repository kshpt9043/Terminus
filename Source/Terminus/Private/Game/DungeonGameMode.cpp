// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/DungeonGameMode.h"

#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AActor* ADungeonGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Slots;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Slots);

	// X 좌표 순 -> 왼쪽부터 0번 자리. 순서가 뒤집혀 나오면 부등호만 바꾸면 됨
	Slots.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetActorLocation().X < B.GetActorLocation().X;
	});

	// PlayerArray 순서는 리스타트 중에 계속 바뀌어서 못 쓴다(전원이 맨 뒤로 온다)
	// 그래서 순번을 묻지 않고, 아직 안 준 자리를 왼쪽부터 건넨다
	for (AActor* Slot : Slots)
	{
		if (!AssignedStarts.Contains(Slot))
		{
			AssignedStarts.Add(Slot);
			return Slot;
		}
	}

	// 자리보다 인원이 많으면 엔진 기본에 맡긴다
	
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADungeonGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	if (!ErrorMessage.IsEmpty()) { return; }   // 엔진이 이미 거절했으면 그대로

	// PIE 는 심리스 트래블이 막혀서 클라가 재접속으로 넘어옴 -> 막으면 테스트 불가
	if (GetWorld()->WorldType == EWorldType::PIE) { return; }

	// 던전엔 주점에서 같이 넘어온 사람만. 심리스로 온 사람은 여기를 안 탐
	ErrorMessage = TEXT("이미 던전이 진행 중입니다.");
}
