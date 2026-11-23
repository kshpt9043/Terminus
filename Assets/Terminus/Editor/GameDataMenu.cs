// 에디터 메뉴: 기획 CSV 다시 읽기
using Terminus.Game;
using UnityEditor;

namespace Terminus.EditorTools
{
    public static class GameDataMenu
    {
        [MenuItem("Terminus/Reload Game Data")]
        public static void Reload()
        {
            AssetDatabase.Refresh();
            GameDataLoader.Load();
        }
    }
}
