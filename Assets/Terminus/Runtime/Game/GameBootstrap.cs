// 씬이 뜰 때마다: 서버면 그 씬의 서버 로직을 켜고, 네트워크 없이 던전 / 주점 씬을 바로 켰으면 싱글로 시작
// (언리얼: 에디터에서 Lv_Dungeon 을 바로 PIE 하면 주점을 안 거치고 기본 스텟으로 시작하던 것)
using Terminus.Game.Net;
using Unity.Netcode;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Terminus.Game
{
    public static class GameBootstrap
    {
        public static event System.Action<string> SceneReady;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Init()
        {
            SceneManager.sceneLoaded -= OnSceneLoaded;
            SceneManager.sceneLoaded += OnSceneLoaded;
            OnSceneLoaded(SceneManager.GetActiveScene(), LoadSceneMode.Single);
        }

        static void OnSceneLoaded(Scene scene, LoadSceneMode mode)
        {
            if (mode != LoadSceneMode.Single) return;

            bool connected = NetBus.IsConnected;
            if (!connected && (scene.name == SceneNames.Dungeon || scene.name == SceneNames.Tavern))
            {
                // 바로 켠 씬: 혼자 방장으로 열고 이 씬에서 시작
                NetworkRoot.StartSingleInPlace(scene.name == SceneNames.Dungeon);
            }

            if (scene.name == SceneNames.Dungeon) DungeonServer.StartIfServer();
            if (scene.name == SceneNames.MainMenu) Client.ClientState.Reset();

            SceneReady?.Invoke(scene.name);
        }
    }
}
