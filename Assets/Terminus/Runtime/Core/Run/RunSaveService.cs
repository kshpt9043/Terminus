// 런 세이브 (이어하기) (언리얼 Game/TerminusSaveSubsystem 이식)
//  - 자동 저장: 방(전투)이 끝나고 전원이 지도로 돌아왔을 때 서버가 저장 (언리얼 UDungeonAreaSubsystem::FinishAllRooms)
//    싱글 / 멀티 모두 서버(방장) 컴퓨터에만 저장됨
//  - 싱글: 세이브 하나 (SingleRunSlot). '던전 입장' 은 늘 새 게임 (있던 싱글 세이브는 확인 후 삭제)
//  - 멀티: 런마다 슬롯 하나
//  - 둘 다 메인 메뉴 '이어하기' 목록에서 골라 이어함
//  - 이어하기: 세이브를 읽어 두고 엶. 게임 쪽이 PendingLoad 로 꺼내 씀
//    싱글 = 세션 없이 주점 -> 바로 출발. 멀티 = 세이브 인원으로 주점을 열고, 세이브 당시 플레이어가 다 모여야 출발
//
// 영구 데이터(골드 / 창고 / 보유 스킬)는 여기가 아니라 ProfileService
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Run
{
    /// <summary>
    /// 지금 런을 저장할 때 게임 쪽이 모아 주는 것 (언리얼은 SaveCurrentRun 이 월드에서 직접 찾았음)
    /// </summary>
    public sealed class RunSaveRequest
    {
        // 멀티인가 (언리얼 NetMode != Standalone)
        public bool bMultiplayer = false;

        // 레벨의 지도 (언리얼 MapManager). null 이면 RunSession 의 사본을 씀
        public List<RoomNode> MapRooms = null;
        public int MapFloor = 0;
        // 지도의 테마. 비어 있으면 RunSession 의 테마
        public string MapTheme = "";

        // 지금 접속해 있는 플레이어. 순서가 매번 같게 넣을 것 (언리얼: PlayerId 순). RunSavePlayer.FromLive 로 만듦
        public List<RunSavePlayer> Players = new List<RunSavePlayer>();

        // 진행 중에 나갔다가 아직 안 돌아온 사람 (이어하기 때 그 사람도 와야 출발). RunSavePlayer.FromDeparted 로 만듦
        public List<RunSavePlayer> DepartedPlayers = new List<RunSavePlayer>();
    }

    public sealed class RunSaveService
    {
        public const string RunSaveIndexSlot = "RunSaveIndex";
        public const string SingleRunSlot = "SingleRun";

        readonly ISaveStorage _storage;
        readonly ISaveSerializer _serializer;
        readonly Func<DateTime> _now;

        /// <param name="now">지금 시각 (null 이면 DateTime.Now). 세이브 시각 / 새 슬롯 이름</param>
        public RunSaveService(ISaveStorage storage, ISaveSerializer serializer, Func<DateTime> now = null)
        {
            _storage = storage ?? throw new ArgumentNullException(nameof(storage));
            _serializer = serializer ?? throw new ArgumentNullException(nameof(serializer));
            _now = now ?? (() => DateTime.Now);
        }

        // 세이브가 생기거나 지워졌을 때 (메인 메뉴 이어하기 버튼 표시용)
        public event Action RunSavesChanged;

        // 이어하기로 연 주점이면 그 세이브. 주점 / 던전 게임모드가 꺼내 씀
        public RunSave PendingLoad { get; private set; }

        // 새 판을 시작하거나 메뉴로 돌아오면 비움 (안 비우면 다음 주점이 이어하기로 열림)
        public void ClearPendingLoad() => PendingLoad = null;

        public string MakeNewSlotName() => RunSession.MakeNewSlotName(_now());

        // ============================================================ 목록 (인덱스)

        RunSaveIndex LoadIndex()
        {
            if (_storage.Exists(RunSaveIndexSlot))
            {
                var index = SaveCrypto.LoadFromSlot<RunSaveIndex>(_storage, _serializer, RunSaveIndexSlot, out var result);
                if (index != null)
                {
                    if (index.Entries == null) index.Entries = new List<RunSaveSummary>();
                    // 예전 평문이면 바로 암호화해서 다시 씀
                    if (result == SaveLoadResult.Legacy)
                    {
                        SaveCrypto.SaveToSlot(_storage, _serializer, index, RunSaveIndexSlot);
                    }
                    return index;
                }
            }
            return new RunSaveIndex();
        }

        void SaveIndex(RunSaveIndex index)
        {
            if (index != null) SaveCrypto.SaveToSlot(_storage, _serializer, index, RunSaveIndexSlot);
        }

        // 세이브 목록 (최근 저장 먼저). 파일이 없어진 항목은 뺌. multiplayerOnly = 멀티 세이브만
        public List<RunSaveSummary> GetRunSaves(bool multiplayerOnly = false)
        {
            var index = LoadIndex();

            // 파일을 직접 지운 경우 등 -> 목록에서도 뺌
            int removed = index.Entries.RemoveAll(e => !_storage.Exists(e.SlotName));
            if (removed > 0)
            {
                SaveIndex(index);
            }

            IEnumerable<RunSaveSummary> result = index.Entries;
            if (multiplayerOnly)
            {
                result = result.Where(e => e.bMultiplayer);
            }
            return result.OrderByDescending(e => e.SavedAt).Select(e => e.Clone()).ToList();
        }

        // 이어하기 목록에 띄울 세이브가 있는가 (싱글 + 멀티)
        public bool HasRunSaves() => GetRunSaves().Count > 0;

        // 싱글은 세이브가 하나뿐 (슬롯 SingleRunSlot). 진행 중인 싱글 런이 있으면 그 요약, 없으면 null
        public RunSaveSummary FindSingleRunSave()
        {
            // 가장 최근 싱글 세이브 (예전 방식으로 여러 개 남아 있어도 최신 하나)
            foreach (var entry in GetRunSaves())
            {
                if (!entry.bMultiplayer) return entry;
            }
            return null;
        }

        // 싱글 세이브 지우기 (던전 입장으로 새 싱글 게임을 시작할 때)
        public void DeleteSingleRunSaves()
        {
            RunSaveSummary summary;
            while ((summary = FindSingleRunSave()) != null)
            {
                // 파일은 이미 없고 목록만 남은 경우에도 목록에서 빠지니 무한 반복 안 함
                DeleteRunSave(summary.SlotName);
            }
        }

        // ============================================================ 저장 / 읽기 / 삭제

        /// <summary>지금 런 저장 (서버만). 실패하면 false</summary>
        public bool SaveCurrentRun(RunSession run, RunSaveRequest request)
        {
            if (run == null || request == null) return false;

            var save = new RunSave();

            // 지도 / 층. 레벨의 지도가 진짜 (런 세션은 그 원본 사본)
            bool hasMap = request.MapRooms != null;
            save.Rooms = (hasMap ? (IEnumerable<RoomNode>)request.MapRooms : run.Rooms).Select(r => r.Clone()).ToList();
            save.Floor = hasMap ? request.MapFloor : run.Floor;
            save.Theme = hasMap && !string.IsNullOrEmpty(request.MapTheme) ? request.MapTheme : run.ThemePath;
            save.SeenGuardians = new List<string>(run.SeenGuardians);
            save.SeenEvents = new List<string>(run.SeenEvents);
            save.RunSeed = run.RunSeed;

            foreach (var p in request.Players ?? new List<RunSavePlayer>())
            {
                var entry = p.Clone();
                save.Players.Add(entry);
                save.Summary.PlayerNames.Add(entry.PlayerName);
                save.Summary.PlayerClasses.Add(entry.RunState.CharacterClass);
                save.Summary.RoomsCleared = Math.Max(save.Summary.RoomsCleared, entry.RunState.VisitedRoomIds.Count);
                save.Summary.bHardMode |= entry.RunState.bHardMode;
            }

            // 진행 중에 나갔다가 아직 안 돌아온 사람도 세이브에 (이어하기 때 그 사람도 와야 출발)
            // (언리얼도 나간 사람의 하드 모드는 요약에 안 넣음. 그대로)
            foreach (var p in request.DepartedPlayers ?? new List<RunSavePlayer>())
            {
                var entry = p.Clone();
                save.Players.Add(entry);
                save.Summary.PlayerNames.Add(entry.PlayerName);
                save.Summary.PlayerClasses.Add(entry.RunState.CharacterClass);
                save.Summary.RoomsCleared = Math.Max(save.Summary.RoomsCleared, entry.RunState.VisitedRoomIds.Count);
            }

            if (save.Players.Count == 0) return false;

            // 슬롯: 싱글은 언제나 하나 (SingleRunSlot). 멀티는 이 런의 슬롯을 계속 덮어씀 (주점을 안 거친 경우처럼 없으면 새로)
            bool multiplayer = request.bMultiplayer;
            string slot = multiplayer ? run.SaveSlot : SingleRunSlot;
            if (string.IsNullOrEmpty(slot))
            {
                slot = MakeNewSlotName();
            }
            run.SaveSlot = slot;

            save.Summary.SlotName = slot;
            save.Summary.RoomName = run.RoomName;
            save.Summary.SavedAt = _now().Ticks;
            save.Summary.bMultiplayer = multiplayer;
            save.Summary.Floor = save.Floor;

            if (!SaveCrypto.SaveToSlot(_storage, _serializer, save, slot))
            {
                RunLog.Warn($"[Save] {slot} 저장 실패");
                return false;
            }

            // 목록 갱신 (같은 슬롯이면 교체). 싱글이면 다른 싱글 세이브(예전 방식으로 남은 것)는 지움 -> 싱글은 하나만
            var index = LoadIndex();
            index.Entries.RemoveAll(e =>
            {
                if (e.SlotName == slot) return true;
                if (!multiplayer && !e.bMultiplayer)
                {
                    _storage.Delete(e.SlotName);
                    return true;
                }
                return false;
            });
            index.Entries.Add(save.Summary.Clone());
            SaveIndex(index);
            RunSavesChanged?.Invoke();

            RunLog.Log($"[Save] {slot} 저장 (플레이어 {save.Players.Count}명, 방 {save.Summary.RoomsCleared}개 지남)");
            return true;
        }

        // 조작 / 손상이면 null (이어하기 실패로 안내). 예전 평문이면 바로 암호화해서 다시 씀
        public RunSave LoadRunSave(string slotName)
        {
            if (string.IsNullOrEmpty(slotName) || !_storage.Exists(slotName)) return null;

            var save = SaveCrypto.LoadFromSlot<RunSave>(_storage, _serializer, slotName, out var result);
            if (save != null && result == SaveLoadResult.Legacy)
            {
                SaveCrypto.SaveToSlot(_storage, _serializer, save, slotName);
            }
            if (save != null) MigrateRunSave(save);
            return save;
        }

        // 버전별 이전 + 직렬화기가 null 로 둔 칸 채우기. 지금은 버전 1 하나뿐
        static void MigrateRunSave(RunSave save)
        {
            if (save.Summary == null) save.Summary = new RunSaveSummary();
            if (save.Rooms == null) save.Rooms = new List<RoomNode>();
            if (save.SeenGuardians == null) save.SeenGuardians = new List<string>();
            if (save.SeenEvents == null) save.SeenEvents = new List<string>();
            if (save.Players == null) save.Players = new List<RunSavePlayer>();
            foreach (var p in save.Players)
            {
                if (p.RunState == null) p.RunState = new RunState();
            }
            if (save.Version < RunSave.CurrentVersion) save.Version = RunSave.CurrentVersion;
        }

        public bool DeleteRunSave(string slotName)
        {
            if (string.IsNullOrEmpty(slotName)) return false;

            bool deleted = _storage.Delete(slotName);

            var index = LoadIndex();
            if (index.Entries.RemoveAll(e => e.SlotName == slotName) > 0)
            {
                SaveIndex(index);
            }

            if (PendingLoad != null && PendingLoad.Summary.SlotName == slotName)
            {
                PendingLoad = null;
            }

            RunLog.Log($"[Save] {slotName} 삭제");
            RunSavesChanged?.Invoke();
            return deleted;
        }

        // ============================================================ 이어하기

        /// <summary>
        /// 이어하기 준비 (언리얼 ContinueRun 의 규칙 부분). 세이브를 읽어 PendingLoad 에 두고, 런 이름을 세이브 당시 그대로.
        /// 못 열면 false + 사유. 성공하면 게임 쪽이 로딩 화면을 띄우고
        ///  - 싱글 (PendingLoad.Summary.bMultiplayer == false): 세션 없이 주점 씬 -> 주점이 바로 출발시킴
        ///  - 멀티: 세이브 인원(Players.Count)만 들어올 수 있는 세션을 열고 던전 씬으로 -> 다 모일 때까지 대기.
        ///    세션을 못 열면 ClearPendingLoad 하고 "주점을 열 수 없습니다." 안내
        /// </summary>
        public bool TryBeginContinue(string slotName, RunSession run, out string error)
        {
            error = "";
            var save = LoadRunSave(slotName);
            if (save == null || save.Players.Count == 0 || save.Rooms.Count == 0)
            {
                error = "세이브 파일을 읽을 수 없습니다. 손상되었거나 변조된 파일입니다.";
                return false;
            }

            PendingLoad = save;

            // 이름은 세이브 당시 그대로
            run?.SetRoomName(save.Summary.RoomName);
            return true;
        }

        // ============================================================ 표시

        // 목록에 띄울 글 ("멀티 3인 · 1층 표층 · 방 4개 지남")
        public static string DescribeTitle(RunSaveSummary summary)
        {
            var mode = summary.bMultiplayer ? $"멀티 {summary.PlayerNames.Count}인" : "싱글";
            var title = $"{mode} · {summary.Floor}층 {FloorRules.GetTierName(summary.Floor)} · 방 {summary.RoomsCleared}개 지남";
            if (summary.bHardMode)
            {
                title += " · 하드";
            }
            return title;
        }

        // "무도가 Joe, 성기사 Kim"
        public static string DescribePlayers(RunSaveSummary summary)
        {
            var parts = new List<string>();
            for (int i = 0; i < summary.PlayerNames.Count; ++i)
            {
                string className = "";
                if (i < summary.PlayerClasses.Count)
                {
                    var row = GameData.FindCharacterClassRow(summary.PlayerClasses[i]);
                    className = row != null ? row.DisplayName : summary.PlayerClasses[i].ToString();
                }
                parts.Add($"{className} {summary.PlayerNames[i]}");
            }
            return string.Join(", ", parts);
        }
    }
}
