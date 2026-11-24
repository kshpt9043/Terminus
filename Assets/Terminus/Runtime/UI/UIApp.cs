// 씬이 뜨면 그 씬의 화면을 띄움 (언리얼: 각 PlayerController 의 BeginPlay 에서 위젯 생성)
// UIRoot(레이어) 는 한 번만 만들어 씬을 넘어 유지. 채팅은 메인 메뉴를 뺀 모든 씬
//  - 메인 메뉴: Terminus.UI.Menu.MainMenuView
//  - 주점: Terminus.UI.Menu.TavernView
//  - 던전: Terminus.UI.Dungeon.DungeonView (전투 HUD / 방 패널은 이 화면이 띄움) + 무대 AreaView
using Terminus.Game;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Chat;
using Terminus.UI.Dungeon;
using Terminus.UI.Menu;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI
{
    public static class UIApp
    {
        static VisualElement _current;
        static ChatView _chat;

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
            EnsureEventSystem();
            return UIRoot.Instance;
        }

        // 프로젝트가 새 Input System 만 씀 -> UI Toolkit 이 키보드 / 마우스 / 패드 입력을 받으려면 EventSystem + Input System 모듈이 있어야 함
        // (언리얼은 PlayerController 가 입력을 위젯에 넘겼음)
        static void EnsureEventSystem()
        {
            if (Object.FindAnyObjectByType<UnityEngine.EventSystems.EventSystem>() != null) return;
            var es = new GameObject("EventSystem");
            Object.DontDestroyOnLoad(es);
            es.AddComponent<UnityEngine.EventSystems.EventSystem>();
            es.AddComponent<UnityEngine.InputSystem.UI.InputSystemUIInputModule>();
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
                    _current = new MainMenuView();
                    break;
                case SceneNames.Tavern:
                    _current = new TavernView();
                    break;
                case SceneNames.Dungeon:
                    StageBackgroundView.Create();
                    AreaView.Create();
                    _current = new DungeonView();
                    break;
            }
            if (_current != null) root.Show(UILayer.Map, _current);

            if (scene != SceneNames.MainMenu)
            {
                if (_chat == null) _chat = new ChatView();
                root.Show(UILayer.Chat, _chat);
            }
            else if (_chat != null)
            {
                root.Hide(_chat);
            }
        }
    }
}
