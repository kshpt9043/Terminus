// 몬스터 (언리얼 Data/MonsterTypes.h 이식). monster_data.xlsx 의 Monsters 시트 기준
namespace Terminus.Data
{
    public enum MonsterCategory
    {
        Normal,
        Guardian,
        Boss,
    }

    /// <summary>몬스터 한 마리의 정의. DT_Monster.csv 한 행. 필드 이름 = CSV 컬럼 이름</summary>
    [System.Serializable]
    public sealed class MonsterRow : ITableRow
    {
        public string RowName { get; set; }

        // --- 식별
        public string MonsterID = "";
        public string DisplayName_KR = "";

        // --- 분류
        public MonsterCategory MonsterCategory = MonsterCategory.Normal;
        public string Theme_KR = "";
        // 표층 1층 2층이 섞여 있어서 아직 열거형으로 못 뺌
        public string AppearFloor = "";

        // --- 스텟. 스폰 때 랜덤 보정은 스폰하는 쪽 몫
        public int BaseHP = 0;
        public int BaseATK = 0;
        public int BaseDEF = 0;

        // --- 스킬. DT_Skill 행 이름 (또는 SkillID -> GameData.ResolveSkillRow). 가중치는 고를 때 정규화
        [NameField] public string Skill1_ID = "";
        public int Skill1_Weight = 0;
        [NameField] public string Skill2_ID = "";
        public int Skill2_Weight = 0;
        [NameField] public string Skill3_ID = "";
        public int Skill3_Weight = 0;

        // --- 특수. 세미콜론으로 여러 개
        public string PassiveID = "";
        public string SpecialMechanic_Notes = "";

        // --- 연출 태그 (2D 라 스켈레탈 메시는 안 씀)
        [NameField] public string SkeletalMesh = "";
        [NameField] public string AnimMontageTag = "";

        public string Concept_KR = "";

        /// <summary>전투가 먹는 모양으로. 몬스터는 에너지를 안 써서 에너지 쪽은 기본값 그대로</summary>
        public CharacterStats ToStats()
        {
            return new CharacterStats { MaxHealth = BaseHP, Attack = BaseATK, Defense = BaseDEF };
        }
    }

    /// <summary>
    /// 몬스터 외형 (DT_MonsterVisual.csv, 행 이름 = DT_Monster 행 이름). 아트 쪽 표
    /// 언리얼 열은 플립북 경로라 유니티에선 Source(PixelLab 폴더 이름) 로 애니메이션을 찾는다.
    /// 그림은 오른쪽을 보는 원본 기준 (몬스터는 좌우 반전돼 왼쪽을 봄)
    /// </summary>
    [System.Serializable]
    public sealed class MonsterVisualRow : ITableRow
    {
        public string RowName { get; set; }

        public string Idle = "";
        public string Attack = "";
        public string Hit = "";
        public string Defend = "";
        public string Special = "";
        public string Death = "";
        // PixelLab/<이름>
        public string Source = "";
    }
}
