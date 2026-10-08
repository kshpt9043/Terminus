#include "Game/TerminusProfileSubsystem.h"

#include "Data/TerminusDataSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Game/TerminusSaveCrypto.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* ProfileSlotName = TEXT("Profile");
	const TCHAR* ProfileBackupSlotName = TEXT("Profile_Backup");     // 저장할 때마다 같이 씀 (원본이 깨졌을 때 복구용)
	const TCHAR* ProfileTamperedSlotName = TEXT("Profile_Tampered"); // 조작 / 손상된 원본 보관
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

// [테스트] 창고 콘솔 명령 (메인 메뉴에서도 됨)
//   Terminus.StoreRelic RLC_Common_005 / Terminus.StoreAllRelics / Terminus.ClearStorage
//   Terminus.OwnSkill holy_charge / Terminus.OwnAllSkills
static UTerminusProfileSubsystem* GetProfileFromWorld(UWorld* World)
{
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UTerminusProfileSubsystem>() : nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs GStoreRelicCommand(
	TEXT("Terminus.StoreRelic"), TEXT("창고에 유물 넣기. 예: Terminus.StoreRelic RLC_Common_005"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World); Profile && Args.Num() > 0)
		{
			Profile->AddStoredRelic(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorld GStoreAllRelicsCommand(
	TEXT("Terminus.StoreAllRelics"), TEXT("보관할 수 있는 유물을 하나씩 전부 창고에"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World);
		const UDataTable* Table = UTerminusDataSettings::Get()->RelicTable.LoadSynchronous();
		if (!Profile || !Table) return;
		for (const FName& Row : Table->GetRowNames())
		{
			Profile->AddStoredRelic(Row);
		}
	}));

static FAutoConsoleCommandWithWorld GClearStorageCommand(
	TEXT("Terminus.ClearStorage"), TEXT("창고 유물 전부 비우기"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World)) Profile->ClearStoredRelics();
	}));

static FAutoConsoleCommandWithWorld GResetUpgradesCommand(
	TEXT("Terminus.ResetUpgrades"), TEXT("거점 강화(연무장 / 훈련소) 전부 0 으로"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World)) Profile->ResetUpgrades();
	}));

static FAutoConsoleCommandWithWorldAndArgs GOwnSkillCommand(
	TEXT("Terminus.OwnSkill"), TEXT("보유 스킬 추가. 예: Terminus.OwnSkill holy_charge"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World); Profile && Args.Num() > 0)
		{
			Profile->AddOwnedSkill(FName(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorld GOwnAllSkillsCommand(
	TEXT("Terminus.OwnAllSkills"), TEXT("보유할 수 있는 스킬 전부 보유"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UTerminusProfileSubsystem* Profile = GetProfileFromWorld(World);
		const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous();
		if (!Profile || !Table) return;
		for (const FName& Row : Table->GetRowNames())
		{
			Profile->AddOwnedSkill(Row);
		}
	}));

void UTerminusProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 암호화 세이브 (TerminusSaveCrypto). 조작 / 손상이면 백업에서, 백업도 안 되면 새로
	using TerminusSaveCrypto::ELoadResult;
	ELoadResult Result = ELoadResult::NotFound;
	Profile = Cast<UTerminusProfileSave>(TerminusSaveCrypto::LoadFromSlot(ProfileSlotName, ProfileUserIndex, &Result));
	bool bNeedsSave = Result == ELoadResult::Legacy;   // 예전 평문 -> 바로 암호화해서 다시 씀

	if (!Profile && Result == ELoadResult::Tampered)
	{
		TerminusSaveCrypto::CopySlot(ProfileSlotName, ProfileTamperedSlotName, ProfileUserIndex);

		ELoadResult BackupResult = ELoadResult::NotFound;
		Profile = Cast<UTerminusProfileSave>(TerminusSaveCrypto::LoadFromSlot(ProfileBackupSlotName, ProfileUserIndex, &BackupResult));
		LoadNotice = FText::FromString(Profile
			? TEXT("세이브 파일이 손상되어 백업에서 복구했습니다.")
			: TEXT("세이브 파일이 손상되었거나 변조되어 새로 시작합니다."));
		UE_LOG(LogTemp, Warning, TEXT("[Profile] 세이브 조작 / 손상 -> %s"), Profile ? TEXT("백업에서 복구") : TEXT("새로 시작"));
		bNeedsSave = true;
	}

	if (!Profile)
	{
		Profile = Cast<UTerminusProfileSave>(UGameplayStatics::CreateSaveGameObject(UTerminusProfileSave::StaticClass()));
	}

	if (bNeedsSave)
	{
		Save();
	}

	// 예전 세이브엔 같은 유물이 여러 개 있을 수 있음 -> 도감처럼 하나씩만
	TArray<FName> Unique;
	for (const FName& Row : Profile->StoredRelics)
	{
		Unique.AddUnique(Row);
	}
	if (Unique.Num() != Profile->StoredRelics.Num())
	{
		Profile->StoredRelics = MoveTemp(Unique);
		Save();
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
	OnStorageChanged.Broadcast();
	return true;
}

void UTerminusProfileSubsystem::ClearOwnedSkills()
{
	if (!Profile) return;

	Profile->OwnedSkills.Reset();
	Save();
	OnStorageChanged.Broadcast();
}

const TArray<FName>& UTerminusProfileSubsystem::GetStoredRelics() const
{
	static const TArray<FName> Empty;
	return Profile ? Profile->StoredRelics : Empty;
}

bool UTerminusProfileSubsystem::IsStorableRelic(FName RelicRow)
{
	const FRelicRow* Relic = RelicRow.IsNone() ? nullptr : UTerminusDataSettings::FindRelicRow(RelicRow);
	return Relic && Relic->RelicTier != ERelicTier::Basic && Relic->OwnerClass != ESkillOwner::Monster;
}

bool UTerminusProfileSubsystem::AddStoredRelic(FName RelicRow)
{
	// 창고 = 도감: 개수 제한 없음, 같은 유물은 한 번만
	if (!Profile || !IsStorableRelic(RelicRow) || Profile->StoredRelics.Contains(RelicRow)) return false;

	Profile->StoredRelics.Add(RelicRow);
	Save();
	OnStorageChanged.Broadcast();
	return true;
}

bool UTerminusProfileSubsystem::RemoveStoredRelic(FName RelicRow)
{
	if (!Profile || !Profile->StoredRelics.RemoveSingle(RelicRow)) return false;

	Save();
	OnStorageChanged.Broadcast();
	return true;
}

void UTerminusProfileSubsystem::ClearStoredRelics()
{
	if (!Profile) return;

	Profile->StoredRelics.Reset();
	Save();
	OnStorageChanged.Broadcast();
}

int32 UTerminusProfileSubsystem::GetGold() const
{
	return Profile ? Profile->Gold : 0;
}

// =====================================================================
// 정산
// =====================================================================

FPendingSettlement UTerminusProfileSubsystem::MakeSettlement(const TArray<FName>& RunRelics, const TArray<FName>& StartRelics, const TArray<FName>& EnhanceSkills,
	const FString& Reason, const FString& RoomName, int32 Floor) const
{
	FPendingSettlement Out;
	Out.bValid = true;
	Out.Reason = Reason;
	Out.RoomName = RoomName;
	Out.Floor = Floor;

	for (const FName& Row : RunRelics)
	{
		// 직업 기본 유물 / 몬스터 유물 / 창고에서 들고 간 시작 유물은 후보가 아님
		if (!IsStorableRelic(Row) || StartRelics.Contains(Row)) continue;
		Out.Relics.AddUnique(Row);
	}

	// 장착 중인 픽업 스킬 중 아직 없는 것 하나 랜덤 (중복 X, 전부 있으면 건너뜀)
	TArray<FName> NewSkills;
	for (const FName& Skill : EnhanceSkills)
	{
		if (IsOwnableSkill(Skill) && !GetOwnedSkills().Contains(Skill)) NewSkills.AddUnique(Skill);
	}
	if (NewSkills.Num() > 0)
	{
		Out.KeptSkill = NewSkills[FMath::RandRange(0, NewSkills.Num() - 1)];
	}
	return Out;
}

void UTerminusProfileSubsystem::BeginSettlement(const FPendingSettlement& Settlement)
{
	if (!Profile) return;

	Profile->PendingSettlement = Settlement;
	Profile->PendingSettlement.bValid = true;
	Save();
	UE_LOG(LogTemp, Log, TEXT("[Profile] 정산 대기: 유물 %d개, 스킬 %s"), Settlement.Relics.Num(), *Settlement.KeptSkill.ToString());
}

bool UTerminusProfileSubsystem::HasPendingSettlement() const
{
	return Profile && Profile->PendingSettlement.bValid;
}

const FPendingSettlement& UTerminusProfileSubsystem::GetPendingSettlement() const
{
	static const FPendingSettlement Empty;
	return Profile ? Profile->PendingSettlement : Empty;
}

void UTerminusProfileSubsystem::FinishSettlement(FName KeptRelic, int32 GoldEarned)
{
	if (!HasPendingSettlement()) return;

	const FPendingSettlement Done = Profile->PendingSettlement;
	Profile->PendingSettlement = FPendingSettlement();   // 먼저 비워서 아래가 중간에 실패해도 두 번 받지 않게
	Save();

	// 고른 유물 하나만 보관 (이미 있는 유물은 아무 일도 없음: 사용자 결정 10-06). 나머지는 사라짐
	if (!KeptRelic.IsNone() && Done.Relics.Contains(KeptRelic))
	{
		AddStoredRelic(KeptRelic);
	}
	if (!Done.KeptSkill.IsNone())
	{
		AddOwnedSkill(Done.KeptSkill);
	}
	AddGold(GoldEarned);

	UE_LOG(LogTemp, Log, TEXT("[Profile] 정산 끝: 골드 +%d, 보관 %s, 스킬 %s"), GoldEarned, *KeptRelic.ToString(), *Done.KeptSkill.ToString());
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

// =====================================================================
// 거점 강화
// =====================================================================

FClassUpgrades UTerminusProfileSubsystem::GetClassUpgrades(ECharacterClass InClass) const
{
	if (Profile)
	{
		if (const FClassUpgrades* Found = Profile->Upgrades.FindByPredicate([InClass](const FClassUpgrades& U) { return U.Class == InClass; }))
		{
			return *Found;
		}
	}

	FClassUpgrades Empty;
	Empty.Class = InClass;
	return Empty;
}

const TArray<FClassUpgrades>& UTerminusProfileSubsystem::GetAllUpgrades() const
{
	static const TArray<FClassUpgrades> Empty;
	return Profile ? Profile->Upgrades : Empty;
}

FClassUpgrades& UTerminusProfileSubsystem::FindOrAddUpgrades(ECharacterClass InClass)
{
	if (FClassUpgrades* Found = Profile->Upgrades.FindByPredicate([InClass](const FClassUpgrades& U) { return U.Class == InClass; }))
	{
		return *Found;
	}

	FClassUpgrades& Added = Profile->Upgrades.AddDefaulted_GetRef();
	Added.Class = InClass;
	return Added;
}

bool UTerminusProfileSubsystem::TryUpgradeStat(ECharacterClass InClass, EStatUpgrade Stat)
{
	if (!Profile || Stat >= EStatUpgrade::MAX) return false;

	const FClassUpgrades Current = GetClassUpgrades(InClass);
	const int32 Cost = UTerminusUpgradeSettings::Get()->GetStatCost(Stat, Current.GetStatLevel(Stat));
	if (Cost < 0 || !TrySpendGold(Cost)) return false;

	FClassUpgrades& Upgrades = FindOrAddUpgrades(InClass);
	const int32 Index = static_cast<int32>(Stat);
	if (Upgrades.StatLevels.Num() <= Index)
	{
		Upgrades.StatLevels.SetNumZeroed(static_cast<int32>(EStatUpgrade::MAX));
	}
	++Upgrades.StatLevels[Index];

	Save();
	OnUpgradesChanged.Broadcast();
	return true;
}

bool UTerminusProfileSubsystem::TryUpgradeSkill(ECharacterClass InClass, int32 SkillIndex)
{
	if (!Profile || SkillIndex < 0 || SkillIndex >= 3) return false;

	const FClassUpgrades Current = GetClassUpgrades(InClass);
	const int32 Cost = UTerminusUpgradeSettings::Get()->GetSkillCost(Current.GetSkillLevel(SkillIndex));
	if (Cost < 0 || !TrySpendGold(Cost)) return false;

	FClassUpgrades& Upgrades = FindOrAddUpgrades(InClass);
	if (Upgrades.SkillLevels.Num() <= SkillIndex)
	{
		Upgrades.SkillLevels.SetNumZeroed(3);
	}
	++Upgrades.SkillLevels[SkillIndex];

	Save();
	OnUpgradesChanged.Broadcast();
	return true;
}

void UTerminusProfileSubsystem::ResetUpgrades()
{
	if (!Profile) return;

	Profile->Upgrades.Reset();
	Save();
	OnUpgradesChanged.Broadcast();
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
	if (Profile && TerminusSaveCrypto::SaveToSlot(Profile, ProfileSlotName, ProfileUserIndex))
	{
		TerminusSaveCrypto::SaveToSlot(Profile, ProfileBackupSlotName, ProfileUserIndex);
	}
}

FText UTerminusProfileSubsystem::ConsumeLoadNotice()
{
	FText Out = LoadNotice;
	LoadNotice = FText::GetEmpty();
	return Out;
}
