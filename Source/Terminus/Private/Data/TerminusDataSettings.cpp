#include "Data/TerminusDataSettings.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

namespace
{
	// 직업 열거형과 스킬 소유자 열거형은 따로 있다(공용 몬스터 때문에). 여기서 한 번만 잇는다
	ESkillOwner ToSkillOwner(ECharacterClass InClass)
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
}

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

UTexture2D* UTerminusDataSettings::FindSkillIcon(const FSkillRow& Skill)
{
	const UTerminusDataSettings* Settings = Get();

	if (!Skill.IconID.IsNone())
	{
		if (const TSoftObjectPtr<UTexture2D>* Found = Settings->SkillIcons.Find(Skill.IconID))
		{
			if (UTexture2D* Tex = Found->LoadSynchronous())
			{
				return Tex;
			}
		}
	}

	if (const TSoftObjectPtr<UTexture2D>* Found = Settings->SkillTypeIcons.Find(Skill.SkillType))
	{
		return Found->LoadSynchronous();
	}
	return nullptr;
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

TArray<const FSkillRow*> UTerminusDataSettings::FindBasicSkills(ECharacterClass InClass)
{
	TArray<const FSkillRow*> Out;

	const UDataTable* Table = Get()->SkillTable.LoadSynchronous();
	if (!Table)
	{
		return Out;
	}

	const ESkillOwner Owner = ToSkillOwner(InClass);
	Table->ForeachRow<FSkillRow>(TEXT("FindBasicSkills"),
		[&](const FName& Key, const FSkillRow& Row)
		{
			if (Row.OwnerClass == Owner && Row.SkillCategory == ESkillCategory::Basic)
			{
				Out.Add(&Row);
			}
		});

	// 테이블 행 순서에 기대지 않고 공격 방어 특수 순으로 정렬
	Out.Sort([](const FSkillRow& A, const FSkillRow& B)
	{
		return A.SkillType < B.SkillType;
	});
	return Out;
}
