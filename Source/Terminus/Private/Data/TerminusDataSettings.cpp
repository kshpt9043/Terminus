#include "Data/TerminusDataSettings.h"

#include "Engine/DataTable.h"

const FCharacterClassRow* UTerminusDataSettings::FindCharacterClassRow(ECharacterClass InClass)
{
	// 소프트 참조라 여기서 처음 꺼낼 때 로드된다. 한 번 로드되면 다음부턴 이미 메모리에 있음
	const UDataTable* Table = Get()->CharacterClassTable.LoadSynchronous();
	if (!Table)
	{
		return nullptr;
	}

	// Ctx 는 행 구조가 안 맞을 때 로그에 찍히는 이름
	static const FString Ctx(TEXT("FindCharacterClassRow"));
	TArray<FCharacterClassRow*> Rows;
	Table->GetAllRows<FCharacterClassRow>(Ctx, Rows);

	for (const FCharacterClassRow* Row : Rows)
	{
		if (Row && Row->Class == InClass)
		{
			return Row;
		}
	}

	return nullptr;
}

const FSkillRow* UTerminusDataSettings::FindSkillRow(FName RowName)
{
	const UDataTable* Table = Get()->SkillTable.LoadSynchronous();
	if (!Table)
	{
		return nullptr;
	}
	
	static const FString Ctx(TEXT("FindSkillRow"));
	return Table->FindRow<FSkillRow>(RowName, Ctx);
}

const FMonsterRow* UTerminusDataSettings::FindMonsterRow(FName RowName)
{
	const UDataTable* Table = Get()->MonsterTable.LoadSynchronous();
	if (!Table)
	{
		return nullptr;
	}
	
	static const FString Ctx(TEXT("FindMonsterRow"));
	return Table->FindRow<FMonsterRow>(RowName, Ctx);
}
