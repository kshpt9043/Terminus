// 화면 하나 = UIDocument 하나 대신, UIDocument 하나에 레이어(UILayer) 순서대로 칸을 만들어 거기에 화면을 붙인다
// 언리얼 AddToViewport(ZOrder) 자리. 화면 쪽은 UIRoot.Instance.Show(UILayer.X, element) 만 부르면 됨
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI
{
    [RequireComponent(typeof(UIDocument))]
    public sealed class UIRoot : MonoBehaviour
    {
        public static UIRoot Instance { get; private set; }

        [SerializeField] StyleSheet _theme;

        readonly Dictionary<UILayer, VisualElement> _layers = new Dictionary<UILayer, VisualElement>();
        UIDocument _document;

        public VisualElement Root => _document != null ? _document.rootVisualElement : null;

        void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }
            Instance = this;
            DontDestroyOnLoad(gameObject);
        }

        void OnEnable()
        {
            _document = GetComponent<UIDocument>();
            BuildLayers();
        }

        void OnDestroy()
        {
            if (Instance == this) Instance = null;
        }

        void BuildLayers()
        {
            var root = Root;
            if (root == null) return;
            if (_theme != null && !root.styleSheets.Contains(_theme)) root.styleSheets.Add(_theme);

            _layers.Clear();
            root.Clear();
            var values = (UILayer[])Enum.GetValues(typeof(UILayer));
            Array.Sort(values, (a, b) => ((int)a).CompareTo((int)b));
            foreach (var layer in values)
            {
                var container = new VisualElement { name = "Layer_" + layer, pickingMode = PickingMode.Ignore };
                container.AddToClassList("t-layer");
                root.Add(container); // 뒤에 붙은 것이 위에 그려짐
                _layers[layer] = container;
            }
        }

        public VisualElement GetLayer(UILayer layer)
        {
            if (_layers.Count == 0) BuildLayers();
            return _layers.TryGetValue(layer, out var v) ? v : null;
        }

        /// <summary>그 레이어에 화면을 붙임. 이미 붙어 있으면 맨 위로</summary>
        public void Show(UILayer layer, VisualElement screen)
        {
            var container = GetLayer(layer);
            if (container == null || screen == null) return;
            if (screen.parent == container)
            {
                screen.BringToFront();
                return;
            }
            screen.RemoveFromHierarchy();
            screen.AddToClassList("t-layer");
            container.Add(screen);
        }

        public void Hide(VisualElement screen)
        {
            screen?.RemoveFromHierarchy();
        }
    }
}
