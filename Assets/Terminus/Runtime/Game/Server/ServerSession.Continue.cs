// [서버] 이어하기 (언리얼 UTerminusSaveSubsystem::ContinueRun + ATavernGameMode InitGame / PostLogin / TryStartGame 의 LoadedSave 부분
//        + ADungeonGameMode::InitGame 의 멀티 세이브 부분)
//  - 메인 메뉴 이어하기 목록이 StartContinue(세이브) 를 부름 -> 여기서 방을 열고 세이브를 들고 있다가 ServerSession 이 생기면 꺼내 씀
//  - 싱글: 혼자 주점을 열고, 세이브 당시 직업 / 하드 모드로 고정한 뒤 바로 출발 (세이브의 지도 / 층 / RunState 그대로)
//  - 멀티: 세이브 이름으로 방을 열고 주점을 거치지 않고 던전으로. 세이브 플레이어 전원을 '아직 안 온 사람'(이공간)으로 두고
//          방장은 자기 자리를 바로 차지. 나머지는 방 목록의 '재합류 대기' 방으로 들어오는 대로 세이브 당시 상태로 (DungeonServer.Continue)
// NetworkRoot 가 언제나 주점 씬을 먼저 열므로 멀티도 주점을 잠깐 거친다 (로딩 화면이 덮고 있음)
using System.Collections;
using Terminus.Data;
using Terminus.Dungeon;
using Terminus.Game.Net;
using Terminus.Run;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Terminus.Game
{
    public sealed partial class ServerSession
    {
        // 방을 열기 전에 메뉴가 넘긴 세이브. ServerSession 이 생기면(Start) 꺼내 감
        static RunSave _pendingContinue;

        /// <summary>이어하기로 연 판의 세이브 (주점 단계 동안). 싱글은 출발하면 비움, 멀티는 던전이 이공간을 세우며 꺼내 감 (TakeContinueSave)</summary>
        public RunSave ContinueSave { get; private set; }

        /// <summary>이어하기로 연 판인가 (주점 화면이 로딩 화면을 걷지 않게. 언리얼 ATavernGameMode::IsContinuingRun)</summary>
        public bool IsContinuingRun => _pendingContinue != null || ContinueSave != null || _continueStarting;

        // 이어하기로 연 판이면 계속 true (주점을 떠날 때까지 로딩 화면을 덮어 두게). 판(ServerSession)이 끝나면 같이 사라짐
        bool _continueStarting;

        /// <summary>메뉴에 돌아가거나 방을 못 열었을 때 (안 비우면 다음 방이 이어하기로 열림. 언리얼 ClearPendingLoad)</summary>
        public static void ClearPendingContinue() => _pendingContinue = null;

        /// <summary>
        /// 이어하기 시작 (언리얼 ContinueRun 의 방 열기 부분). save 는 RunSaveService.TryBeginContinue 로 읽은 것 (PendingLoad).
        /// 싱글 = 혼자 주점으로 -> 바로 출발. 멀티 = 세이브 이름으로 방을 열고 던전으로 -> 다 모일 때까지 이공간.
        /// 못 열면 false (실패 이유는 NetworkRoot.ConnectionFailed 로도 감)
        /// </summary>
        public static bool StartContinue(RunSave save, out string error)
        {
            error = "";
            if (save == null || save.Players == null || save.Players.Count == 0 || save.Rooms == null || save.Rooms.Count == 0)
            {
                error = "세이브 파일을 읽을 수 없습니다. 손상되었거나 변조된 파일입니다.";
                return false;
            }
            if (NetworkRoot.IsBusy)
            {
                error = "이미 방에 들어가 있습니다.";
                return false;
            }

            _pendingContinue = save;
            bool ok = save.Summary != null && save.Summary.bMultiplayer
                ? NetworkRoot.StartHost(save.Summary.RoomName, "", true)
                : NetworkRoot.StartSingle();
            if (!ok)
            {
                _pendingContinue = null;
                error = "주점을 열 수 없습니다.";
                return false;
            }
            return true;
        }

        // Awake(ServerSession.cs) 다음. 방장 자신은 Awake 에서 이미 사람으로 들어와 있음
        void Start()
        {
            var save = _pendingContinue;
            _pendingContinue = null;
            if (save == null) return;

            ContinueSave = save;
            bHardMode = save.Summary != null && save.Summary.bHardMode;
            bool multiplayer = save.Summary != null && save.Summary.bMultiplayer && !NetworkRoot.IsSinglePlayer;
            Debug.Log($"[Continue] {(multiplayer ? "멀티" : "싱글")} 세이브 {save.Summary?.SlotName} 이어하기 (플레이어 {save.Players.Count}명)");

            if (multiplayer) BeginMultiplayerContinue(save);
            else BeginSingleContinue(save);
        }

        // ------------------------------------------------------------ 싱글 (언리얼 ATavernGameMode::PostLogin + TryStartGame)

        void BeginSingleContinue(RunSave save)
        {
            // 세이브 당시 직업 / 하드 모드로 고정 (주점 화면도 그 직업으로)
            var saved = save.Players[0].RunState;
            foreach (var m in Members)
            {
                m.Run.LockClassFromSave(saved.CharacterClass);
                m.Run.SetHardMode(bHardMode);
            }
            _continueStarting = true;
            StartCoroutine(AutoStartSingle(save));
        }

        // 로그인 도중에 씬을 옮기지 않게 주점이 다 뜬 뒤 살짝 뒤에 (언리얼 AutoStartTimer 0.2초)
        IEnumerator AutoStartSingle(RunSave save)
        {
            yield return WaitForTavern();
            yield return new WaitForSecondsRealtime(0.2f);
            if (NetworkRoot.bRunStarted || NetworkManager.Singleton == null || !NetworkManager.Singleton.IsServer) yield break;

            // 세이브의 지도 / 층 / 슬롯 그대로. 세이브 당시 RunState 로 (시작 고르기는 이미 끝난 상태)
            Run.BeginLoadedRun(save);
            var saved = save.Players[0].RunState.Clone();
            saved.SelectedRoomId = -1;
            foreach (var m in ConnectedMembers)
            {
                m.Run.SetRunState(saved);
                m.Run.bReady = true;
                break;   // 싱글 = 방장 한 명
            }
            ContinueSave = null;

            NetworkRoot.bRunStarted = true;
            NetworkRoot.UpdateHostedRoom();
            SyncParty(true);
            StartCoroutine(LoadDungeon());
        }

        // ------------------------------------------------------------ 멀티 (언리얼 ADungeonGameMode::InitGame + 방장 PostLogin)

        void BeginMultiplayerContinue(RunSave save)
        {
            // 지도 / 층 / 슬롯 / 이름 -> 던전이 이 지도를 씀 (DungeonServer.StartFirstFloor 의 HasMap)
            Run.BeginLoadedRun(save);

            // 방장은 세이브를 만든 사람 -> 아이디가 달라도 자리를 줌 (던전이 같은 규칙으로 이공간 목록에서 뺌)
            var host = Find(NetworkManager.ServerClientId);
            if (host != null)
            {
                var restored = ClaimHostFromSave(save, host);
                if (restored != null)
                {
                    host.Run.LockClassFromSave(restored.CharacterClass);
                    host.Run.SetRunState(restored);
                }
                host.Run.bReady = true;
            }

            // 주점을 건너뜀: 세이브 플레이어만 들어올 수 있게 (던전의 이공간이 확인)
            NetworkRoot.bRunStarted = true;
            NetworkRoot.UpdateHostedRoom();
            _continueStarting = true;
            StartCoroutine(TravelToDungeon());
        }

        IEnumerator TravelToDungeon()
        {
            yield return WaitForTavern();
            yield return new WaitForSecondsRealtime(0.2f);
            if (NetworkManager.Singleton == null || !NetworkManager.Singleton.IsServer) yield break;
            SyncParty(true);
            yield return LoadDungeon();
        }

        // 주점 씬 이벤트가 덜 끝났으면 LoadScene 이 거절됨 (SceneEventInProgress) -> 잠깐 뒤에 다시
        static IEnumerator LoadDungeon()
        {
            for (int i = 0; i < 50; ++i)
            {
                var nm = NetworkManager.Singleton;
                if (nm == null || !nm.IsServer || nm.SceneManager == null) yield break;
                if (SceneManager.GetActiveScene().name == SceneNames.Dungeon) yield break;
                var status = nm.SceneManager.LoadScene(SceneNames.Dungeon, LoadSceneMode.Single);
                if (status == SceneEventProgressStatus.Started) yield break;
                yield return new WaitForSecondsRealtime(0.1f);
            }
            Debug.LogWarning("[Continue] 던전 씬으로 못 넘어감");
        }

        /// <summary>
        /// 세이브의 방장 자리 (언리얼 FindDeparted(.., bLocal=true)): 온라인 아이디 -> 이름 -> 첫 자리.
        /// DungeonServer.Continue 가 진짜 이공간에서 같은 규칙으로 다시 뺌
        /// </summary>
        public static RunState ClaimHostFromSave(RunSave save, Member host)
        {
            var rift = new DungeonRift(null, null, null);
            rift.BeginFromSave(save);
            return rift.ClaimDeparted(host.OnlineId, host.Run.PlayerName, true);
        }

        /// <summary>[던전] 멀티 이어하기 세이브를 꺼냄 (한 번만). 없으면 null</summary>
        public RunSave TakeContinueSave()
        {
            var save = ContinueSave;
            ContinueSave = null;
            return save;
        }

        // 주점 씬이 다 뜰 때까지 (NetworkRoot 가 방을 열며 주점을 부름)
        static IEnumerator WaitForTavern()
        {
            float until = Time.realtimeSinceStartup + 10f;
            while (SceneManager.GetActiveScene().name != SceneNames.Tavern && Time.realtimeSinceStartup < until)
            {
                yield return null;
            }
            // 씬 전환 이벤트가 끝나야 다음 LoadScene 이 받아짐
            yield return null;
        }
    }
}
