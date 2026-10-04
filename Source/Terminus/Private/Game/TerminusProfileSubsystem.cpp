#include "Game/TerminusProfileSubsystem.h"

#include "Data/TerminusDataSettings.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* ProfileSlotName = TEXT("Profile");
	constexpr int32 ProfileUserIndex = 0;
}

// [테스트] 콘솔: Terminus.AddGold 500 / Terminus.SetGold 0  (주점 / 메인 메뉴 / 던전 어디서나)
static FAutoConsoleCommandWithWorldAndArgs GAddGoldCommand(
	TEXT("Terminus.AddGold"), TEXT("골드를 더함. 예: Terminus.AddGold 500"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UTerminusProfileSubsystem* Profile = GI ? GI->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
		if (Profile && Args.Num() > 0)
		{
			Profile->AddGold(FCString::Atoi(*Args[0]));
			UE_LOG(LogTemp, Log, TEXT("[Debug] 골드 %d"), Profile->GetGold());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSetGoldCommand(
	TEXT("Terminus.SetGold"), TEXT("골드를 이 값으로. 예: Terminus.SetGold 0"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UTerminusProfileSubsystem* Profile = GI ? GI->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
		if (Profile && Args.Num() > 0)
		{
			Profile->SetGoldForDebug(FCString::Atoi(*Args[0]));
			UE_LOG(LogTemp, Log, TEXT("[Debug] 골드 %d"), Profile->GetGold());
		}
	}));

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

int32 UTerminusProfileSubsystem::GetGold() const
{
	return Profile ? Profile->Gold : 0;
}

void UTerminusProfileSubsystem::AddGold(int32 Amount)
{
	if (!Profile || Amount <= 0) return;

	// int32 넘침 방지
	const int64 Sum = static_cast<int64>(Profile->Gold) + Amount;
	Profile->Gold = static_cast<int32>(FMath::Min<int64>(Sum, MAX_int32));
	Save();
	OnGoldChanged.Broadcast(Profile->Gold, Amount);
}

bool UTerminusProfileSubsystem::TrySpendGold(int32 Cost)
{
	if (!Profile || Cost < 0 || Profile->Gold < Cost) return false;
	if (Cost == 0) return true;

	Profile->Gold -= Cost;
	Save();
	OnGoldChanged.Broadcast(Profile->Gold, -Cost);
	return true;
}

void UTerminusProfileSubsystem::SetGoldForDebug(int32 NewGold)
{
	if (!Profile) return;

	const int32 Delta = FMath::Max(0, NewGold) - Profile->Gold;
	Profile->Gold = FMath::Max(0, NewGold);
	Save();
	OnGoldChanged.Broadcast(Profile->Gold, Delta);
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
