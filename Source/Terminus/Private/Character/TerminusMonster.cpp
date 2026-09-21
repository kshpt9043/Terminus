#include "Character/TerminusMonster.h"

#include "Combat/CombatStatsComponent.h"
#include "Data/TerminusDataSettings.h"

ATerminusMonster::ATerminusMonster()
{
	// 몬스터는 오른쪽에 서서 플레이어 쪽(왼쪽)을 봄
	bFacingRight = false;
}

void ATerminusMonster::BeginPlay()
{
	Super::BeginPlay();

	InitAsMonster(MonsterRow);
}

void ATerminusMonster::InitAsMonster(FName RowName)
{
	const FMonsterRow* Row = UTerminusDataSettings::FindMonsterRow(RowName);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Monster: %s 행이 없음"), *RowName.ToString());
		return;
	}

	// 스텟은 서버만 정함(InitFrom 안에 가드). 클라는 복제로 받음. 플레이어랑 같은 길
	CombatStats->InitFrom(Row->ToStats());
}