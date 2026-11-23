// 스킬 한 개를 정의하는 열거형과 행 (언리얼 Data/SkillTypes.h 이식)
// 전부 skill_data.xlsx 기준이고, 열거형 값 이름과 순서도 그 파일 그대로 맞춤
// 값 이름 바꾸면 CSV 읽을 때 그 행이 조용히 틀어짐. 새 값은 맨 뒤에만

namespace Terminus.Data
{
    public enum SkillOwner
    {
        Fighter,
        Engineer,
        Paladin,
        Assassin,
        // 공용 스킬. CharacterClass 에 끼우면 직업 반복문까지 같이 늘어서 따로 뺌
        Shared,
        Monster,
        // 이벤트 / 퀘스트에서만 주는 것. 누구나 가질 수 있지만 보상 / 상점 후보엔 안 나옴
        Event,
        Quest,
    }

    // 스킬을 어떻게 획득하는지
    public enum SkillCategory
    {
        Basic,
        Common,
        Personal,
        Event,
    }

    // 주점 UI 의 공격/방어/특수 세 칸
    public enum SkillType
    {
        Attack,
        Defense,
        Special,
    }

    // 획득 가능한 층. 별 색깔과 1:1 -> 표층 회색, 중층 노랑, 심층 빨강
    public enum SkillTier
    {
        Surface,
        Mid,
        Deep,
    }

    public enum TargetType
    {
        Self,
        SingleEnemy,
        AllEnemies,
        SingleAlly,
        AllAllies,
    }

    // 엑셀 순서가 ATK, DEF, None 이라 None 이 마지막 -> 행 기본값을 None 으로 직접 박아둠
    public enum ScalingStat
    {
        ATK,
        DEF,
        None,
    }

    public enum StatusEffect
    {
        None,
        Counter,         // State_01  반격
        Mark,            // State_02  급소 지정
        Evasion,         // State_03  민첩
        Poison,          // State_04  중독
        Brave,           // State_05  용기
        Protection,      // State_06  가호
        Ice,             // State_07  얼음
        Lava,            // State_08  용암
        Metal,           // State_09  강철
        Acid,            // State_10  산성
        Fear,            // State_11  공포
        Fusion,          // State_12  퓨전
        Vitality,        // State_13  활력
        Reflux,          // State_14  역류
        InternalInjury,  // State_15  내상
        Immortality,     // State_16  불사
        Absorption,      // State_17  흡수
        // 유물 데이터에서 추가
        Solid,           // 견고 (가호 그룹)
        Indomitable,     // 불굴. 한 사이클 받는 데미지 1 고정 (강철 그룹)
        IronWall,        // 철벽. 받는 데미지가 수치만큼 줄어듦
        // 10-24 엑셀 Enum_State. 테마 이름만 다르고 효과는 같은 그룹
        Dark,            // 과욕(어둠)   = 산성 그룹
        Flooding,        // 허약(침수)   = 얼음 그룹
        Contrarywind,    // 공포(역풍)   = 공포 그룹
        Spirit,          // 불굴(정령화) = 불굴 그룹
        Blindness,       // 불굴(실명)   = 불굴 그룹
        SelfDamage,      // 반동 피해. 상태로 남지 않음
    }

    // 실제 효과를 실행하는 로직 종류. 엑셀 Enum_Reference 시트 순서 그대로
    // 실행부는 원자(피해 보호막 회복 상태부여 반동 에너지)만 봄 -> Combat.SkillExecutor.GetRecipe
    public enum ActionKind
    {
        HitEnemy,
        HitEnemyMulti,
        HitEnemySingleAoe,
        HitAllEnemies,
        StatusMark,
        StatusPoison,
        HitEnemyMark,
        HitEnemyPoison,
        DmgSelf,
        HitEnemySelfDmg,
        ShieldSelf,
        CounterSelf,
        ShieldSelfCounter,
        ShieldAll,
        HealAlly,
        HealAll,
        EvasionSelf,
        StatusPoisonAll,
        EvasionSelfPoisonAll,
        GainEnergy,
        BraveSelf,
        BraveAll,
        HitEnemyProtect,
        ProtectSelf,
        HitEnemyShield,
        StatusLava,
        StatusLavaAll,
        StatusIce,
        StatusIceAll,
        ShieldSelfPoisonAll,
        MetalSelf,
        HitAllEnemiesLavaAll,
        HitAllEnemiesIceAll,
        HitAllEnemiesPoisonAll,
        StatusAcid,
        StatusAcidAll,
        HitAllEnemiesAcidAll,
        StatusFear,
        StatusFearAll,
        HitAllEnemiesFearAll,
        StatusFusion,
        StatusFusionAll,
        HitAllEnemiesFusionAll,
        HealAllBraveAll,

        // 유물 데이터에서 추가
        RevivalSelfAll,
        RevivalSelfHalf,
        IndomitableSelfAllyDead,
        HealSelf,
        GainMoney,
        RandomUpgrade,
        IndomitableSelf,
        SolidSelf,
        MaxHPUp,
        MaxHPDown,
        StatusMarkAll,
        IronWallSelf,
        EvasionDamageReduction,

        // 10-24 엑셀
        HitAllEnemiesMulti,
        SolidAll,
        HitAllEnemiesShield,
        HitAllEnemiesMark,
        ShieldSelfSolid,
        HitEnemyLava,
        HitEnemyIce,
        HitEnemyAcid,
        HitEnemyFear,
        StatusInternalInjury,
        StatusInternalInjuryAll,
        HitEnemyInternalInjury,
        HitAllEnemiesInternalInjuryAll,
        AbsorptionSelf,
        ShieldSelfAbsorption,
        ShieldSelfProtect,
        ShieldSelfEvasion,
        ShieldSelfIronWall,
        HealSelfProtect,
        HitEnemyHealSelf,
        HitAllEnemiesHealSelf,
        HitEnemyHealSelfIce,
        HitAllEnemiesHealSelfIceAll,
        HitEnemyShieldMetal,
        HitAllEnemiesShieldMetal,
        BlindnessSelf,
        SpiritSelf,
        StatusDark,
        StatusDarkAll,
        HitEnemyDark,
        HitAllEnemiesDarkAll,
        StatusFlooding,
        StatusFloodingAll,
        HitEnemyFlooding,
        HitAllEnemiesFloodingAll,
        StatusContrarywind,
        StatusContrarywindAll,
        HitEnemyContrarywind,
        HitAllEnemiesContrarywindAll,
        GainEnergyProtectSelf,
        GainEnergyIndomitableSelf,
        BraveSelfDmg,
        HitEnemySelfDmgDrain,
        HitEnemyMultiSelfDmgDrain,
        HitAllSelfDmgDrain,
        BonusDamageOnShield,
        HealSelfBraveSelf,
        ShieldSelfBraveSelf,
        HitEnemySingleAoeLava,
        HitEnemySingleAoeMark,
        GainEnergyBraveSelf,
        ShieldAllSolidAll,
        BraveAllSolidAll,
        HitEnemyMultiMark,
        HitEnemyEvasionSelf,
        EvasionSelfBraveSelf,
        EvasionSelfCounterSelf,
        HitEnemySingleAoePoison,
        GainEnergyBlindnessSelf,
        StatusMarkFearAll,
    }

    /// <summary>
    /// 스킬 한 개의 정의. DT_Skill.csv 한 행과 1:1.
    /// 필드 이름 = CSV 컬럼 이름. 하나라도 다르면 그 열은 에러 없이 버려진다.
    /// RowName 은 표의 키 (CsvTable 이 채움).
    /// </summary>
    [System.Serializable]
    public sealed class SkillRow : ITableRow
    {
        public string RowName { get; set; }

        // --- 식별
        // 기획서 추적용 코드. 몬스터 테이블이 이 값으로 스킬을 참조하기도 함
        public string SkillID = "";
        public string DisplayName_KR = "";

        // --- 분류
        public SkillOwner OwnerClass = SkillOwner.Shared;
        public SkillCategory SkillCategory = SkillCategory.Basic;
        public SkillType SkillType = SkillType.Attack;
        public SkillTier SkillTier = SkillTier.Surface;

        // --- 실행
        public ActionKind ActionKind = ActionKind.HitEnemy;
        public TargetType TargetType = TargetType.SingleEnemy;

        // --- 비용
        // 매 턴 최대치까지 회복되는 에너지. 기본 스킬은 보통 1
        public int EnergyCost = 1;
        // 던전에서 주운 스킬만 쓰는 자원. 기본 스킬은 0
        public int SkillEnergyCost = 0;

        // --- 수치
        // 스텟 보정 전 기본 수치. 피해든 보호막이든 회복이든 다 여기서 시작
        public int BaseValue = 0;
        public ScalingStat ScalingStat = ScalingStat.None;
        // 최종값 = BaseValue + 스텟 * ScalingRatio
        public float ScalingRatio = 1f;
        // 타수. 1이면 단타
        public int HitCount = 1;
        // 조합형이 쓰는 두 번째 수치 (광역 피해 계수 등). 0이면 안 씀
        public int SecondaryValue = 0;

        // --- 상태 효과
        public StatusEffect StatusEffect = StatusEffect.None;
        public int StatusValue = 0;
        // 지속 사이클 수. 턴이 아니라 사이클 기준
        public int StatusDuration = 0;
        // 시전자가 대가로 받는 피해
        public int SelfDamage = 0;

        // --- 연출 (태그)
        [NameField] public string IconID = "";
        [NameField] public string AnimMontageTag = "";
        [NameField] public string VFXTag = "";
        [NameField] public string SFXTag = "";

        // --- 문서
        public string Description_KR = "";
        // 기획 메모. 런타임에선 안 씀
        public string Notes = "";

        // --- 가격 (던전 재화). 기본 스킬은 0 / 0
        public int BuyPrice_Dungeon = 0;
        public int SellPrice_Dungeon = 0;

        // 강화 단계를 더할 땐 표의 행을 직접 고치지 말고 복사본에 (RunPlayer.ApplyBasicSkillUpgrade)
        public SkillRow Clone() => (SkillRow)MemberwiseClone();
    }

    /// <summary>상태이상 그룹 / 이름 / 설명 (언리얼 SkillTypes.cpp)</summary>
    public static class StatusInfo
    {
        // 같은 그룹 = 이름만 다르고 효과가 같음, 한 번에 하나만 걸림. 그룹의 대표값을 돌려줌
        public static StatusEffect GetGroup(StatusEffect type)
        {
            switch (type)
            {
                case StatusEffect.Dark: return StatusEffect.Acid;
                case StatusEffect.Flooding: return StatusEffect.Ice;
                case StatusEffect.Solid: return StatusEffect.Protection;
                case StatusEffect.Metal:
                case StatusEffect.Spirit:
                case StatusEffect.Blindness: return StatusEffect.Indomitable;
                case StatusEffect.Contrarywind: return StatusEffect.Fear;
                default: return type;
            }
        }

        // 화면 표시 이름. "시스템 이름(테마 이름)", 둘이 같으면 시스템 이름만
        public static string GetDisplayName(StatusEffect type)
        {
            string Name(string system, string theme) => system == theme ? system : system + "(" + theme + ")";

            switch (type)
            {
                case StatusEffect.Counter: return Name("반격", "반격");
                case StatusEffect.Mark: return Name("급소", "급소");
                case StatusEffect.Evasion: return Name("민첩", "민첩");
                case StatusEffect.Poison: return Name("중독", "중독");
                case StatusEffect.Brave: return Name("용기", "용기");
                case StatusEffect.Protection: return Name("견고", "가호");
                case StatusEffect.Ice: return Name("허약", "얼음");
                case StatusEffect.Lava: return Name("허점", "용암");
                case StatusEffect.Metal: return Name("불굴", "강철");
                case StatusEffect.Acid: return Name("과욕", "산성");
                case StatusEffect.Fear: return Name("공포", "공포");
                case StatusEffect.Fusion: return Name("퓨전", "퓨전");
                case StatusEffect.Vitality: return Name("활력", "활력");
                case StatusEffect.Reflux: return Name("역류", "역류");
                case StatusEffect.InternalInjury: return Name("내상", "내상");
                case StatusEffect.Immortality: return Name("불사", "불사");
                case StatusEffect.Absorption: return Name("흡수", "흡수");
                case StatusEffect.Solid: return Name("견고", "견고");
                case StatusEffect.Indomitable: return Name("불굴", "불굴");
                case StatusEffect.IronWall: return Name("철벽", "철벽");
                case StatusEffect.Dark: return Name("과욕", "어둠");
                case StatusEffect.Flooding: return Name("허약", "침수");
                case StatusEffect.Contrarywind: return Name("공포", "역풍");
                case StatusEffect.Spirit: return Name("불굴", "정령화");
                case StatusEffect.Blindness: return Name("불굴", "실명");
                case StatusEffect.SelfDamage: return Name("반동 피해", "반동 피해");
                default: return "";
            }
        }

        // 효과 설명 (Enum_State "효과" 열). 같은 그룹은 문구도 같음
        public static string GetDescription(StatusEffect type)
        {
            switch (GetGroup(type))
            {
                case StatusEffect.Counter: return "자신을 공격한 대상에게 수치만큼의 피해를 돌려준다.";
                case StatusEffect.Mark: return "받는 데미지가 25% 증가한다.";
                case StatusEffect.Evasion: return "수치만큼 회피 스텟이 증가한다.";
                case StatusEffect.Poison: return "자신의 턴 종료 시 수치만큼의 데미지를 받는다.";
                case StatusEffect.Brave: return "수치만큼 공격 스텟이 오른다.";
                case StatusEffect.Protection: return "수치만큼 방어 스텟이 오른다.";
                case StatusEffect.Ice: return "수치만큼 공격 스텟이 하락한다.";
                case StatusEffect.Lava: return "수치만큼 방어 스텟이 하락한다.";
                case StatusEffect.Indomitable: return "한 사이클 동안 받는 데미지가 1로 고정된다.";
                case StatusEffect.Acid: return "스킬 사용 시 에너지 소모량이 1 증가한다.";
                case StatusEffect.Fear: return "주는 최종 데미지가 25% 감소한다.";
                case StatusEffect.Fusion: return "중독, 허점(용암), 허약(얼음) 중 하나를 무작위로 건다.";
                case StatusEffect.Vitality: return "자신의 턴 시작 시 에너지를 수치만큼 추가로 얻는다.";
                case StatusEffect.Reflux: return "에너지를 사용할 때마다 수치만큼 데미지를 받는다.";
                case StatusEffect.InternalInjury: return "스킬 에너지를 사용할 때마다 수치만큼 데미지를 받는다.";
                case StatusEffect.Immortality: return "체력이 1 아래로 내려가지 않는다.";
                case StatusEffect.Absorption: return "데미지를 받으면 받은 데미지의 수치%만큼 회복한다.";
                case StatusEffect.IronWall: return "받는 데미지가 수치만큼 줄어든다.";
                case StatusEffect.SelfDamage: return "스킬 사용 시 시전자가 수치만큼 반동 피해를 받는다.";
                default: return "";
            }
        }

        // 해로운 상태인지 (Enum_State "종류" 열이 디버프)
        public static bool IsDebuff(StatusEffect type)
        {
            switch (GetGroup(type))
            {
                case StatusEffect.Mark:
                case StatusEffect.Poison:
                case StatusEffect.Ice:
                case StatusEffect.Lava:
                case StatusEffect.Acid:
                case StatusEffect.Fear:
                case StatusEffect.Fusion:
                case StatusEffect.Reflux:
                case StatusEffect.InternalInjury:
                case StatusEffect.SelfDamage:
                    return true;
                default:
                    return false;
            }
        }
    }
}
