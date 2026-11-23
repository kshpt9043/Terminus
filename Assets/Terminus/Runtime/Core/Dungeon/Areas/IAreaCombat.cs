// 구역이 쓰는 전투 진행 (언리얼 UDungeonCombatComponent 중 ADungeonArea / UDungeonAreaSubsystem 이 부르는 것만)
// 구현은 Dungeon/Combat 의 DungeonCombat. 구역 코드는 그 클래스를 모르고 이 인터페이스만 봄 (테스트는 가짜로)
// 언리얼: 전투 컴포넌트가 GetOwner() 로 구역을 찾아 MarkCleared / MarkWiped 를 부르고, 맵 매니저에 배신 결과를 알렸음
//  -> 여기선 전투가 이벤트를 올리고 구역이 받는다
using System;
using System.Collections.Generic;
using Terminus.Map;

namespace Terminus.Dungeon
{
    public interface IAreaCombat
    {
        // 다음 StartCombat 한 번만 몬스터 체력에 곱함 (이벤트 '쉬는 가디언' 70%). StartCombat 이 1 로 되돌림
        void SetNextSpawnHealthScale(float scale);

        // 다음 StartCombat 한 번만 이 몬스터들로 (퀘스트 특수 방). 비어 있으면 평소대로 고름
        void SetNextForcedMonsters(IReadOnlyList<string> rows);

        // 몬스터를 스폰하고 첫 사이클 시작. 전투 방(몬스터 / 가디언 / 보스)일 때 구역이 부름
        void StartCombat(RoomNode room, IReadOnlyList<DungeonPlayer> players, DungeonTheme theme);

        // 전투 정리 (몬스터 제거, 타이머 정지). 구역을 비울 때
        void EndCombat();

        // [배신 전투] 팀은 플레이어 자리, 배신자는 몬스터 자리. players = 팀 먼저, 배신자 마지막
        void StartBetrayal(IReadOnlyList<DungeonPlayer> players, DungeonPlayer betrayer, float victimHealthCut, int statPerOpponent);

        // 배신 전투 중 누가 나감: 배신자면 팀 승리, 팀원이면 그 사람은 쓰러진 걸로
        void HandleBetrayalLeaver(DungeonPlayer leaver);

        // [난입] 전멸한 이 구역의 전투를 새로 들어온 사람들과 이어서 함 (몬스터 체력은 그대로). 패배 상태에서만
        void ResumeWithPlayers(IReadOnlyList<DungeonPlayer> joiners);

        // 유물 후보: 공용이거나 내 직업, 지금 계층 등급, 이번 런에 안 나온 것 중 count 개 (휴식터 탐색도 씀)
        List<string> PickRewardRelics(DungeonPlayer player, int count);

        // 배신 전투인가 (언리얼 IsBetrayal)
        bool IsBetrayal { get; }

        // Phase == Victory (승리 후 결과 / 보상 화면). 언리얼 ADungeonArea::IsFightOver 가 봄
        bool IsVictory { get; }

        // Phase == PlayerTurn || MonsterTurn (살아서 턴이 도는 전투). 관전 바가 봄
        bool IsTurnInProgress { get; }

        // 승리 + 보상까지 끝남 -> 구역 클리어 (언리얼 UDungeonCombatComponent::ClearArea -> ADungeonArea::MarkCleared)
        event Action AreaCleared;

        // 이 구역 전멸 (언리얼 ReportWipe -> ADungeonArea::MarkWiped)
        event Action AreaWiped;

        // 배신 전투 끝 (언리얼 ReportBetrayalResult -> AMapManager::HandleBetrayalResult). (배신자, 배신자가 이겼나)
        event Action<DungeonPlayer, bool> BetrayalFinished;
    }
}
