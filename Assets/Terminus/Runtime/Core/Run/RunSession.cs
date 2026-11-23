// 한 판(런) 동안 레벨 이동을 넘어 살아야 하는 서버 쪽 데이터 (언리얼 Game/TerminusRunSubsystem 이식)
//
// 언리얼: MapManager 는 레벨에 놓인 액터라 ServerTravel 하면 같이 죽고, 새 레벨 BeginPlay 에서 지도를 또 뽑아버림
// -> 전투 갔다 오면 지도가 바뀌는 문제. GameInstance 는 트래블해도 안 죽어서 지도 원본을 여기 둠.
// 유니티에서도 씬을 넘어 살아야 하는 건 같음 -> 게임 쪽이 DontDestroyOnLoad 객체에 하나 들고 있을 것
//
// 서버에서만 의미 있음. 클라는 지도 복제로 받음
// 플레이어별 진행도(현재 방, 층)는 RunPlayer 의 RunState 가 들고 감
using System;
using System.Collections.Generic;
using Terminus.Map;

namespace Terminus.Run
{
    public sealed class RunSession
    {
        // 방 이름 최대 글자 수 (스팀 로비 광고 값 길이 때문)
        public const int MaxRoomNameLength = 24;

        readonly Random _rng;
        readonly Func<DateTime> _now;

        List<RoomNode> _rooms = new List<RoomNode>();
        readonly List<string> _seenGuardians = new List<string>();
        readonly List<string> _seenEvents = new List<string>();

        /// <param name="now">지금 시각 (null 이면 DateTime.Now). 세이브 슬롯 이름에 씀</param>
        public RunSession(Random rng, Func<DateTime> now = null)
        {
            _rng = rng ?? new Random();
            _now = now ?? (() => DateTime.Now);
        }

        // 0 이면 주점을 안 거친 것 (던전 맵 바로 시작). 이때는 지도 쪽 기본값을 씀
        public int PartySize { get; private set; }

        // 이 런의 층 (세이브에서 불러온 값 / 층을 넘어갈 때). 0 이면 지도 쪽 기본값
        public int Floor { get; set; }

        // 이 런의 지금 테마. 비어 있으면 지도 쪽 기본값 (언리얼 FSoftObjectPath -> 테마 아이디)
        public string ThemePath { get; set; } = "";

        // 이 런의 세이브 슬롯. 방을 끝낼 때마다 여기에 덮어씀
        public string SaveSlot { get; set; } = "";

        // 런 시드. 새 런마다 하나, 세이브에 같이 남음 -> 이벤트 방은 (시드, 층, 방 번호)로 굴려서 이어하기를 해도 같은 이벤트 (11-04 피드백)
        public int RunSeed { get; private set; }

        // 방(던전) 이름. 처음 주점을 열 때(싱글은 시작할 때) 정한 이름이 런 끝까지 유지됨
        // 주점 목록 / 재합류 대기 / 세이브 목록 / 이어하기로 연 주점에 모두 이 이름
        public string RoomName { get; private set; } = "";

        public void SetRoomName(string name)
        {
            var trimmed = (name ?? "").Trim();
            RoomName = trimmed.Length > MaxRoomNameLength ? trimmed.Substring(0, MaxRoomNameLength) : trimmed;
        }

        // 지도 원본
        public bool HasMap => _rooms.Count > 0;
        public IReadOnlyList<RoomNode> Rooms => _rooms;
        public void SetRooms(IEnumerable<RoomNode> rooms)
        {
            _rooms = new List<RoomNode>();
            if (rooms == null) return;
            foreach (var r in rooms) _rooms.Add(r.Clone());
        }

        // 이번 런에 나온 가디언 (DT_Monster 행). 테마의 가디언이 전부 한 번씩 나오기 전엔 다시 안 나옴 (기획 10-07)
        public IReadOnlyList<string> SeenGuardians => _seenGuardians;
        public void AddSeenGuardian(string row)
        {
            if (!_seenGuardians.Contains(row)) _seenGuardians.Add(row);
        }
        public void ForgetSeenGuardians(ICollection<string> rows) => _seenGuardians.RemoveAll(rows.Contains);

        // 이번 런에 나온 이벤트 (DT_Event 행). bOneTimePerRun 이벤트는 여기 있으면 다시 안 나옴
        public IReadOnlyList<string> SeenEvents => _seenEvents;
        public void AddSeenEvent(string row)
        {
            if (!_seenEvents.Contains(row)) _seenEvents.Add(row);
        }

        // 층 + 방마다 고정된 시드 (언리얼 HashCombine(HashCombine(RunSeed, Floor), RoomId). GetTypeHash(int) = 그 값)
        public int GetRoomSeed(int floor, int roomId)
        {
            return unchecked((int)HashCombine(HashCombine((uint)RunSeed, (uint)floor), (uint)roomId));
        }

        // 언리얼 Templates/TypeHash.h HashCombine (Bob Jenkins 섞기) 그대로
        public static uint HashCombine(uint a, uint c)
        {
            unchecked
            {
                uint b = 0x9e3779b9;
                a += b;

                a -= b; a -= c; a ^= (c >> 13);
                b -= c; b -= a; b ^= (a << 8);
                c -= a; c -= b; c ^= (b >> 13);
                a -= b; a -= c; a ^= (c >> 12);
                b -= c; b -= a; b ^= (a << 16);
                c -= a; c -= b; c ^= (b >> 5);
                a -= b; a -= c; a ^= (c >> 3);
                b -= c; b -= a; b ^= (a << 10);
                c -= a; c -= b; c ^= (b >> 15);

                return c;
            }
        }

        // 새 시드. 언리얼은 FMath::Rand() ^ Cycles (0 이 나올 수도 있었음). 0 = '예전 세이브' 표시라 여기선 0 을 안 냄
        int NewSeed() => _rng.Next(1, int.MaxValue);

        /// <summary>주점에서 출발할 때 호출. 이전 런의 지도를 버리고 인원을 확정. 세이브 슬롯도 새로</summary>
        public void BeginNewRun(int partySize)
        {
            PartySize = Math.Max(1, partySize);
            Floor = 0;
            ThemePath = "";
            _rooms.Clear();
            _seenGuardians.Clear();
            _seenEvents.Clear();
            RunSeed = NewSeed();
            SaveSlot = MakeNewSlotName(_now());

            // 이름은 주점을 열 때(싱글은 시작할 때) 이미 정해짐. 혹시 비었으면 랜덤
            if (string.IsNullOrEmpty(RoomName))
            {
                RoomName = MakeRandomRoomName(_rng);
            }

            RunLog.Log($"[Run] 새 런 시작 '{RoomName}'. 인원 {PartySize}, 세이브 {SaveSlot}");
        }

        /// <summary>이어하기로 출발할 때 호출. 세이브의 지도 / 층 / 인원 / 슬롯을 그대로</summary>
        public void BeginLoadedRun(RunSave save)
        {
            if (save == null) return;

            PartySize = Math.Max(1, save.Players != null ? save.Players.Count : 0);
            Floor = Math.Max(1, save.Floor);
            ThemePath = save.Theme ?? "";
            SetRooms(save.Rooms);
            _seenGuardians.Clear();
            if (save.SeenGuardians != null) _seenGuardians.AddRange(save.SeenGuardians);
            _seenEvents.Clear();
            if (save.SeenEvents != null) _seenEvents.AddRange(save.SeenEvents);
            // 시드가 없는 예전 세이브는 새로 (다음 저장부터 고정)
            RunSeed = save.RunSeed != 0 ? save.RunSeed : NewSeed();
            SaveSlot = save.Summary?.SlotName ?? "";
            var name = save.Summary?.RoomName;
            RoomName = string.IsNullOrEmpty(name) ? MakeRandomRoomName(_rng) : name;

            RunLog.Log($"[Run] 세이브 {SaveSlot} 이어하기. 인원 {PartySize}, {Floor}층, 방 {_rooms.Count}개");
        }

        // 새 런 슬롯 이름 ("Run_20261005_143210")
        public static string MakeNewSlotName(DateTime now) => "Run_" + now.ToString("yyyyMMdd_HHmmss", System.Globalization.CultureInfo.InvariantCulture);

        static readonly string[] Adjectives =
        {
            "녹슨", "푸른", "붉은", "잠든", "수상한", "떠도는", "용감한", "배고픈",
            "은빛", "그을린", "고요한", "시끄러운", "낡은", "길 잃은", "빛나는", "겁 없는",
        };
        static readonly string[] Nouns =
        {
            "방패", "등불", "고블린", "모험가", "슬라임", "여관", "검", "지도",
            "해골", "망토", "마도구", "주사위", "열쇠", "촛대", "용", "곰",
        };
        static readonly string[] Groups = { "원정대", "탐험대", "일행", "파티" };

        // 기본으로 넣어 줄 랜덤 이름 ("배고픈 슬라임 원정대")
        public static string MakeRandomRoomName(Random rng)
        {
            var name = $"{Adjectives[rng.Next(0, Adjectives.Length)]} {Nouns[rng.Next(0, Nouns.Length)]} {Groups[rng.Next(0, Groups.Length)]}";
            return name.Length > MaxRoomNameLength ? name.Substring(0, MaxRoomNameLength) : name;
        }
    }
}
