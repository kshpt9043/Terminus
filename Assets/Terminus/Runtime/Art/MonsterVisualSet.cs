// 몬스터 한 종의 동작 6개 (언리얼 FMonsterVisualRow 의 플립북 6칸). Resources/Visuals/Monsters/<PixelLab 이름>.asset
using UnityEngine;

namespace Terminus.Art
{
    public enum MonsterAction
    {
        Idle,     // 평소 (반복)
        Attack,   // 공격 의도
        Hit,      // 체력이 깎였을 때
        Defend,   // 방어 의도
        Special,  // 버프 / 디버프 의도
        Death,    // 끝나면 대기로 돌아가지 않고 사라짐
    }

    [CreateAssetMenu(menuName = "Terminus/Monster Visual Set", fileName = "MV_New")]
    public sealed class MonsterVisualSet : ScriptableObject
    {
        public const string ResourceFolder = "Visuals/Monsters/";

        public SpriteFlipbook Idle;
        public SpriteFlipbook Attack;
        public SpriteFlipbook Hit;
        public SpriteFlipbook Defend;
        public SpriteFlipbook Special;
        public SpriteFlipbook Death;

        // PixelLab 그림은 왼쪽을 봄 = 몬스터가 화면에서 보는 방향. 오른쪽을 봐야 하면 SpriteRenderer.flipX
        public bool FacesLeft = true;

        public SpriteFlipbook Get(MonsterAction action)
        {
            switch (action)
            {
                case MonsterAction.Idle: return Idle;
                case MonsterAction.Attack: return Attack;
                case MonsterAction.Hit: return Hit;
                case MonsterAction.Defend: return Defend;
                case MonsterAction.Special: return Special;
                case MonsterAction.Death: return Death;
                default: return null;
            }
        }

        /// <summary>
        /// DT_MonsterVisual 의 Source 열 ("PixelLab/slime_blue", "slime_big_blue (메모)") -> 에셋 이름 -> 로드. 없으면 null
        /// </summary>
        public static MonsterVisualSet LoadBySource(string source)
        {
            var name = SourceToName(source);
            return string.IsNullOrEmpty(name) ? null : Resources.Load<MonsterVisualSet>(ResourceFolder + name);
        }

        public static string SourceToName(string source)
        {
            if (string.IsNullOrWhiteSpace(source)) return "";
            var s = source.Trim();
            int space = s.IndexOf(' ');
            if (space >= 0) s = s.Substring(0, space);
            int slash = s.LastIndexOf('/');
            if (slash >= 0) s = s.Substring(slash + 1);
            return s;
        }
    }
}
