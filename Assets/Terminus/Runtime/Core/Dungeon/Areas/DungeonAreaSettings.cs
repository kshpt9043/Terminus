// 구역 설정값 (언리얼 ADungeonArea 의 UPROPERTY(EditAnywhere) 기본값 + UDungeonAreaSubsystem 상수 + 타이머 초)
// 유니티에선 Game 쪽 ScriptableObject 등이 이 값을 채워 넘김
namespace Terminus.Dungeon
{
    [System.Serializable]
    public sealed class DungeonAreaSettings
    {
        // ---- 휴식터

        // 휴식터 회복량 (최대 체력 비율). 기획: 20%
        public float RestHealRatio = 0.2f;

        // 탐색 던전 재화 (최소, 최대). 기획: '소량' -> 수치 미정이라 임시 (몬스터방 10 의 절반쯤)
        public int RestExploreCurrencyMin = 5;
        public int RestExploreCurrencyMax = 10;

        // 탐색 유물 확률. 기획: 5%. 유물은 가디언 보상과 같은 조건 (공용 / 내 직업, 지금 계층, 이번 런에 안 나온 것)
        public float RestExploreRelicChance = 0.05f;

        // ---- 상점

        // 상점 진열 수 (방 설명: 유물 3, 스킬 3)
        public int ShopRelicCount = 3;
        public int ShopSkillCount = 3;

        // 픽업 스킬 구매가. 10-24 엑셀부터 스킬 BuyPrice_Dungeon 을 쓰고, 가격이 0 인 스킬만 이 임시 고정가
        public int ShopSkillPrice = 30;

        // 픽업 스킬 강화 비용 = 이 값 x 다음 단계 (11-04 피드백: 50 / 100 / 150 / 200 / 250)
        public int ShopUpgradeCostPerLevel = 50;

        // ---- 이벤트

        // 데이터에 맞는 이벤트가 하나도 없을 때 화면에 띄울 말
        public string EventFallbackText = "조용한 방입니다. 아무 일도 일어나지 않았습니다.";

        // 디버그: 다음 이벤트 방이 고를 이벤트 (PC DebugForceEvent). "" 이면 무작위. 서버만
        // (언리얼은 static FName ADungeonArea::DebugForcedEvent)
        public string DebugForcedEvent = "";

        // ---- 구출 / 난입 (언리얼 UDungeonAreaSubsystem 상수)

        // 구출 비용 (구출하는 사람의 현재 체력 비율). 사용자 결정 10-08: 40%
        public float RescueHealthCost = 0.4f;

        // 구출 / 난입 투표 시간 (초). 안 고른 사람은 기권, 아무도 안 고르면 구출
        public float RescueVoteSeconds = 20f;

        // ---- 결과를 잠깐 보여주고 방을 끝내는 시간 (초. 언리얼 SetTimer 값)
        public float RestFinishDelay = 1.5f;
        public float EventFinishDelay = 1.5f;
        // 쉬는 가디언 전투로 넘어갈 때는 조금 더 길게
        public float EventGuardianFinishDelay = 2.5f;
        public float ShopFinishDelay = 0.5f;

        public DungeonAreaSettings Clone() => (DungeonAreaSettings)MemberwiseClone();
    }
}
