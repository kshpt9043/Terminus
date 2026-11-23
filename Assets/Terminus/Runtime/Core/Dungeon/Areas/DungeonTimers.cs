// 언리얼 FTimerManager 대신 (구역 / 구출 투표의 SetTimer). 서버 시각(초)을 Game 쪽이 매 프레임 Tick 으로 넘김
// 같은 핸들에 다시 Set 하면 앞의 것은 취소 (언리얼 SetTimer 와 같음)
using System;
using System.Collections.Generic;

namespace Terminus.Dungeon
{
    /// <summary>타이머 하나 (언리얼 FTimerHandle). 주인이 들고 있다가 Set / Clear 에 넘김</summary>
    public sealed class TimerHandle
    {
        internal bool Active;
        internal double At;
        internal long Order;
        internal Action Callback;

        public bool IsActive => Active;
    }

    public sealed class DungeonTimers
    {
        readonly List<TimerHandle> _active = new List<TimerHandle>();
        long _nextOrder;

        public DungeonTimers(double now = 0)
        {
            Now = now;
        }

        /// <summary>지금 서버 시각 (초). Tick 으로만 앞으로 감</summary>
        public double Now { get; private set; }

        /// <summary>다음 타이머가 울리는 시각. 없으면 null</summary>
        public double? NextTime
        {
            get
            {
                double? best = null;
                foreach (var h in _active)
                {
                    if (!best.HasValue || h.At < best.Value) best = h.At;
                }
                return best;
            }
        }

        public void Set(TimerHandle handle, double delaySeconds, Action callback)
        {
            if (handle == null || callback == null) return;

            Clear(handle);
            handle.Active = true;
            handle.At = Now + Math.Max(0, delaySeconds);
            handle.Order = _nextOrder++;
            handle.Callback = callback;
            _active.Add(handle);
        }

        public void Clear(TimerHandle handle)
        {
            if (handle == null || !handle.Active) return;

            handle.Active = false;
            handle.Callback = null;
            _active.Remove(handle);
        }

        /// <summary>시각을 now 로 옮기며 그 사이 타이머를 순서대로 울림. 콜백 안에서 건 타이머는 울린 시각 기준</summary>
        public void Tick(double now)
        {
            while (true)
            {
                TimerHandle due = null;
                foreach (var h in _active)
                {
                    if (h.At > now) continue;
                    if (due == null || h.At < due.At || (h.At == due.At && h.Order < due.Order)) due = h;
                }
                if (due == null) break;

                if (due.At > Now) Now = due.At;
                var cb = due.Callback;
                Clear(due);
                cb?.Invoke();
            }
            if (now > Now) Now = now;
        }

        /// <summary>테스트 / 고정 프레임용: 지금부터 seconds 만큼</summary>
        public void Advance(double seconds) => Tick(Now + Math.Max(0, seconds));
    }
}
