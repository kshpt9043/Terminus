#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterTypes.h"
#include "Data/UpgradeTypes.h"
#include "TerminusProfileSubsystem.generated.h"

// 아직 안 끝낸 정산 (던전 탈출 직후 저장 -> 메인 화면에서 정산). 순서: 유물 하나 보관 -> 완료 퀘스트 -> 최종 골드
// 사용자 결정 10-08: 던전에서 얻은 유물 중 하나만 골라 창고로, 나머지는 사라짐 (유물 판매 없음. 노션 정산 페이지의 판매와 다름)
// 방장이 먼저 나가서 끊기거나 게임이 꺼져도 다음 메인 화면에서 이어서 정산하게 프로필에 저장
USTRUCT()
struct FPendingSettlement
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;

	// 전멸로 끝남 (기획 사망 순서도): 유물은 고르지 않고 전부 골드로 판매 (RelicGold), 스킬은 못 가져감
	UPROPERTY() bool bDeath = false;

	// "던전을 탈출했습니다." 같은 안내 / 던전 이름 / 몇 층까지 갔는지
	UPROPERTY() FString Reason;
	UPROPERTY() FString RoomName;
	UPROPERTY() int32 Floor = 0;

	// 고를 수 있는 유물: 던전에서 얻은 것 (직업 기본 유물 / 몬스터 유물 / 시작 유물 제외)
	UPROPERTY() TArray<FName> Relics;

	// 사망 정산일 때 Relics 와 같은 순서의 판매 골드 (DT 정산 판매가, 못 파는 건 0)
	UPROPERTY() TArray<int32> RelicGold;

	// 하드 모드에서 죽음: 창고에서 들고 간 시작 유물이 창고(도감)에서 지워짐 (사용자 결정 10-08)
	UPROPERTY() TArray<FName> LostStoredRelics;

	// 장착 중인 픽업 스킬 중 아직 없는 것 하나 랜덤 (전부 있으면 None = 건너뜀)
	UPROPERTY() FName KeptSkill;
};

/**
 * 런이 끝나도 남는 플레이어 영구 데이터. 이 컴퓨터의 세이브 슬롯에 저장됨
 * (멀티면 각자 자기 컴퓨터의 것을 씀)
 */
UCLASS()
class TERMINUS_API UTerminusProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	// 영구 소유한 스킬 (DT_Skill 행 이름). 던전을 클리어하면 그 던전에서 쓴 스킬 중 하나를 여기에 얻음
	// 런 시작 때 이 중에서 강화 스킬을 하나 골라 장착
	UPROPERTY()
	TArray<FName> OwnedSkills;

	// 골드. 던전 밖 재화 (정산에서 얻고 훈련소 / 직업의 탑에서 씀). 던전 재화와 달리 판이 끝나도 남음
	UPROPERTY()
	int32 Gold = 0;

	// 창고에 보관한 유물 (DT_Relic 행 이름). 정산에서 팔지 않고 '보유'를 고른 것
	// 도감처럼 개수 제한 없이 종류별로 하나씩. 던전에 들어갈 때 이 중 1개를 들고 갈 수 있음 (기획 UI 레퍼런스 > 게임 시작)
	UPROPERTY()
	TArray<FName> StoredRelics;

	// 거점 강화 (직업마다). 연무장 스텟 / 훈련소 기본 스킬 단계
	UPROPERTY()
	TArray<FClassUpgrades> Upgrades;

	// 아직 안 끝낸 정산 (있으면 메인 화면이 정산 화면부터 띄움)
	UPROPERTY()
	FPendingSettlement PendingSettlement;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoldChanged, int32, NewGold, int32, Delta);

/**
 * 영구 데이터(UTerminusProfileSave) 창구. 게임 켤 때 불러오고, 바뀔 때마다 바로 저장
 */
UCLASS()
class TERMINUS_API UTerminusProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 보유 스킬 전부 (저장된 그대로)
	const TArray<FName>& GetOwnedSkills() const;

	// 보유 스킬 중 이 직업이 지금 강화 스킬로 장착할 수 있는 것만 (IsEquippableSkill)
	TArray<FName> GetUsableOwnedSkills(ECharacterClass InClass) const;

	// 영구 소유 추가. 이미 있거나 보유할 수 있는 스킬이 아니면 false
	bool AddOwnedSkill(FName SkillRow);

	void ClearOwnedSkills();

	// -------------------------------------------------------------
	// [골드]
	// 각자 자기 컴퓨터의 세이브에 있음. 멀티에서 정산하면 서버가 각 클라에 "얼마 얻었다"를 알려주고
	// 클라가 자기 세이브에 더하는 식으로 쓸 것 (서버가 남의 세이브를 만지지 않음)
	// -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Terminus|Gold")
	int32 GetGold() const;

	// 골드 얻기 (0 이하는 무시). 바로 저장
	UFUNCTION(BlueprintCallable, Category = "Terminus|Gold")
	void AddGold(int32 Amount);

	// 골드 쓰기. 모자라면 false 하고 그대로. 바로 저장
	UFUNCTION(BlueprintCallable, Category = "Terminus|Gold")
	bool TrySpendGold(int32 Cost);

	// [테스트] 골드를 이 값으로 (Terminus.SetGold 콘솔 명령)
	void SetGoldForDebug(int32 NewGold);

	// -------------------------------------------------------------
	// [창고 유물]
	// -------------------------------------------------------------

	const TArray<FName>& GetStoredRelics() const;

	// 창고에 넣기. 개수 제한 없음, 같은 유물은 한 번만 (도감)
	// 보관할 수 없는 유물(DT 에 없음 / 직업 기본 유물 / 몬스터 유물)이거나 이미 있으면 false
	bool AddStoredRelic(FName RelicRow);

	// 창고에서 하나 빼기 (던전에 들고 갈 때 등)
	bool RemoveStoredRelic(FName RelicRow);

	void ClearStoredRelics();

	static bool IsStorableRelic(FName RelicRow);

	// -------------------------------------------------------------
	// [거점 강화] 직업마다 따로. 골드로 사고 바로 저장
	// -------------------------------------------------------------

	// 이 직업의 강화 단계 (없으면 전부 0)
	FClassUpgrades GetClassUpgrades(ECharacterClass InClass) const;

	// 모든 직업의 강화 (멀티에서 서버에 알릴 때)
	const TArray<FClassUpgrades>& GetAllUpgrades() const;

	// 연무장 / 훈련소 한 단계 올리기. 골드가 모자라거나 최대면 false
	bool TryUpgradeStat(ECharacterClass InClass, EStatUpgrade Stat);
	bool TryUpgradeSkill(ECharacterClass InClass, int32 SkillIndex);

	// [테스트] 강화 전부 0 으로
	void ResetUpgrades();

	DECLARE_MULTICAST_DELEGATE(FOnUpgradesChanged);
	FOnUpgradesChanged OnUpgradesChanged;

	// -------------------------------------------------------------
	// [정산] 던전 탈출 -> 정산 대기 저장 -> 메인 화면에서 유물 판매 / 보관 -> 골드
	// -------------------------------------------------------------

	// 이번 런 결과로 정산 내용을 만듦: 고를 수 있는 유물(시작 유물 StartRelics 제외), 얻을 스킬 하나
	// bDeath = 전멸 (유물은 전부 골드로, 스킬 없음)
	FPendingSettlement MakeSettlement(const TArray<FName>& RunRelics, const TArray<FName>& StartRelics, const TArray<FName>& EnhanceSkills,
		const FString& Reason, const FString& RoomName, int32 Floor, bool bDeath = false) const;

	// 정산 대기로 저장 (이미 있으면 덮어씀)
	void BeginSettlement(const FPendingSettlement& Settlement);

	bool HasPendingSettlement() const;
	const FPendingSettlement& GetPendingSettlement() const;

	// 정산 끝: 고른 유물 하나를 창고에(None = 없음), 스킬을 보유 스킬에, 퀘스트 골드를 골드에. 정산 대기는 비움
	// 하드 모드 사망이면 LostStoredRelics 를 창고에서 지움
	void FinishSettlement(FName KeptRelic, int32 GoldEarned);

	// 게임을 켤 때 세이브가 조작 / 손상돼서 백업으로 복구했거나 새로 시작했으면 그 안내. 꺼내면 비워짐 (메인 메뉴 팝업)
	FText ConsumeLoadNotice();

	// 창고 내용(보유 스킬 / 보관 유물)이 바뀜. 창고 화면 갱신용
	DECLARE_MULTICAST_DELEGATE(FOnStorageChanged);
	FOnStorageChanged OnStorageChanged;

	// 골드가 바뀜 (메인 메뉴 등 표시 갱신용)
	UPROPERTY(BlueprintAssignable, Category = "Terminus|Gold")
	FOnGoldChanged OnGoldChanged;

	// 보유할 수 있는 스킬인가 (DT 에 있고, 기본 스킬 / 몬스터 스킬이 아님). 직업은 안 봄
	static bool IsOwnableSkill(FName SkillRow);

	// 이 직업이 런 시작 때 장착할 수 있는가
	// 기획(UI 레퍼런스 > 게임 시작): 카테고리가 Common / Event 면 누구나, Personal 이면 OwnerClass 가 같을 때만
	static bool IsEquippableSkill(FName SkillRow, ECharacterClass InClass);

private:
	void Save();

	FClassUpgrades& FindOrAddUpgrades(ECharacterClass InClass);

	UPROPERTY()
	TObjectPtr<UTerminusProfileSave> Profile;

	FText LoadNotice;
};
