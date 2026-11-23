// Steam 시작 (언리얼 OnlineSubsystemSteam, DefaultEngine.ini SteamDevAppId=480)
//  - 빌드: Steam 이 켜져 있으면 Steam 모드 (로비 / 초대 / 릴레이). 실행 인자 -nosteam 이면 IP 모드
//  - 에디터: 기본 IP 모드 (Multiplayer Play Mode 복제는 같은 Steam 계정이라 Steam 으로 서로 못 붙음). 메인 메뉴에서 바꿀 수 있음
// 진짜 AppID 가 나오면 AppId 하나만 바꾸면 됨 (언리얼 마일스톤 M5)
using System;
using Steamworks;
using UnityEngine;

namespace Terminus.Game.Net.Steam
{
    public sealed class SteamBootstrap : MonoBehaviour
    {
        public const uint AppId = 480;

        public static bool Available { get; private set; }
        public static string LastError { get; private set; } = "";

        static SteamRoomDirectory _directory;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            bool noSteam = Array.Exists(Environment.GetCommandLineArgs(), a => a == "-nosteam");
            bool mppmClone = Application.dataPath.Replace('\\', '/').Contains("/Library/VP/");
#if UNITY_EDITOR
            bool wantSteam = !noSteam && !mppmClone && PlayerPrefsWantSteamInEditor;
#else
            bool wantSteam = !noSteam;
#endif
            if (wantSteam) TryStart();
            NetworkRoot.Mode = Available ? TransportMode.Steam : TransportMode.UnityTransport;
        }

        /// <summary>에디터에서 Steam 모드를 쓸지 (메인 메뉴 토글이 저장)</summary>
        public static bool PlayerPrefsWantSteamInEditor
        {
            get => PlayerPrefs.GetInt("Terminus.EditorUseSteam", 0) == 1;
            set => PlayerPrefs.SetInt("Terminus.EditorUseSteam", value ? 1 : 0);
        }

        /// <summary>Steam 시작. 이미 켜져 있으면 true. Steam 클라이언트가 안 켜져 있으면 false</summary>
        public static bool TryStart()
        {
            if (Available && SteamClient.IsValid) return true;
            try
            {
                if (!SteamClient.IsValid) SteamClient.Init(AppId, false);
                Available = SteamClient.IsValid;
                LastError = "";
            }
            catch (Exception e)
            {
                Available = false;
                LastError = "Steam 을 시작할 수 없습니다. Steam 이 켜져 있는지 확인하세요.";
                Debug.LogWarning("[Steam] 시작 실패: " + e.Message);
            }

            if (Available)
            {
                if (_directory == null) _directory = new SteamRoomDirectory();
                RoomDirectory.SetBackend(_directory);
                var go = new GameObject("SteamPump");
                DontDestroyOnLoad(go);
                go.AddComponent<SteamBootstrap>();
                if (string.IsNullOrWhiteSpace(NetworkRoot.LocalPlayerName)) NetworkRoot.LocalPlayerName = SteamClient.Name;
                Debug.Log($"[Steam] 시작: {SteamClient.Name} ({SteamClient.SteamId.Value})");
            }
            return Available;
        }

        /// <summary>Steam 을 끄고 IP 모드로 (메인 메뉴 토글)</summary>
        public static void UseIpMode()
        {
            NetworkRoot.Mode = TransportMode.UnityTransport;
            RoomDirectory.SetBackend(null);
        }

        public static void UseSteamMode()
        {
            if (TryStart())
            {
                NetworkRoot.Mode = TransportMode.Steam;
                RoomDirectory.SetBackend(_directory);
            }
        }

        // 메뉴처럼 네트워크가 안 돌 때도 Steam 콜백(로비 검색 / 초대)을 돌림. 게임 중엔 트랜스포트도 부름 (두 번 불러도 무해)
        void Update()
        {
            if (SteamClient.IsValid) SteamClient.RunCallbacks();
        }

        void OnApplicationQuit()
        {
            if (SteamClient.IsValid) SteamClient.Shutdown();
        }
    }
}
