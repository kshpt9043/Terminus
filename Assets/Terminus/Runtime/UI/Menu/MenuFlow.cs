// 메뉴 쪽 흐름을 화면 밖에서 잡아 두는 곳 (언리얼 USessionSubsystem / ULoadingScreenSubsystem / PlayerController 의 화면 관련 부분)
//  - 끊김 / 참가 실패 이유: 화면이 없을 때 와도 들고 있다가 메인 메뉴가 뜨면 보여 줌 (언리얼 ConsumeDisconnectReason)
//  - 로딩 화면: 방 열기 / 참가 / 씬 이동 동안. 도착한 화면이 걷고, 못 걷으면 시간이 지나 걷음 (언리얼 MaxShowSeconds)
//  - 런 끝(RunEndedMsg): 정산 대기를 바로 내 프로필에 저장 -> "정산하러 가기" -> 메인 메뉴에서 정산 (언리얼 Client_BeginSettlement)
//  - 이어하기 목록 (RunSaveService 하나를 메뉴 화면들이 같이 씀)
//  - 내 이름 (실행 인자 -name / Steam 이름이 먼저, 아니면 PlayerPrefs "Terminus.Name")
using System;
using Terminus.Data;
using Terminus.Game;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;

namespace Terminus.UI.Menu
{
    public static class MenuFlow
    {
        public const string NamePrefKey = "Terminus.Name";
        public const string AddressPrefKey = "Terminus.LastAddress";
        public const string DefaultName = "모험가";
        public const int MaxNameLength = 16;

        // 로딩 화면을 못 걷었을 때 최대 몇 초 (언리얼 ULoadingScreenSettings::MaxShowSeconds)
        const float MaxLoadingSeconds = 25f;

        static RunSaveService _saves;
        static int _loadingToken;
        static bool _waitDungeonMap;
        static ConfirmPopupWidget _runEndPopup;

        /// <summary>아직 안 보여 준 끊김 / 실패 이유 (메인 메뉴가 꺼내 감)</summary>
        public static string PendingFailure { get; private set; } = "";

        /// <summary>연결 실패 / 끊김 (메인 메뉴가 떠 있으면 바로 받음)</summary>
        public static event Action<string> FailureRaised;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            PendingFailure = "";
            _saves = null;
            _waitDungeonMap = false;
            _runEndPopup = null;

            NetworkRoot.ConnectionFailed -= OnConnectionFailed;
            NetworkRoot.ConnectionFailed += OnConnectionFailed;
            GameBootstrap.SceneReady -= OnSceneReady;
            GameBootstrap.SceneReady += OnSceneReady;
            ClientState.MapChanged -= OnMapChanged;
            ClientState.MapChanged += OnMapChanged;

            // NetBus 처리기는 Play 마다 NetBus 가 비움 (SubsystemRegistration) -> 매번 붙임
            NetBus.On<RunEndedMsg>(OnRunEnded);
        }

        // ------------------------------------------------------------ 세이브 목록

        /// <summary>이 컴퓨터의 런 세이브 (이어하기 목록 / 메인 메뉴 이어하기 버튼)</summary>
        public static RunSaveService Saves
        {
            get
            {
                if (_saves == null) _saves = new RunSaveService(LocalProfile.Storage, LocalProfile.SaveSerializer, () => DateTime.Now);
                return _saves;
            }
        }

        // ------------------------------------------------------------ 이름

        /// <summary>처음 보여 줄 이름: 실행 인자 / Steam 이름이 먼저, 아니면 저장해 둔 이름</summary>
        public static string InitialName()
        {
            if (!string.IsNullOrWhiteSpace(NetworkRoot.LocalPlayerName)) return NetworkRoot.LocalPlayerName.Trim();
            return PlayerPrefs.GetString(NamePrefKey, DefaultName);
        }

        /// <summary>방을 열거나 들어가기 전에 내 이름을 정함 (NetworkRoot.LocalPlayerName). 직접 바꾼 이름이면 저장</summary>
        public static void ApplyName(string typed)
        {
            var name = (typed ?? "").Trim();
            if (name.Length == 0) name = InitialName();
            if (name.Length == 0) name = DefaultName;
            if (name.Length > MaxNameLength) name = name.Substring(0, MaxNameLength);

            // 실행 인자(-name)로 받은 이름은 저장하지 않음 (같은 PC 에 여러 개 띄울 때 서로 덮어쓰지 않게)
            var launch = LaunchOptions.Get("-name");
            if (string.IsNullOrWhiteSpace(launch) || launch.Trim() != name)
            {
                PlayerPrefs.SetString(NamePrefKey, name);
                PlayerPrefs.Save();
            }
            NetworkRoot.LocalPlayerName = name;
        }

        // ------------------------------------------------------------ 끊김 / 실패

        static void OnConnectionFailed(string reason)
        {
            HideLoading();
            PendingFailure = string.IsNullOrEmpty(reason) ? "연결이 끊어졌습니다." : reason;
            FailureRaised?.Invoke(PendingFailure);
        }

        /// <summary>보여 줄 이유를 꺼냄 (꺼내면 비워짐)</summary>
        public static string ConsumeFailure()
        {
            var r = PendingFailure;
            PendingFailure = "";
            return r;
        }

        // ------------------------------------------------------------ 로딩 화면

        /// <summary>로딩 화면 (언리얼 ULoadingScreenSubsystem::Show / ShowIfHidden). 너무 오래 걸리면 알아서 걷음</summary>
        public static void ShowLoading(string message, LoadingStyle style = LoadingStyle.Full, bool onlyIfHidden = false)
        {
            if (onlyIfHidden && LoadingScreenWidget.IsShowing)
            {
                // 크게 덮고 있으면 그대로, 작게 덮고 있는데 크게 덮으라면 바꿈
                var current = LoadingScreenWidget.Current;
                if (current == null || current.Style == LoadingStyle.Full || style == LoadingStyle.Light) return;
            }
            var widget = LoadingScreenWidget.Show(message, style);
            if (widget == null) return;

            int token = ++_loadingToken;
            widget.schedule.Execute(() =>
            {
                if (token != _loadingToken || !LoadingScreenWidget.IsShowing) return;
                Debug.LogWarning("[Loading] 준비 신호가 안 와서 로딩 화면을 걷음 (" + message + ")");
                HideLoading();
            }).ExecuteLater((long)(MaxLoadingSeconds * 1000f));
        }

        public static void HideLoading()
        {
            ++_loadingToken;
            _waitDungeonMap = false;
            LoadingScreenWidget.Hide();
        }

        /// <summary>던전으로 가는 중 (주점 출발 / 이어하기). 던전 지도가 처음 오면 걷음</summary>
        public static void ShowDungeonTravel()
        {
            ShowLoading("던전으로 이동하는 중...", LoadingStyle.Full, true);
            _waitDungeonMap = true;
        }

        static void OnSceneReady(string scene)
        {
            if (scene == SceneNames.MainMenu)
            {
                // 런 끝 안내가 남아 있으면 치움 (끊겨서 먼저 메뉴로 온 경우)
                if (_runEndPopup != null)
                {
                    _runEndPopup.RemoveFromHierarchy();
                    _runEndPopup = null;
                }
            }
            else if (scene == SceneNames.Dungeon && LoadingScreenWidget.IsShowing)
            {
                // 던전 지도가 처음 오면 걷음 (던전 화면이 먼저 걷어도 무해)
                _waitDungeonMap = true;
            }
        }

        static void OnMapChanged()
        {
            if (!_waitDungeonMap) return;
            if (UnityEngine.SceneManagement.SceneManager.GetActiveScene().name != SceneNames.Dungeon) return;
            _waitDungeonMap = false;
            HideLoading();
        }

        // ------------------------------------------------------------ 런 끝 -> 정산 (언리얼 Client_BeginSettlement)

        static void OnRunEnded(RunEndedMsg msg)
        {
            if (msg == null) return;

            // 정산 내용은 바로 내 프로필에 저장 (방장이 먼저 나가 끊겨도 메인 화면에서 이어서 정산)
            try
            {
                var profile = LocalProfile.Service;
                var settlement = SettlementRules.MakeFromRun(profile, msg.Run ?? new RunState(), msg.Adjust, msg.Message, msg.RoomName, msg.Floor, msg.bDeath);
                profile.BeginSettlement(settlement);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }

            HideLoading();
            if (_runEndPopup != null) _runEndPopup.RemoveFromHierarchy();
            _runEndPopup = ConfirmPopupWidget.Show(msg.bDeath ? "전멸" : "던전 탈출",
                (msg.Message ?? "") + (msg.bDeath
                    ? "\n강화 스킬과 던전 재화는 사라집니다.\n유물은 메인 화면 정산에서 보관하거나 판매합니다."
                    : "\n메인 화면에서 정산합니다."),
                LeaveForSettlement, "정산하러 가기", "", LeaveForSettlement);
        }

        static void LeaveForSettlement()
        {
            _runEndPopup = null;
            LeaveToMenu();
        }

        /// <summary>나가서 메인 메뉴로 (언리얼 USessionSubsystem::LeaveToMenu: "메인 화면으로 나가는 중...")</summary>
        public static void LeaveToMenu()
        {
            ServerSession.ClearPendingContinue();
            ShowLoading("메인 화면으로 나가는 중...", LoadingStyle.Light, true);
            NetworkRoot.LeaveToMenu();
            // 이미 메인 메뉴였으면 씬이 다시 안 뜸 -> 여기서 걷음
            if (UnityEngine.SceneManagement.SceneManager.GetActiveScene().name == SceneNames.MainMenu) HideLoading();
        }
    }
}
