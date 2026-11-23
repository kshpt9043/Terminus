// 씬이 뜨면 그 씬의 화면을 띄움 (언리얼: 각 PlayerController 의 BeginPlay 에서 위젯 생성)
// UIRoot(레이어) 는 한 번만 만들어 씬을 넘어 유지. 채팅 / 알림은 모든 씬 공통
using Terminus.Game;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Screens;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI
{
    public static class UIApp
    {
        static VisualElement _current;
        static ChatOverlay _chat;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            _current = null;
            _chat = null;
            GameBootstrap.SceneReady -= OnSceneReady;
            GameBootstrap.SceneReady += OnSceneReady;
        }

        public static UIRoot EnsureRoot()
        {
            if (UIRoot.Instance != null) return UIRoot.Instance;
            var go = new GameObject("UIRoot");
            go.SetActive(false);
            var doc = go.AddComponent<UIDocument>();
            doc.panelSettings = Resources.Load<PanelSettings>("UI/TerminusPanelSettings");
            if (doc.panelSettings == null) Debug.LogError("[UI] Resources/UI/TerminusPanelSettings 없음 (메뉴 Terminus/Setup/Build Project Assets)");
            go.AddComponent<UIRoot>();
            go.SetActive(true);
            return UIRoot.Instance;
        }

        static void OnSceneReady(string scene)
        {
            var root = EnsureRoot();
            if (root == null) return;
            if (_current != null) root.Hide(_current);
            _current = null;

            switch (scene)
            {
                case SceneNames.MainMenu:
                    _current = new MainMenuScreen();
                    root.Show(UILayer.MenuWindow, _current);
                    break;
                case SceneNames.Tavern:
                    _current = new TavernScreen();
                    root.Show(UILayer.Map, _current);
                    break;
                case SceneNames.Dungeon:
                    AreaView.Create();
                    _current = new DungeonScreen();
                    root.Show(UILayer.Map, _current);
                    break;
            }

            if (scene != SceneNames.MainMenu)
            {
                if (_chat == null) _chat = new ChatOverlay();
                root.Show(UILayer.Chat, _chat);
            }
            else if (_chat != null)
            {
                root.Hide(_chat);
            }
        }
    }
}
