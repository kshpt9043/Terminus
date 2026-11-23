// 유물 (언리얼 Data/RelicTypes.h 이식). skill_data.xlsx 의 Relic_DataTable 시트 기준
namespace Terminus.Data
{
    public enum RelicTier
    {
        Basic,   // 직업 / 몬스터가 처음부터 가진 패시브. 추가 획득 불가 (RLC_<직업>_001)
        Upgrade, // Basic 의 강화판. 얻으면 베이스 Basic 유물은 사라짐
        Surface, // 1~2층
        Mid,     // 3~4층
        Deep,    // 5~6층
    }

    // 유물 효과가 발동하는 시점 (Enum_TriggerTiming 시트)
    public enum RelicTrigger
    {
        OnBattleStart,
        OnTurnStart,            // 자기 턴 시작. 에너지가 최대치로 회복된 뒤
        OnHitEnemy,             // 적의 체력을 깎았을 때
        OnTakeDamage,           // 내 체력이 깎였을 때
        OnGainShield,
        OnSkillUsed,            // OnSkill 에 적힌 스킬을 쓴 직후
        OnAllyTakeDamage,
        OnBattleEnd,
        OnTurnEnd,
        OnDead,
        OnUseEnergy,
        OnUseSkillEnergy,
        OnGainRelic,            // 얻었을 때 (팔아도 효과 유지)
        OnRestStart,
        OnGuardianBattleStart,
        OnBossBattleStart,
        OnHealHP,
        OnHitShield,            // 보호막 있는 대상을 공격했을 때
    }

    /// <summary>
    /// 유물 한 개. DT_Relic.csv 한 행 (행 이름 = RLC_Common_001 같은 코드).
    /// 가격 -1: 구매가면 상점에서 살 수 없음, 판매가면 팔 수 없음. 판매가 0 은 0원에 팔 수 있음
    /// </summary>
    [System.Serializable]
    public sealed class RelicRow : ITableRow
    {
        public string RowName { get; set; }

        // --- 표시
        public string RelicName = "";
        public string RelicDesc = "";
        // 언리얼 텍스처 경로. 유니티에선 파일 이름(T_Relic_...)으로 스프라이트를 찾음
        public string Icon = "";

        // --- 분류
        public RelicTier RelicTier = RelicTier.Surface;
        public SkillOwner OwnerClass = SkillOwner.Shared;

        // --- 효과
        public RelicTrigger TriggerTiming = RelicTrigger.OnBattleStart;
        public ActionKind ActionKind = ActionKind.HitEnemy;
        public TargetType TargetType = TargetType.Self;
        public int BaseValue = 0;
        public ScalingStat ScalingStat = ScalingStat.None;
        public float ScalingRatio = 0f;
        public StatusEffect StatusEffect = StatusEffect.None;
        public int StatusValue = 0;
        // 지속 사이클 수. -1 = 전투 끝까지
        public int StatusDuration = 0;
        // OnSkillUsed 일 때 대상 스킬. DT_Skill 행 이름 / SkillID / 스킬 종류(Attack, Defense, Special)
        [NameField] public string OnSkill = "";
        // 일시적 이벤트 유물의 유지 전투 수. -1 = 영구
        public int RemainBattle = -1;

        // --- 가격
        public int BuyPrice_Dungeon = -1;
        public int SellPrice_Dungeon = -1;
        // 정산 판매 기본가 (골드). 세력별로 -5%~+5%
        public int SellPrice_Gold = -1;

        // 상점에 나올 수 있는가 (Basic / Upgrade 는 가격과 상관없이 안 나옴)
        public bool CanBuyInShop() => BuyPrice_Dungeon >= 0 && RelicTier != RelicTier.Basic && RelicTier != RelicTier.Upgrade;
        public bool CanSellInShop() => SellPrice_Dungeon >= 0;
        public bool CanSellAtSettlement() => SellPrice_Gold >= 0;
    }
}
