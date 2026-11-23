// Art/Monsters/<이름>/<동작>/frame_N.png -> 플립북 6개 + MonsterVisualSet
// 언리얼 PixelLab/tools/ue_monster_flipbooks.py 자리. 새 몬스터 그림을 넣으면 메뉴 한 번
//   Terminus/Art/Build Monster Visuals
// 결과: Art/Monsters/<이름>/FB_<이름>_<동작>.asset, Resources/Visuals/Monsters/<이름>.asset
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

namespace Terminus.Art.EditorTools
{
    public static class MonsterVisualBuilder
    {
        // 몬스터: PixelLab 그림, 왼쪽을 봄. 플레이어 직업: 언리얼 TempSprites 임시 그림(대기만), 오른쪽을 봄
        static readonly (string artRoot, string visualsRoot, bool facesLeft)[] Groups =
        {
            ("Assets/Terminus/Art/Monsters", "Assets/Terminus/Resources/Visuals/Monsters", true),
            ("Assets/Terminus/Art/Characters", "Assets/Terminus/Resources/Visuals/Characters", false),
        };

        static readonly (MonsterAction action, string folder)[] Actions =
        {
            (MonsterAction.Idle, "idle"),
            (MonsterAction.Attack, "attack"),
            (MonsterAction.Hit, "hit"),
            (MonsterAction.Defend, "defend"),
            (MonsterAction.Special, "special"),
            (MonsterAction.Death, "death"),
        };

        [MenuItem("Terminus/Art/Build Monster Visuals")]
        public static void BuildAllMenu()
        {
            var report = BuildAll();
            Debug.Log(report);
        }

        /// <summary>CLI 에서도 부름: unity command script eval 'return Terminus.Art.EditorTools.MonsterVisualBuilder.BuildAll();'</summary>
        public static string BuildAll()
        {
            var lines = new List<string>();
            foreach (var (artRoot, visualsRoot, facesLeft) in Groups)
            {
                if (!AssetDatabase.IsValidFolder(artRoot)) continue;
                EnsureFolder(visualsRoot);
                foreach (var dir in Directory.GetDirectories(artRoot).OrderBy(d => d))
                {
                    lines.Add(Build(artRoot, visualsRoot, Path.GetFileName(dir), facesLeft));
                }
            }
            AssetDatabase.SaveAssets();
            return "[MonsterVisualBuilder] " + lines.Count + "종\n" + string.Join("\n", lines);
        }

        public static string Build(string artRoot, string visualsRoot, string name, bool facesLeft)
        {
            var monsterDir = artRoot + "/" + name;
            var visualPath = visualsRoot + "/" + name + ".asset";
            var visual = AssetDatabase.LoadAssetAtPath<MonsterVisualSet>(visualPath);
            if (visual == null)
            {
                visual = ScriptableObject.CreateInstance<MonsterVisualSet>();
                AssetDatabase.CreateAsset(visual, visualPath);
            }

            var counts = new List<string>();
            foreach (var (action, folder) in Actions)
            {
                var frames = LoadFrames(monsterDir + "/" + folder);
                if (frames.Count == 0)
                {
                    counts.Add(folder + "=0");
                    continue;
                }

                var fbPath = monsterDir + "/FB_" + name + "_" + action + ".asset";
                var fb = AssetDatabase.LoadAssetAtPath<SpriteFlipbook>(fbPath);
                if (fb == null)
                {
                    fb = ScriptableObject.CreateInstance<SpriteFlipbook>();
                    AssetDatabase.CreateAsset(fb, fbPath);
                }
                fb.Frames = frames.ToArray();
                fb.FramesPerSecond = 10f;
                EditorUtility.SetDirty(fb);

                Assign(visual, action, fb);
                counts.Add(folder + "=" + frames.Count);
            }

            visual.FacesLeft = facesLeft;
            EditorUtility.SetDirty(visual);
            return "  " + name + ": " + string.Join(" ", counts);
        }

        static List<Sprite> LoadFrames(string folder)
        {
            var result = new List<(int index, Sprite sprite)>();
            if (!AssetDatabase.IsValidFolder(folder)) return new List<Sprite>();

            var re = new Regex(@"frame_(\d+)\.png$");
            foreach (var guid in AssetDatabase.FindAssets("t:Sprite", new[] { folder }))
            {
                var path = AssetDatabase.GUIDToAssetPath(guid);
                var m = re.Match(path);
                if (!m.Success) continue;
                var sprite = AssetDatabase.LoadAssetAtPath<Sprite>(path);
                if (sprite != null) result.Add((int.Parse(m.Groups[1].Value), sprite));
            }
            // frame_10 이 frame_2 앞에 오지 않게 숫자로 정렬
            return result.OrderBy(r => r.index).Select(r => r.sprite).ToList();
        }

        static void Assign(MonsterVisualSet visual, MonsterAction action, SpriteFlipbook fb)
        {
            switch (action)
            {
                case MonsterAction.Idle: visual.Idle = fb; break;
                case MonsterAction.Attack: visual.Attack = fb; break;
                case MonsterAction.Hit: visual.Hit = fb; break;
                case MonsterAction.Defend: visual.Defend = fb; break;
                case MonsterAction.Special: visual.Special = fb; break;
                case MonsterAction.Death: visual.Death = fb; break;
            }
        }

        static void EnsureFolder(string path)
        {
            if (AssetDatabase.IsValidFolder(path)) return;
            var parent = Path.GetDirectoryName(path).Replace('\\', '/');
            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, Path.GetFileName(path));
        }
    }
}
