// Resources/Data/DT_*.csv 를 읽어 Terminus.Data.GameData 를 채운다 (언리얼 UTerminusDataSettings 의 DT 로드 자리)
// 씬이 뜨기 전에 한 번 자동으로 불림. 에디터에서 CSV 를 고친 뒤엔 메뉴 Terminus/Reload Game Data 로 다시 읽음
using Terminus.Data;
using UnityEngine;

namespace Terminus.Game
{
    public static class GameDataLoader
    {
        public const string ResourceFolder = "Data/";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        public static void LoadOnStartup()
        {
            Load();
        }

        public static void Load()
        {
            GameData.Load(name =>
            {
                var asset = Resources.Load<TextAsset>(ResourceFolder + name);
                return asset != null ? asset.text : null;
            });

            int warnings = 0;
            foreach (var w in GameData.AllWarnings)
            {
                Debug.LogWarning("[GameData] " + w);
                ++warnings;
            }

            Debug.Log($"[GameData] 로드: 스킬 {GameData.Skills.Count}, 몬스터 {GameData.Monsters.Count}, 유물 {GameData.Relics.Count}, " +
                      $"이벤트 {GameData.Events.Count}/{GameData.EventChoices.Count}, 퀘스트 {GameData.Quests.Count}, 직업 {GameData.CharacterClasses.Count}" +
                      (warnings > 0 ? $" (경고 {warnings})" : ""));
        }
    }
}
