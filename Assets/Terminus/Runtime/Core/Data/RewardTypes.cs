// 이벤트 선택지 / 상점 / 방 보상 (언리얼 Data/RewardTypes.h 이식). 서버가 만들어 보내고 화면은 그대로 보여줌
using System.Collections.Generic;
using Terminus.Map;

namespace Terminus.Data
{
    // 이벤트 선택지 하나 (DT_EventChoice 한 행을 이 사람 기준으로 풀어 둔 것)
    [System.Serializable]
    public sealed class EventOption
    {
        // DT_EventChoice 행 이름
        public string ChoiceRow = "";
        public EventOutcomeType Outcome = EventOutcomeType.Leave;

        // 버튼 문구 / 대가와 결과 안내
        public string Title = "";
        public string Description = "";

        // 받을 스킬 / 유물 (미리 정해 둔 것). 없으면 ""
        public string SkillRow = "";
        public string RelicRow = "";

        // 재화 / 회복 수치 (범위는 미리 굴려 둠)
        public int Amount = 0;

        // 대가
        public int CostHealth = 0;
        public int CostMaxHealth = 0;
        public int CostCurrency = 0;
        public int CostSkills = 0;     // 장착 픽업 스킬 무작위로 잃음
        public int CostRelics = 0;     // 유물 무작위로 잃음
        public string CostRelic = "";  // 대가로 받는 디버프 유물 (GainDebuffRelic)

        // 툴팁에만 적힌 추가 결과
        public int BonusHeal = 0;
        public int BonusUpgrades = 0;
        public int BonusWinCurrency = 0;     // 쉬는 가디언을 이기면
        public bool bBonusWinRelic = false;  // 쉬는 가디언을 이기면 테마 유물

        // 고를 수 있나. 못 고르면 이유
        public bool bAvailable = true;
        public string UnavailableReason = "";

        public bool GivesSkill() => !string.IsNullOrEmpty(SkillRow);

        // 유물 칸을 하나 차지하는 선택지 (꽉 찼으면 버릴 유물부터 고름)
        public bool GivesRelic()
        {
            return !string.IsNullOrEmpty(RelicRow)
                && Outcome != EventOutcomeType.SwapRelicSameTierRandom
                && Outcome != EventOutcomeType.SwapRelicHigherTierRandom;
        }
    }

    /// <summary>상점 (서버 -> 플레이어 한 명). 진열은 사람마다 따로. 사고 나면 Bought 가 true</summary>
    [System.Serializable]
    public sealed class ShopState
    {
        public List<string> RelicOffers = new List<string>();
        public List<int> RelicPrices = new List<int>();
        public List<bool> RelicBought = new List<bool>();

        public List<string> SkillOffers = new List<string>();
        public List<int> SkillPrices = new List<int>();
        public List<bool> SkillBought = new List<bool>();

        // 픽업 스킬 강화 비용 = UpgradeCostPerLevel x (다음 단계)
        public int UpgradeCostPerLevel = 50;

        // 마지막 행동 결과 ("구매: 은빛 방패" / "던전 재화가 부족합니다")
        public string Message = "";
    }

    /// <summary>
    /// 방 클리어 보상 (서버 -> 플레이어 한 명)
    ///  - 몬스터방: 던전 재화 + 픽업 스킬 후보 중 하나
    ///  - 가디언방: 유물 하나 / 보스방: 유물 하나 + 픽업 스킬 하나
    /// </summary>
    [System.Serializable]
    public sealed class RoomRewardOffer
    {
        public RoomType RoomType = RoomType.MONSTER;
        // 이미 받은 던전 재화 (보여주기만)
        public int Currency = 0;
        public List<string> SkillOffers = new List<string>();
        public List<string> RelicOffers = new List<string>();
    }
}
