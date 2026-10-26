// 스킬 실행기 / 피해 계산 자동 테스트
// 실행: 에디터 Session Frontend > Automation 에서 "Terminus" 또는
//   UnrealEditor-Cmd Terminus.uproject -ExecCmds="Automation RunTests Terminus;Quit" -unattended -nullrhi
// 월드 없이 액터 + 컴포넌트만 만들어서 돌림 (데미지 계산은 서버 = 권한 있는 액터면 됨)

#include "Misc/AutomationTest.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/SkillExecutor.h"
#include "Data/TerminusDataSettings.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Data/MonsterTypes.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Engine/Texture2D.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerminusTest
{
	UCombatStatsComponent* MakeBattler(int32 MaxHealth = 100, int32 Attack = 0, int32 Defense = 0, int32 Evasion = 0)
	{
		AActor* Owner = NewObject<AActor>(GetTransientPackage());
		UCombatStatsComponent* Stats = NewObject<UCombatStatsComponent>(Owner);
		FCharacterStats Base;
		Base.MaxHealth = MaxHealth;
		Base.Attack = Attack;
		Base.Defense = Defense;
		Base.Evasion = Evasion;
		Base.MaxEnergy = 3;
		Base.MaxSkillEnergy = 4;
		Stats->InitFrom(Base);
		return Stats;
	}

	int32 Health(const UCombatStatsComponent* S) { return S->GetCombatState().Health; }
	int32 Shield(const UCombatStatsComponent* S) { return S->GetCombatState().Shield; }
}

using namespace TerminusTest;

// DT_Skill 의 모든 행이 터지지 않고 돌고, 피해 스킬은 적 체력을 깎는지
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerminusAllSkillsTest, "Terminus.Skill.AllRowsExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerminusAllSkillsTest::RunTest(const FString& Parameters)
{
	const UDataTable* Table = UTerminusDataSettings::Get()->SkillTable.LoadSynchronous();
	if (!TestNotNull(TEXT("DT_Skill"), Table)) return false;

	int32 Count = 0;
	for (const FName& Row : Table->GetRowNames())
	{
		const FSkillRow* Skill = UTerminusDataSettings::FindSkillRow(Row);
		if (!Skill) continue;

		UCombatStatsComponent* Caster = MakeBattler(100, 1, 1);
		UCombatStatsComponent* Ally = MakeBattler(100);
		UCombatStatsComponent* EnemyA = MakeBattler(1000);
		UCombatStatsComponent* EnemyB = MakeBattler(1000);
		Caster->ApplyDamage(30);   // 회복 스킬이 보이게
		Ally->ApplyDamage(30);
		const int32 CasterBefore = Health(Caster);

		TArray<UCombatStatsComponent*> Targets;
		switch (Skill->TargetType)
		{
		case ETargetType::Self:        Targets = { Caster }; break;
		case ETargetType::SingleEnemy: Targets = { EnemyA }; break;
		case ETargetType::AllEnemies:  Targets = { EnemyA, EnemyB }; break;
		case ETargetType::SingleAlly:  Targets = { Ally }; break;
		case ETargetType::AllAllies:   Targets = { Caster, Ally }; break;
		}

		FSkillExecutor::Execute(*Skill, Caster, Targets, { EnemyA, EnemyB });
		++Count;

		bool bDamage, bShield, bHeal, bOnCaster;
		FSkillExecutor::DescribeValueUse(Skill->ActionKind, bDamage, bShield, bHeal, bOnCaster);
		if (bDamage && Skill->BaseValue > 0)
		{
			TestTrue(FString::Printf(TEXT("%s: 적 체력이 깎임"), *Row.ToString()), Health(EnemyA) < 1000);
		}
		if (bShield && Skill->BaseValue > 0)
		{
			TestTrue(FString::Printf(TEXT("%s: 보호막 생김"), *Row.ToString()), Shield(bOnCaster ? Caster : Targets[0]) > 0);
		}
		if (bHeal && bOnCaster && Skill->BaseValue > 0)
		{
			TestTrue(FString::Printf(TEXT("%s: 시전자 회복"), *Row.ToString()), Health(Caster) > CasterBefore - Skill->SelfDamage);
		}
		if (Skill->StatusEffect != EStatusEffect::None && Skill->StatusEffect != EStatusEffect::SelfDamage && Skill->StatusDuration != 0)
		{
			const bool bAny = Caster->HasStatus(Skill->StatusEffect) || Ally->HasStatus(Skill->StatusEffect)
				|| EnemyA->HasStatus(Skill->StatusEffect) || EnemyB->HasStatus(Skill->StatusEffect)
				|| Skill->StatusEffect == EStatusEffect::Fusion;
			TestTrue(FString::Printf(TEXT("%s: 상태 %s 걸림"), *Row.ToString(), *UEnum::GetValueAsString(Skill->StatusEffect)), bAny);
		}
	}

	AddInfo(FString::Printf(TEXT("스킬 %d개 실행"), Count));
	return Count > 0;
}

// 모든 몬스터의 스킬 칸이 실제 스킬 행을 가리키는지 (10-26: SkillID 로 적힌 칸 때문에 몬스터가 아무 행동도 안 하던 버그)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerminusMonsterSkillRefsTest, "Terminus.Data.MonsterSkillRefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerminusMonsterSkillRefsTest::RunTest(const FString& Parameters)
{
	const UDataTable* Table = UTerminusDataSettings::Get()->MonsterTable.LoadSynchronous();
	if (!TestNotNull(TEXT("DT_Monster"), Table)) return false;

	int32 Checked = 0;
	Table->ForeachRow<FMonsterRow>(TEXT("MonsterSkillRefs"), [&](const FName& Key, const FMonsterRow& Row)
	{
		for (const FName& Skill : { Row.Skill1_ID, Row.Skill2_ID, Row.Skill3_ID })
		{
			if (Skill.IsNone()) continue;
			++Checked;
			TestNotNull(FString::Printf(TEXT("%s: 스킬 '%s' 이 DT_Skill 행"), *Key.ToString(), *Skill.ToString()), UTerminusDataSettings::FindSkillRow(Skill));
		}
	});
	AddInfo(FString::Printf(TEXT("몬스터 스킬 칸 %d개 확인"), Checked));
	return Checked > 0;
}

// 몬스터 외형 표: 플립북 6개가 다 있고, 스프라이트가 시트의 정사각형 칸 하나씩을 가리키는지
// (10-26: 템플릿 스프라이트의 텍스처를 바꾸면 UV 가 예전 텍스처 비율로 틀어져 옆 프레임과 섞여 보이던 문제)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerminusMonsterVisualTest, "Terminus.Data.MonsterVisuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerminusMonsterVisualTest::RunTest(const FString& Parameters)
{
	const UDataTable* Table = UTerminusDataSettings::Get()->MonsterVisualTable.LoadSynchronous();
	if (!TestNotNull(TEXT("DT_MonsterVisual"), Table)) return false;

	int32 Sprites = 0;
	Table->ForeachRow<FMonsterVisualRow>(TEXT("MonsterVisuals"), [&](const FName& Key, const FMonsterVisualRow& Row)
	{
		TestNotNull(FString::Printf(TEXT("%s: DT_Monster 에 있음"), *Key.ToString()), UTerminusDataSettings::FindMonsterRow(Key));

		for (const TSoftObjectPtr<UPaperFlipbook>& Ref : { Row.Idle, Row.Attack, Row.Hit, Row.Defend, Row.Special, Row.Death })
		{
			const UPaperFlipbook* Flipbook = Ref.LoadSynchronous();
			if (!TestNotNull(FString::Printf(TEXT("%s: 플립북 %s"), *Key.ToString(), *Ref.ToString()), Flipbook)) continue;
			TestTrue(FString::Printf(TEXT("%s: %s 프레임 있음"), *Key.ToString(), *Flipbook->GetName()), Flipbook->GetNumKeyFrames() > 0);

			for (int32 i = 0; i < Flipbook->GetNumKeyFrames(); ++i)
			{
				const UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(i).Sprite;
				if (!TestNotNull(TEXT("스프라이트"), Sprite)) continue;
				++Sprites;

				const FVector2D Size = Sprite->GetSourceSize();
				const FVector2D UV = Sprite->GetSourceUV();
				// 원본 이미지 크기로 비교 (-nullrhi 에선 GetSizeX/Y 가 0)
				const UTexture2D* Texture = Sprite->GetSourceTexture();
				const FIntPoint TexSize = Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
				const bool bSquare = FMath::IsNearlyEqual(Size.X, Size.Y) && FMath::IsNearlyEqual(Size.Y, (float)TexSize.Y);
				const bool bOnGrid = Size.X > 0 && FMath::IsNearlyEqual(FMath::Fmod(UV.X, Size.X), 0.0, 0.01) && FMath::IsNearlyZero(UV.Y)
					&& UV.X + Size.X <= TexSize.X + 0.01;
				TestTrue(FString::Printf(TEXT("%s: 시트 칸 하나 (UV %.1f, 크기 %.1f x %.1f)"), *Sprite->GetName(), UV.X, Size.X, Size.Y), bSquare && bOnGrid);
			}
		}
	});
	AddInfo(FString::Printf(TEXT("스프라이트 %d개 확인"), Sprites));
	return Sprites > 0;
}

// 조합형 / 새 규칙 수치 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerminusSkillRulesTest, "Terminus.Skill.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerminusSkillRulesTest::RunTest(const FString& Parameters)
{
	// 마도 캐논: 주 대상 (5+공격), 상대 전체 (2+공격) 추가
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::HitEnemySingleAoe;
		Skill.TargetType = ETargetType::SingleEnemy;
		Skill.BaseValue = 5;
		Skill.ScalingStat = EScalingStat::ATK;
		Skill.SecondaryValue = 2;
		UCombatStatsComponent* Caster = MakeBattler(100, 1);
		UCombatStatsComponent* A = MakeBattler();
		UCombatStatsComponent* B = MakeBattler();
		FSkillExecutor::Execute(Skill, Caster, { A }, { A, B });
		TestEqual(TEXT("광역: 주 대상 6 + 3"), Health(A), 100 - 9);
		TestEqual(TEXT("광역: 나머지 3"), Health(B), 100 - 3);
	}

	// 피해 + 시전자 보호막 (Secondary + 방어)
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::HitEnemyShield;
		Skill.BaseValue = 6;
		Skill.ScalingStat = EScalingStat::ATK;
		Skill.SecondaryValue = 3;
		UCombatStatsComponent* Caster = MakeBattler(100, 0, 2);
		UCombatStatsComponent* A = MakeBattler();
		FSkillExecutor::Execute(Skill, Caster, { A });
		TestEqual(TEXT("공격+보호막: 피해 6"), Health(A), 94);
		TestEqual(TEXT("공격+보호막: 보호막 3+2"), Shield(Caster), 5);
	}

	// 흡혈 다단: 체력을 깎은 타격마다 Secondary 회복, 반동은 SelfDamage
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::HitEnemyMultiSelfDmgDrain;
		Skill.BaseValue = 7;
		Skill.ScalingStat = EScalingStat::ATK;
		Skill.HitCount = 2;
		Skill.SecondaryValue = 3;
		Skill.SelfDamage = 3;
		UCombatStatsComponent* Caster = MakeBattler();
		Caster->ApplyDamage(20);
		UCombatStatsComponent* A = MakeBattler();
		FSkillExecutor::Execute(Skill, Caster, { A });
		TestEqual(TEXT("흡혈: 적 7x2"), Health(A), 86);
		TestEqual(TEXT("흡혈: 80 + 6 - 3"), Health(Caster), 83);
	}

	// 공격하면서 자기 가호 (예전엔 적에게 걸렸음)
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::HitEnemyProtect;
		Skill.BaseValue = 4;
		Skill.StatusEffect = EStatusEffect::Protection;
		Skill.StatusValue = 1;
		Skill.StatusDuration = 1;
		UCombatStatsComponent* Caster = MakeBattler();
		UCombatStatsComponent* A = MakeBattler();
		FSkillExecutor::Execute(Skill, Caster, { A });
		TestTrue(TEXT("가호는 시전자에게"), Caster->HasStatus(EStatusEffect::Protection));
		TestFalse(TEXT("적에겐 안 걸림"), A->HasStatus(EStatusEffect::Protection));
	}

	// 용기 + 견고 같이
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::BraveAllSolidAll;
		Skill.TargetType = ETargetType::AllAllies;
		Skill.StatusEffect = EStatusEffect::Brave;
		Skill.StatusValue = 2;
		Skill.StatusDuration = 2;
		UCombatStatsComponent* Caster = MakeBattler();
		FSkillExecutor::Execute(Skill, Caster, { Caster });
		TestEqual(TEXT("용기 2"), Caster->GetStatusValue(EStatusEffect::Brave), 2);
		TestEqual(TEXT("견고 2"), Caster->GetStatusValue(EStatusEffect::Solid), 2);
	}

	// 은신: (StatusValue + 방어) 만큼 민첩
	{
		FSkillRow Skill;
		Skill.ActionKind = EActionKind::EvasionSelf;
		Skill.TargetType = ETargetType::Self;
		Skill.ScalingStat = EScalingStat::DEF;
		Skill.StatusEffect = EStatusEffect::Evasion;
		Skill.StatusValue = 1;
		Skill.StatusDuration = 3;
		UCombatStatsComponent* Caster = MakeBattler(100, 0, 2);
		FSkillExecutor::Execute(Skill, Caster, { Caster });
		TestEqual(TEXT("민첩 1+2"), Caster->GetStatusValue(EStatusEffect::Evasion), 3);
	}

	return true;
}

// 피해 계산: 상태 그룹 / 체술 / 보호막 관통
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerminusDamageRulesTest, "Terminus.Combat.DamageRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerminusDamageRulesTest::RunTest(const FString& Parameters)
{
	// 같은 그룹은 하나만: 얼음 다음 침수 -> 침수 하나
	{
		UCombatStatsComponent* S = MakeBattler();
		S->ApplyStatus(EStatusEffect::Ice, 1, 2);
		S->ApplyStatus(EStatusEffect::Flooding, 3, 2);
		TestEqual(TEXT("그룹: 항목 하나"), S->GetCombatState().Statuses.Num(), 1);
		TestEqual(TEXT("그룹: 얼음으로 물어도 침수 수치"), S->GetStatusValue(EStatusEffect::Ice), 3);
		TestEqual(TEXT("그룹: 표시 이름"), GetStatusDisplayName(S->GetCombatState().Statuses[0].Type), FString(TEXT("허약(침수)")));
	}

	// 불굴(정령화): 한 대 1 고정
	{
		UCombatStatsComponent* S = MakeBattler();
		S->ApplyStatus(EStatusEffect::Spirit, 0, 1);
		S->ApplyDamage(50);
		TestEqual(TEXT("정령화: 1 만 받음"), Health(S), 99);
	}

	// 체술: 회피 4. 3 은 무시, 보호막 5 가 먼저 받고 남은 5 에서 4 를 뺌
	{
		UCombatStatsComponent* S = MakeBattler(100, 0, 0, 4);
		S->SetEvasionReduction(true);
		S->ApplyDamage(3);
		TestEqual(TEXT("체술: 회피 이하 무시"), Health(S), 100);
		S->AddShield(5);
		S->ApplyDamage(10);
		TestEqual(TEXT("체술: 보호막 뒤 남은 피해에서 감소"), Health(S), 99);
		TestEqual(TEXT("체술: 보호막 다 깎임"), Shield(S), 0);
		S->ApplyStatus(EStatusEffect::Evasion, 2, 1);
		S->ApplyDamage(7);
		TestEqual(TEXT("체술: 민첩도 더함 (7-6)"), Health(S), 98);
	}

	// 체술 유물이 없으면 회피가 있어도 그대로 받음
	{
		UCombatStatsComponent* S = MakeBattler(100, 0, 0, 4);
		S->ApplyDamage(3);
		TestEqual(TEXT("체술 없음"), Health(S), 97);
	}

	// 보호막 관통: 공격자 +20%. 보호막 없는 대상엔 그대로
	{
		UCombatStatsComponent* Attacker = MakeBattler();
		Attacker->SetShieldBonusPercent(20);
		UCombatStatsComponent* T = MakeBattler();
		T->AddShield(100);
		T->ApplyDamage(10, Attacker);
		TestEqual(TEXT("관통: 12 흡수"), Shield(T), 88);
		UCombatStatsComponent* U = MakeBattler();
		U->ApplyDamage(10, Attacker);
		TestEqual(TEXT("관통: 보호막 없으면 10"), Health(U), 90);
	}

	// ApplyDamage 반환값 = 깎인 체력
	{
		UCombatStatsComponent* S = MakeBattler();
		S->AddShield(4);
		TestEqual(TEXT("반환: 보호막 뒤 6"), S->ApplyDamage(10), 6);
	}

	return true;
}

#endif
