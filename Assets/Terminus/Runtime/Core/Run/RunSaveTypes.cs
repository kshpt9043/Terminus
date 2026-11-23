// 런 세이브 파일 형식 (언리얼 Game/TerminusSaveSubsystem.h 의 FRunSavePlayer / FRunSaveSummary / UTerminusRunSave / UTerminusRunSaveIndex 이식)
// 세이브에 들어가는 클래스: [Serializable] + public 필드 + List 만 (유니티 JsonUtility 가 읽을 수 있게)
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Run
{
    // 세이브 속 플레이어 한 명
    [System.Serializable]
    public sealed class RunSavePlayer
    {
        // 온라인 아이디 (스팀 ID). 멀티 이어하기 때 같은 사람인지 확인
        public string PlayerId = "";
        public string PlayerName = "";

        // 저장 당시 RunState (직업 / 스텟 / 유물 / 강화 스킬 / 재화 / 지도 위치). SavedHealth = 그때 체력
        public RunState RunState = new RunState();

        /// <summary>
        /// 지금 접속해 있는 사람을 세이브용으로. 고른 방은 비우고, 체력은 0 이면(임시 부활 전 등) 1 로.
        /// currentHealth null = 배틀러가 없음 -> SavedHealth -1 (= 최대 체력)
        /// </summary>
        public static RunSavePlayer FromLive(string playerId, string playerName, RunState state, int? currentHealth)
        {
            var entry = new RunSavePlayer
            {
                PlayerId = playerId ?? "",
                PlayerName = playerName ?? "",
                RunState = state != null ? state.Clone() : new RunState(),
            };
            entry.RunState.SelectedRoomId = -1;
            entry.RunState.SavedHealth = currentHealth.HasValue ? Math.Max(1, currentHealth.Value) : -1;
            return entry;
        }

        // 진행 중에 나갔다가 아직 안 돌아온 사람 (던전 게임모드가 들고 있던 RunState 그대로)
        public static RunSavePlayer FromDeparted(string playerId, string playerName, RunState state)
        {
            return new RunSavePlayer
            {
                PlayerId = playerId ?? "",
                PlayerName = playerName ?? "",
                RunState = state != null ? state.Clone() : new RunState(),
            };
        }

        public RunSavePlayer Clone()
        {
            var c = (RunSavePlayer)MemberwiseClone();
            c.RunState = RunState.Clone();
            return c;
        }
    }

    // 세이브 목록에 보여줄 요약. 목록을 띄울 때 세이브 파일을 다 열지 않게 인덱스 파일에 따로 모아 둠
    [System.Serializable]
    public sealed class RunSaveSummary
    {
        public string SlotName = "";

        // 방(던전) 이름. 처음 주점을 열 때(싱글은 시작할 때) 정한 것
        public string RoomName = "";

        // 저장 시각. DateTime.Ticks (언리얼 FDateTime 틱과 같은 단위: 0001-01-01 부터 100ns). JsonUtility 가 DateTime 을 못 써서 long
        public long SavedAt = 0;

        public bool bMultiplayer = false;
        public bool bHardMode = false;
        public int Floor = 1;

        // 지나온 방 수 (가장 앞선 사람 기준)
        public int RoomsCleared = 0;

        public List<string> PlayerNames = new List<string>();
        public List<CharacterClass> PlayerClasses = new List<CharacterClass>();

        public DateTime SavedAtTime => new DateTime(SavedAt);

        public RunSaveSummary Clone()
        {
            var c = (RunSaveSummary)MemberwiseClone();
            c.PlayerNames = new List<string>(PlayerNames);
            c.PlayerClasses = new List<CharacterClass>(PlayerClasses);
            return c;
        }
    }

    // 런 하나의 세이브 파일. 런마다 슬롯 하나 ("Run_20261005_143210"), 방을 끝낼 때마다 덮어씀
    [System.Serializable]
    public sealed class RunSave
    {
        public const int CurrentVersion = 1;
        public int Version = CurrentVersion;

        public RunSaveSummary Summary = new RunSaveSummary();

        // 이번 층 지도
        public List<RoomNode> Rooms = new List<RoomNode>();

        public int Floor = 1;

        // 이 층의 테마 (계층마다 랜덤으로 고른 것). 언리얼 FSoftObjectPath -> 테마 아이디 / 에셋 이름 (게임 쪽이 정함)
        public string Theme = "";

        // 이번 런에 나온 가디언 (중복 출현 금지용)
        public List<string> SeenGuardians = new List<string>();

        // 이번 런에 나온 이벤트 (한 런 한 번 이벤트용)
        public List<string> SeenEvents = new List<string>();

        // 런 시드 (이벤트 방 고정용). 0 = 예전 세이브
        public int RunSeed = 0;

        public List<RunSavePlayer> Players = new List<RunSavePlayer>();
    }

    // 세이브 목록 (요약만). 슬롯 "RunSaveIndex"
    [System.Serializable]
    public sealed class RunSaveIndex
    {
        public List<RunSaveSummary> Entries = new List<RunSaveSummary>();
    }
}
