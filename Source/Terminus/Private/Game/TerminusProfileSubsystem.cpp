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

TArray<FName> UTerminusProfileSubsystem::GetUsableOwnedSkills() const
{
	TArray<FName> Out;
	for (const FName& Row : GetOwnedSkills())
	{
		if (IsEnhanceSkill(Row))
		{
			Out.AddUnique(Row);
		}
	}
	return Out;
}

bool UTerminusProfileSubsystem::AddOwnedSkill(FName SkillRow)
{
	if (!Profile || !IsEnhanceSkill(SkillRow) || Profile->OwnedSkills.Contains(SkillRow))
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

bool UTerminusProfileSubsystem::IsEnhanceSkill(FName SkillRow)
{
	const FSkillRow* Row = SkillRow.IsNone() ? nullptr : UTerminusDataSettings::FindSkillRow(SkillRow);
	return Row
		&& Row->SkillCategory != ESkillCategory::Basic
		&& Row->OwnerClass != ESkillOwner::Monster;
}

void UTerminusProfileSubsystem::Save()
{
	if (Profile)
	{
		UGameplayStatics::SaveGameToSlot(Profile, ProfileSlotName, ProfileUserIndex);
	}
}
