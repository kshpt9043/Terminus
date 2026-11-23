// 던전 화면 공용: USS 로드, 지도 시안 색 (언리얼 TerminusUIColors 의 MapColor), 텍스처 경로
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    internal static class DungeonStyle
    {
        static StyleSheet _sheet;
        static bool _loaded;

        /// <summary>Assets/Terminus/UI/Dungeon/Resources/UI/Dungeon/Dungeon.uss 를 붙임</summary>
        public static void Use(VisualElement element)
        {
            if (element == null) return;
            if (!_loaded)
            {
                _loaded = true;
                _sheet = Resources.Load<StyleSheet>("UI/Dungeon/Dungeon");
                if (_sheet == null) Debug.LogWarning("[UI] USS 없음: Resources/UI/Dungeon/Dungeon");
            }
            if (_sheet != null && !element.styleSheets.Contains(_sheet)) element.styleSheets.Add(_sheet);
        }

        public static Texture2D Tex(string unrealPath) => UiTextures.LoadUnreal(unrealPath);

        public static void SetImage(VisualElement e, string unrealPath)
        {
            var tex = Tex(unrealPath);
            if (tex != null) e.style.backgroundImage = new StyleBackground(tex);
        }

        public static void Fill(VisualElement e)
        {
            e.style.position = Position.Absolute;
            e.style.left = 0;
            e.style.top = 0;
            e.style.right = 0;
            e.style.bottom = 0;
        }

        public static void Box(VisualElement e, Color fill, Color outline, float outlineWidth = 1f, float radius = 0f)
        {
            e.style.backgroundColor = fill;
            UiStyle.Border(e, outlineWidth, outline);
            if (radius > 0f) UiStyle.Radius(e, radius);
        }
    }

    /// <summary>지도 시안 색 (언리얼 TerminusUI::MapColor, Theme/Terminus.uss 의 --map-*)</summary>
    internal static class MapColor
    {
        public static readonly Color OutsidePaper = UiColor.Hex("0E0E10");
        public static readonly Color TopBar = UiColor.Hex("313131");
        public static readonly Color Plate = UiColor.Hex("23202A");
        public static readonly Color PartyPlate = UiColor.Hex("3A253D");
        public static readonly Color PartyEdge = UiColor.Hex("24162A");
        public static readonly Color ChipFill = UiColor.Hex("5A4A5C");
        public static readonly Color Health = UiColor.Hex("FF1A4C");
        public static readonly Color HealthBack = UiColor.Hex("1A1218");
        public static readonly Color Text = UiColor.Hex("E8DFCB");
        public static readonly Color TextSub = UiColor.Hex("A89F8C");
        public static readonly Color BlueTorch = UiColor.Hex("5B8BD9");
        public static readonly Color GreenMana = UiColor.Hex("7BC46A");
    }

    internal static class MapTextures
    {
        public const string Paper = "/Game/UI/Textures/Map/T_Map_Paper";
        public const string Vignette = "/Game/UI/Textures/Map/T_Map_Vignette";
        public const string Glow = "/Game/UI/Textures/Map/T_Map_Glow";
        public const string VisitedX = "/Game/UI/Textures/Map/T_Map_VisitedX";
        public const string CurrentRing = "/Game/UI/Textures/Map/T_Map_CurrentRing";
        public const string SelectCircle = "/Game/UI/Textures/Map/T_Map_SelectCircle";
        public const string InkDot = "/Game/UI/Textures/Map/T_Map_InkDot";
        public const string SoftDot = "/Game/UI/Textures/Map/T_Map_SoftDot";
        public const string Coin = "/Game/UI/Textures/Map/T_Map_Coin";

        // WBP_MapCanvasWidget 의 RoomTypeIcons
        public static string RoomIcon(Terminus.Map.RoomType type)
        {
            switch (type)
            {
                case Terminus.Map.RoomType.MONSTER: return "/Game/UI/Textures/Map/T_Room_Monster";
                case Terminus.Map.RoomType.BREAK: return "/Game/UI/Textures/Map/T_Room_Rest";
                case Terminus.Map.RoomType.STORE: return "/Game/UI/Textures/Map/T_Room_Shop";
                case Terminus.Map.RoomType.GUARDIAN: return "/Game/UI/Textures/Map/T_Room_Guardian";
                case Terminus.Map.RoomType.EVENT: return "/Game/UI/Textures/Map/T_Room_Event";
                case Terminus.Map.RoomType.BOSS: return "/Game/UI/Textures/Map/T_Room_Boss";
                case Terminus.Map.RoomType.QUEST: return "/Game/UI/Textures/Map/T_Room_Quest";
                default: return "";
            }
        }
    }
}
