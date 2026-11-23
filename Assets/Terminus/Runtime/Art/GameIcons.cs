// 유물 / 스킬 아이콘 찾기 (언리얼 UTerminusDataSettings::FindSkillIcon, FRelicRow::Icon)
//  - 유물: DT_Relic Icon 열의 언리얼 경로 "/Game/KSH/Asset/RelicIcon/T_Relic_X.T_Relic_X" -> 파일 이름 T_Relic_X
//  - 스킬: DT_Skill IconID -> T_Icon_<IconID>. 없으면 종류별 기본 아이콘 (Icons/Skills/Default_<Attack|Defense|Special>, 아직 없음)
using System.Collections.Generic;
using UnityEngine;

namespace Terminus.Art
{
    public static class GameIcons
    {
        const string RelicFolder = "Icons/Relics/";
        const string SkillFolder = "Icons/Skills/";

        static readonly Dictionary<string, Sprite> Cache = new Dictionary<string, Sprite>();

        public static Sprite FindRelicIcon(string iconPath)
        {
            var name = AssetNameFromPath(iconPath);
            return string.IsNullOrEmpty(name) ? null : Load(RelicFolder + name);
        }

        /// <param name="skillType">"Attack" / "Defense" / "Special" (IconID 로 못 찾을 때 기본 아이콘)</param>
        public static Sprite FindSkillIcon(string iconId, string skillType)
        {
            if (!string.IsNullOrEmpty(iconId) && iconId != "None")
            {
                var s = Load(SkillFolder + "T_Icon_" + iconId) ?? Load(SkillFolder + iconId);
                if (s != null) return s;
            }
            return string.IsNullOrEmpty(skillType) ? null : Load(SkillFolder + "Default_" + skillType);
        }

        // "/Game/A/B/T_X.T_X" -> "T_X", "T_X.png" -> "T_X"
        public static string AssetNameFromPath(string path)
        {
            if (string.IsNullOrWhiteSpace(path) || path == "None") return "";
            var s = path.Trim();
            int slash = s.LastIndexOf('/');
            if (slash >= 0) s = s.Substring(slash + 1);
            int dot = s.IndexOf('.');
            if (dot >= 0) s = s.Substring(0, dot);
            return s;
        }

        static Sprite Load(string resourcePath)
        {
            if (Cache.TryGetValue(resourcePath, out var cached)) return cached;
            var sprite = Resources.Load<Sprite>(resourcePath);
            Cache[resourcePath] = sprite;
            return sprite;
        }
    }
}
