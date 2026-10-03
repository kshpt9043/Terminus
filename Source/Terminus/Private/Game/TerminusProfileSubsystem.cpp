#include "Game/TerminusProfileSubsystem.h"

#include "Data/TerminusDataSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* ProfileSlotName = TEXT("Profile");
	constexpr int32 ProfileUserIndex = 0;
}

void UTerminusProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(ProfileSlotName, ProfileUserIndex))
	{
		Profile = Cast<UTerminusProfileSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlotName, ProfileUserIndex));
	}

	if (!Profile)
	{
		Profile = Cast<UTerminusProfileSave>(UGameplayStatics::CreateSaveGameObject(UTerminusProfileSave::StaticClass()));
	}
}

const TArray<FName>& UTerminusProfileSubsystem::GetOwnedSkills() const
{
	static const TArray<FName> Empty;
	return Profile ? Profile->OwnedSkills : Empty;
}

TArray<FName> UTerminusProfileSubsystem::GetUsableOwnedSkills(ECharacterClass InClass) const
{
	TArray<FName> Out;
	for (const FName& Row : GetOwnedSkills())
	{
		if (IsEquippableSkill(Row, InClass))
		{
			Out.AddUnique(Row);
		}
	}
	return Out;
}

bool UTerminusProfileSubsystem::AddOwnedSkill(FName SkillRow)
{
	if (!Profile || !IsOwnableSkill(SkillRow) || Profile->OwnedSkills.Contains(SkillRow))
	{
		return false;
	}

	Profile->OwnedSkills.Add(SkillRow);
	Save();
	return true;
}

void UTerminusProfileSubsystem::ClearOwnedSkills()
{
	if (!Profile) return;

	Profile->OwnedSkills.Reset();
	Save();
}

bool UTerminusProfileSubsystem::IsOwnableSkill(FName SkillRow)
{
	const FSkillRow* Row = SkillRow.IsNone() ? nullptr : UTerminusDataSettings::FindSkillRow(SkillRow);
	return Row
		&& Row->SkillCategory != ESkillCategory::Basic
		&& Row->OwnerClass != ESkillOwner::Monster;
}

bool UTerminusProfileSubsystem::IsEquippableSkill(FName SkillRow, ECharacterClass InClass)
{
	if (!IsOwnableSkill(SkillRow)) return false;

	const FSkillRow* Row = UTerminusDataSettings::FindSkillRow(SkillRow);
	if (Row->SkillCategory != ESkillCategory::Personal) return true;   // Common / Event

	// 직업 열거형과 스킬 소유자 열거형은 따로라 이름으로 잇는 대신 하나씩 비교
	switch (InClass)
	{
	case ECharacterClass::Fighter:  return Row->OwnerClass == ESkillOwner::Fighter;
	case ECharacterClass::Engineer: return Row->OwnerClass == ESkillOwner::Engineer;
	case ECharacterClass::Paladin:  return Row->OwnerClass == ESkillOwner::Paladin;
	case ECharacterClass::Assassin: return Row->OwnerClass == ESkillOwner::Assassin;
	default:                        return false;
	}
}

void UTerminusProfileSubsystem::Save()
{
	if (Profile)
	{
		UGameplayStatics::SaveGameToSlot(Profile, ProfileSlotName, ProfileUserIndex);
	}
}
