// 원형 로딩 표시 (언리얼 UCircularThrobber 자리: 점 6개가 원을 그리며 돎)
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class Throbber : VisualElement
    {
        /// <summary>한 바퀴 도는 시간 (초). 언리얼 기본 Period 0.75</summary>
        public float Period = 0.75f;

        readonly VisualElement _wheel;
        IVisualElementScheduledItem _tick;
        float _startTime;

        /// <param name="radius">원 반지름 (언리얼 Radius). 칸 크기 = 2 x radius</param>
        public Throbber(float radius = 28f, int pieces = 6, Color? color = null)
        {
            pickingMode = PickingMode.Ignore;
            style.width = radius * 2f;
            style.height = radius * 2f;
            style.flexShrink = 0;

            _wheel = new VisualElement { pickingMode = PickingMode.Ignore };
            _wheel.style.position = Position.Absolute;
            _wheel.style.left = 0;
            _wheel.style.top = 0;
            _wheel.style.right = 0;
            _wheel.style.bottom = 0;
            Add(_wheel);

            float dot = Mathf.Max(4f, radius * 0.3f);
            var c = color ?? UiColor.TextSub;
            for (int i = 0; i < pieces; ++i)
            {
                float a = i * Mathf.PI * 2f / pieces;
                var d = new VisualElement { pickingMode = PickingMode.Ignore };
                d.style.position = Position.Absolute;
                d.style.width = dot;
                d.style.height = dot;
                d.style.left = radius + Mathf.Cos(a) * (radius - dot * 0.5f) - dot * 0.5f;
                d.style.top = radius + Mathf.Sin(a) * (radius - dot * 0.5f) - dot * 0.5f;
                UiStyle.Radius(d, dot * 0.5f);
                d.style.backgroundColor = c;
                d.style.opacity = 0.25f + 0.75f * (i + 1) / pieces;   // 꼬리가 흐려지게
                _wheel.Add(d);
            }

            RegisterCallback<AttachToPanelEvent>(_ => Start());
            RegisterCallback<DetachFromPanelEvent>(_ => _tick?.Pause());
        }

        void Start()
        {
            _startTime = Time.realtimeSinceStartup;
            if (_tick == null) _tick = schedule.Execute(Spin).Every(16);
            else _tick.Resume();
        }

        void Spin()
        {
            float t = (Time.realtimeSinceStartup - _startTime) / Mathf.Max(0.05f, Period);
            _wheel.style.rotate = new Rotate(new Angle((t % 1f) * 360f, AngleUnit.Degree));
        }
    }
}
