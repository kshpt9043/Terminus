// 방 패널 공용 도우미: 화면마다 USS 하나 (Assets/Terminus/UI/Rooms/Resources/UI/Rooms/<이름>.uss) + 내 런 규칙 조회
using System.Collections.Generic;
using Terminus.Game.Client;
using Terminus.Run;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Rooms
{
    static class RoomLooks
    {
        const string Folder = "UI/Rooms/";
        static readonly Dictionary<string, StyleSheet> Sheets = new Dictionary<string, StyleSheet>();

        /// <summary>방 패널 USS 를 붙임 ("MonsterReward", "Rest", "Event", "Shop", "Rescue") + 방 패널 공용(Rooms) + 공용 부품 시트</summary>
        public static void Use(VisualElement e, string sheet)
        {
            UiStyle.Use(e, "Common");
            AddSheet(e, "Rooms");
            AddSheet(e, sheet);
        }

        static void AddSheet(VisualElement e, string sheet)
        {
            if (!Sheets.TryGetValue(sheet, out var s))
            {
                s = Resources.Load<StyleSheet>(Folder + sheet);
                if (s == null) Debug.LogWarning("[UI] USS 없음: Resources/" + Folder + sheet);
                Sheets[sheet] = s;
            }
            if (s != null && !e.styleSheets.Contains(s)) e.styleSheets.Add(s);
        }

        /// <summary>화면 전체를 덮는 그림 (언리얼 Image 앵커 0~1). /Game/... 경로</summary>
        public static VisualElement FullImage(string unrealPath, string name)
        {
            var image = new VisualElement { name = name, pickingMode = PickingMode.Ignore };
            image.AddToClassList("rp-fullimage");
            var tex = UiTextures.LoadUnreal(unrealPath);
            if (tex != null) image.style.backgroundImage = new StyleBackground(tex);
            return image;
        }

        /// <summary>
        /// 내 런 규칙 (클라엔 RunState 만 있음 -> RunPlayer 에 넣어 Core 규칙 그대로: 강화 칸 수 / 유물 칸 수 / 픽업 스킬 단계).
        /// 내 정보가 아직 없으면 null
        /// </summary>
        public static RunPlayer MyRun()
        {
            var me = ClientState.Me;
            if (me == null || me.Run == null) return null;
            var run = new RunPlayer(new System.Random(0));
            run.SetRunState(me.Run);
            return run;
        }

        /// <summary>강화 칸이 꽉 찼나 (언리얼 EnhanceSkills.Num() >= GetEnhanceSlotCount())</summary>
        public static bool EnhanceSlotsFull()
        {
            var run = MyRun();
            return run != null && run.RunState.EnhanceSkills.Count >= run.GetEnhanceSlotCount();
        }

        /// <summary>유물 칸이 꽉 찼나 (언리얼 GetRelics().Num() >= GetRelicCapacity())</summary>
        public static bool RelicSlotsFull()
        {
            var run = MyRun();
            return run != null && run.Relics.Count >= run.GetRelicCapacity();
        }
    }
}
