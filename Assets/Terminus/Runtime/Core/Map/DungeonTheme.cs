// 던전 테마 (언리얼 Dungeon/DungeonThemeData 의 데이터 에셋 중 지도가 쓰는 부분만)
// 무대(구역 세트) 고르기(PickAreaSet)는 씬 / 프리팹을 다루니 Game 쪽에서. Core 는 테마가 무엇인지만 안다
using System.Collections.Generic;

namespace Terminus.Map
{
    /// <summary>
    /// 던전 테마 하나 (기획서의 테마. 예: 슬라임 왕국).
    /// 테마 추가 = 데이터 에셋(Game 쪽 ScriptableObject 등) 하나 + 구역 세트 몇 개. 코드 수정 없음
    /// </summary>
    [System.Serializable]
    public sealed class DungeonTheme
    {
        // 저장 / 복제용 식별자 (언리얼은 FSoftObjectPath 로 저장). Game 쪽이 이걸로 에셋을 찾음
        public string Id = "";

        // 테마 이름. 몬스터 DT 의 Theme_KR 과 같은 글자로 맞춰 둘 것 (나중에 몬스터 스폰이 이걸로 거름)
        // 퀘스트 특수 방(DT_QuestRoom.Theme)도 이 글자로 맞춤
        public string DisplayName = "";

        public DungeonTheme() { }

        public DungeonTheme(string id, string displayName)
        {
            Id = id ?? "";
            DisplayName = displayName ?? "";
        }

        public DungeonTheme Clone() => (DungeonTheme)MemberwiseClone();

        public override string ToString() => string.IsNullOrEmpty(DisplayName) ? Id : DisplayName;
    }

    // 계층 하나(표층 / 중층 / 심층)에 나올 수 있는 테마들. 계층이 바뀔 때 이 중 하나를 고름 (언리얼 FTierThemes)
    [System.Serializable]
    public sealed class TierThemes
    {
        public List<DungeonTheme> Themes = new List<DungeonTheme>();

        public TierThemes() { }

        public TierThemes(params DungeonTheme[] themes)
        {
            Themes.AddRange(themes);
        }

        // 테마는 에셋 참조였으니 얕은 복사 (같은 테마 객체를 가리킴)
        public TierThemes Clone() => new TierThemes { Themes = new List<DungeonTheme>(Themes) };
    }
}
