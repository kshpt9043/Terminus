#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CharacterTypes.h"
#include "TerminusProfileSubsystem.generated.h"

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

	UPROPERTY()
	TObjectPtr<UTerminusProfileSave> Profile;
};
