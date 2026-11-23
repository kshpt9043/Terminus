// 지도 / 층 / 투표 / 배신 설정값 (언리얼 AMapManager 의 EditAnywhere 값들). 기본값은 언리얼 그대로
using System.Collections.Generic;

namespace Terminus.Map
{
    [System.Serializable]
    public sealed class MapSettings
    {
        // 현재 게임 멀티플레이 참여 인원수. 주점 인원 / 접속 인원을 모를 때만 씀 (FloorProgression.ResolvePlayerCount)
        public int CurrentPlayerCount = 4;

        // ★ 전체 레벨(층수) 개수 (기본값: 12). 최소 2
        public int TotalLevels = 12;

        // ★ 한 레벨(줄) 당 생성 가능한 최대 방 개수 (기본값: 4). 최소 1
        public int MaxRoomsPerRow = 4;

        // 정원을 랜덤으로 뽑고 조건에 맞을 때까지 다시 뽑는 방식이라 인원이 늘수록 성공률이 급락한다.
        // 4인 기준 1회 성공률이 약 2% 라서 200회로는 1.8% 확률로 유효한 지도를 못 만든다(실측).
        // 1000회면 실패가 사실상 사라지고(4000판 중 0회, 최대 458회 시도) 비용도 무시할 수준
        // (언리얼에선 GenerateMap 안의 상수 MaxRetries)
        public int MaxRetries = 1000;

        // 일반 방 비율 설정 (기본값: 몬스터 55%, 휴식 10%, 상점 10%, 가디언 10%, 이벤트 15%)
        // 넣은 순서가 배분 순서 (언리얼 TMap 도 넣은 순서로 돎)
        public Dictionary<RoomType, float> RoomTypeWeights = DefaultRoomTypeWeights();

        // 계층별 테마 후보 (0 = 표층, 1 = 중층, 2 = 심층). 기획: 테마 하나가 2개 층, 중층은 리자드의 늪 / 꽃의 정원
        public List<TierThemes> TierThemes = new List<TierThemes>();

        // 지금 있는 마지막 층 (콘텐츠가 늘면 올릴 것). 이 층 보스 뒤엔 '다음 층' 이 없음
        public int LastFloor = 6;

        // 투표 시간 / 공개 타이머 / 결과를 보여주는 시간 (초)
        public double VoteSeconds = 15.0;
        public double RevealSeconds = 5.0;
        public double ResultSeconds = 3.0;
        // 혼자일 때 결과를 보여주는 시간 (언리얼 ResolveFloorVote 의 0.5f 상수)
        public double SingleResultSeconds = 0.5;

        // 배신 전투 보정 (사용자 결정 10-08: 배신당한 쪽 현재 체력 일정 % 감소 + 배신자 공격 / 방어 상승. 수치는 임시)
        // 0 ~ 0.95
        public float BetrayalVictimHealthCut = 0.3f;

        // 배신자 공격 / 방어 +(상대 수 x 이 값)
        public int BetrayerStatPerOpponent = 1;

        public static Dictionary<RoomType, float> DefaultRoomTypeWeights()
        {
            // 기획 비율 기본값
            return new Dictionary<RoomType, float>
            {
                { RoomType.MONSTER, 55.0f },
                { RoomType.BREAK, 10.0f },
                { RoomType.STORE, 10.0f },
                { RoomType.GUARDIAN, 10.0f },
                { RoomType.EVENT, 15.0f },
            };
        }

        public MapSettings Clone()
        {
            var c = (MapSettings)MemberwiseClone();
            c.RoomTypeWeights = new Dictionary<RoomType, float>(RoomTypeWeights);
            c.TierThemes = TierThemes.ConvertAll(t => t.Clone());
            return c;
        }
    }
}
