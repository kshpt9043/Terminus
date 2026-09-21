#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/CharacterTypes.h"
#include "Data/SkillTypes.h"
#include "TerminusDataSettings.generated.h"

class UDataTable;

/**
 * 프로젝트에 하나뿐인 마스터 데이터 테이블을 가리키는 곳.
 * 주점이든 던전이든 여기서 꺼내 쓴다 -> 위젯이 DT 를 들고 있으면 던전에서 손이 안 닿아서.
 *
 * 값은 Config/DefaultGame.ini 에 저장되고
 * 에디터 Project Settings > Game > Terminus Data 에 항목으로 뜬다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Terminus Data"))
class TERMINUS_API UTerminusDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 호출부를 짧게 하려고. GetDefault 를 매번 쓰면 클래스 이름이 여기저기 박힌다
	static const UTerminusDataSettings* Get() { return GetDefault<UTerminusDataSettings>(); }

	/**
	 * 열거형으로 캐릭터 행을 찾는 유일한 창구.
	 * 주점이든 던전이든 여기로만 들어온다 -> 조회 방식을 바꿀 때 고칠 데가 한 군데
	 */
	static const FCharacterClassRow* FindCharacterClassRow(ECharacterClass InClass);
	
	/**
	 * 캐릭터 클래스 정의 테이블.
	 * 소프트 참조인 이유 -> config 는 엔진 아주 초기에 읽히는데,
	 * 하드 참조면 그 시점에 에셋을 로드하려 들어서 위험하다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Character",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/Terminus.CharacterClassRow"))
	TSoftObjectPtr<UDataTable> CharacterClassTable;
	
	// 같은 방식으로 스킬을 찾는 법
	static const FSkillRow* FindSkillRow(FName RowName);
	
	UPROPERTY(Config, EditAnywhere, Category = "Skill",
	meta = (RequiredAssetDataTags = "RowStructure=/Script/Terminus.SkillRow"))
	TSoftObjectPtr<UDataTable> SkillTable;
};