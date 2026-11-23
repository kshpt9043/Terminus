// UI 텍스처 찾기 (캐시). 언리얼 /Game/X/Y/T_Z 는 Resources/UI/Textures/X/Y/T_Z.png 로 옮겨져 있음
//   /Game/UI/Textures/Common/T_UI_Hex_Primary_Normal -> Resources 경로 "UI/Textures/UI/Textures/Common/T_UI_Hex_Primary_Normal"
//   /Game/KSH/Asset/T_UI_Btn_Hex                    -> "UI/Textures/KSH/Asset/T_UI_Btn_Hex"
// (USS 에서는 background-image: resource("UI/Textures/UI/Textures/Common/T_UI_GoldDivider") 처럼 같은 경로)
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public static class UiTextures
    {
        public const string Root = "UI/Textures/";

        // 자주 쓰는 것 (Resources 경로)
        public const string HexDisabled = Root + "UI/Textures/Common/T_UI_Hex_Disabled";
        public const string GoldDivider = Root + "UI/Textures/Common/T_UI_GoldDivider";
        public const string Coin = Root + "KSH/Asset/T_UI_Coin";
        public const string SelectGlow = Root + "KSH/Asset/T_UI_SelectGlow";
        public const string GoldLine = Root + "KSH/Asset/T_UI_GoldLine";

        /// <summary>육각 버튼 판 (kind = Primary / Secondary / Gold, state = Normal / Hover / Pressed)</summary>
        public static string Hex(string kind, string state) => Root + "UI/Textures/Common/T_UI_Hex_" + kind + "_" + state;

        static readonly Dictionary<string, Texture2D> Textures = new Dictionary<string, Texture2D>();
        static readonly Dictionary<string, Sprite> Sprites = new Dictionary<string, Sprite>();

        /// <summary>언리얼 경로 ("/Game/A/B/T_X.T_X" 또는 "/Script/Engine.Texture2D'/Game/A/B/T_X.T_X'") -> Resources 경로 "UI/Textures/A/B/T_X"</summary>
        public static string FromUnrealPath(string unrealPath)
        {
            if (string.IsNullOrWhiteSpace(unrealPath) || unrealPath == "None") return "";
            var s = unrealPath.Trim().Trim('\'', '"');
            int quote = s.IndexOf('\'');
            if (quote >= 0) s = s.Substring(quote + 1).Trim('\'');
            const string game = "/Game/";
            int g = s.IndexOf(game, System.StringComparison.Ordinal);
            if (g >= 0) s = s.Substring(g + game.Length);
            int dot = s.LastIndexOf('.');
            int slash = s.LastIndexOf('/');
            if (dot > slash) s = s.Substring(0, dot);
            return Root + s;
        }

        /// <summary>Resources 경로 ("UI/Textures/...") -> 텍스처. 없으면 null (한 번만 찾음)</summary>
        public static Texture2D Load(string resourcePath)
        {
            if (string.IsNullOrEmpty(resourcePath)) return null;
            if (Textures.TryGetValue(resourcePath, out var cached)) return cached;
            var tex = Resources.Load<Texture2D>(resourcePath);
            if (tex == null) Debug.LogWarning("[UI] 텍스처 없음: Resources/" + resourcePath);
            Textures[resourcePath] = tex;
            return tex;
        }

        /// <summary>스프라이트로 (UI 텍스처는 Sprite 로 임포트되어 있음)</summary>
        public static Sprite LoadSprite(string resourcePath)
        {
            if (string.IsNullOrEmpty(resourcePath)) return null;
            if (Sprites.TryGetValue(resourcePath, out var cached)) return cached;
            var sprite = Resources.Load<Sprite>(resourcePath);
            Sprites[resourcePath] = sprite;
            return sprite;
        }

        public static Texture2D LoadUnreal(string unrealPath) => Load(FromUnrealPath(unrealPath));

        /// <summary>요소 바탕 그림으로. 텍스처가 없으면 바탕 그림을 지움</summary>
        public static void SetBackground(VisualElement element, string resourcePath)
        {
            if (element == null) return;
            var tex = Load(resourcePath);
            element.style.backgroundImage = tex != null ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
        }
    }
}
