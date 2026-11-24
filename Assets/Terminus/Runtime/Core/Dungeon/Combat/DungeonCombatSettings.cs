// 던전 전투 설정 (언리얼 UDungeonCombatComponent 의 UPROPERTY(EditAnywhere). BP_DungeonArea 의 Combat 컴포넌트에서 조정하던 값)
// 기본값 = 언리얼 기본값. 몬스터 클래스(MonsterClass)는 Game 쪽 프리팹이라 여기 없음
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Dungeon
{
    [System.Serializable]
    public sealed class DungeonCombatSettings
    {
        // ---- Combat|Monster

        // 몬스터방 몬스터 수 (최소, 최대). 기획: 1 ~ 3 마리. 구역 몬스터 슬롯 수를 넘지 않음
        public CombatIntRange NormalMonsterCount = new CombatIntRange(1, 3);

        // 멀티 체력 보정. 인덱스 = 이 방에 들어온 인원 - 1. 기획: 2인 70% / 3인 140% / 4인 220% 상승
        public List<float> PartyHealthBonus = new List<float> { 0f, 0.7f, 1.4f, 2.2f };

        // 몬스터(보스 제외) 개체별 체력 랜덤 보정. 기획: -5% ~ +10%
        // (헤더 주석은 '보스 제외' 지만 코드는 보스 포함 - 사용자 결정 10-08. SpawnMonsters 참고)
        public CombatFloatRange MonsterHealthRandom = new CombatFloatRange(-0.05f, 0.10f);

        // 몬스터(보스 제외) 공격 / 방어 랜덤 가산 최대치. 기획서 "최대 +2" (KSH 코드 주석은 +0~1 -> 기획 확인 필요)
        public int MonsterStatRandomMax = 2;

        // 몬스터 데이터에 스킬이 비어 있을 때 쓸 임시 스킬 (DT_Skill 행 이름)과 가중치
        // 지금 DT_Monster 의 Skill1~3 이 전부 비어 있어서 넣어 둠. 데이터가 채워지면 자동으로 그걸 씀
        public List<WeightedSkill> FallbackMonsterSkills = new List<WeightedSkill>
        {
            new WeightedSkill("slime_strike", 70),
            new WeightedSkill("condense", 30),
        };

        // 같이 나오는 몬스터 (키 = DT_Monster 행). 구역 몬스터 슬롯이 남는 만큼만 나옴
        // 수가 기획에 없는 것(족장 / 인도자)은 임시값. 노션 던전 테마 페이지의 '함께 소환' 규칙
        public Dictionary<string, MonsterCompanionRule> MonsterCompanions = new Dictionary<string, MonsterCompanionRule>
        {
            { "mon_slimeking", new MonsterCompanionRule(MonsterCategory.Guardian, 1) },          // 랜덤 가디언 1마리 (잡기 전엔 받는 피해 1: 유물 RLC_Monster_SLI03)
            { "mon_slimegeneral", new MonsterCompanionRule(MonsterCategory.Normal, 2) },         // 랜덤 일반 슬라임 2마리
            { "mon_lizardchieftain", new MonsterCompanionRule(MonsterCategory.Normal, 2) },      // 일반 몬스터와 함께 (수 미정, 임시 2)
            { "mon_lizardleader", new MonsterCompanionRule(MonsterCategory.Normal, 1) },         // 일반 몬스터와 함께 (수 미정, 임시 1)
            { "mon_ignis", new MonsterCompanionRule(MonsterCategory.Normal, 1, "mon_salamander") },   // 정령계 가디언 = 같은 속성 일반 1
            { "mon_nereid", new MonsterCompanionRule(MonsterCategory.Normal, 1, "mon_undine") },
            { "mon_jinn", new MonsterCompanionRule(MonsterCategory.Normal, 1, "mon_sylph") },
            { "mon_clay", new MonsterCompanionRule(MonsterCategory.Normal, 1, "mon_gnome") },
        };

        // ---- Combat|Balance

        // 테마의 두 번째 층(2 / 4 / 6층) 몬스터 / 가디언 기본 체력 보정 (보스 제외). 기획(10-07): +30%
        public float SecondFloorHealthBonus = 0.3f;

        // 테마의 두 번째 층 몬스터 / 가디언 공격 / 방어 가산 (보스 제외). 기획(10-07): +1
        public int SecondFloorStatBonus = 1;

        // 방에 들어온 플레이어 1명당 몬스터 공격 / 방어 가산 (보스 포함). 기획(10-06) + 사용자 결정(10-08): 혼자 +1 ~ 4명 +4
        public int PartyStatBonusPerPlayer = 1;

        // ---- Combat|Timing (초)

        // 몬스터 행동 사이 간격. 한꺼번에 하면 무슨 일이 일어났는지 안 보임
        public float MonsterActionInterval = 0.8f;

        // 다타 공격의 타격 사이 간격 (타격마다 이펙트가 끝나고 다음 피해). 0 이면 한꺼번에
        public float HitInterval = 0.75f;

        // 승리 / 패배 후 구역을 끝내기까지 대기
        public float CombatEndDelay = 1.5f;

        // ---- Combat|Reward (기획: 던전 재화 + 랜덤 강화 스킬 2개 중 1개, 안 골라도 됨)

        // 몬스터방 던전 재화 (최소, 최대)
        public CombatIntRange MonsterRewardCurrency = new CombatIntRange(10, 10);

        // 가디언 / 보스방 던전 재화 (사용자 결정 2026-10-06: 몬스터 10 / 가디언 20 / 보스 40. X~Y 사이 랜덤)
        public CombatIntRange GuardianRewardCurrency = new CombatIntRange(20, 20);
        public CombatIntRange BossRewardCurrency = new CombatIntRange(40, 40);

        // 보스방 픽업 스킬 후보 수 (기본 1 = 그 하나를 받거나 말거나). 언리얼 ClampMin 1
        public int BossRewardSkillChoices = 1;

        // 보상 스킬 후보 수. 언리얼 ClampMin 1
        public int RewardSkillChoices = 2;

        // 이미 장착한 스킬도 후보에 넣을지 (넣으면 같은 스킬을 두 칸에 낄 수 있음)
        public bool bRewardIncludeEquippedSkills = false;

        // 지금 층보다 높은 티어 스킬은 뺄지 (표층 = 모든 층, 중층 = 3층~, 심층 = 5층~)
        public bool bRewardExcludeHigherTier = true;

        // 이벤트 스킬을 뺄지 (기획: 이벤트 스킬은 이벤트 방에서만)
        public bool bRewardExcludeEventSkills = true;

        // 승리 후 보상 화면을 띄우기까지 대기(초). 마지막 공격이 보이게
        public float RewardDelay = 1f;

        // 전원이 보상을 마친 뒤 구역 클리어까지 (언리얼은 코드에 0.3f 로 박혀 있었음)
        public float RewardFinishDelay = 0.3f;

        public DungeonCombatSettings Clone()
        {
            var c = (DungeonCombatSettings)MemberwiseClone();
            c.NormalMonsterCount = NormalMonsterCount.Clone();
            c.PartyHealthBonus = new List<float>(PartyHealthBonus);
            c.MonsterHealthRandom = MonsterHealthRandom.Clone();
            c.FallbackMonsterSkills = FallbackMonsterSkills.ConvertAll(s => s.Clone());
            c.MonsterCompanions = new Dictionary<string, MonsterCompanionRule>();
            foreach (var pair in MonsterCompanions) c.MonsterCompanions.Add(pair.Key, pair.Value.Clone());
            c.MonsterRewardCurrency = MonsterRewardCurrency.Clone();
            c.GuardianRewardCurrency = GuardianRewardCurrency.Clone();
            c.BossRewardCurrency = BossRewardCurrency.Clone();
            return c;
        }
    }
}
