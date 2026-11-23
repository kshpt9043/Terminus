// 던전 테마의 무대 데이터 (언리얼 Dungeon/DungeonThemeData + Dungeon/DungeonAreaSet)
// 테마 자체(이름 / 아이디)는 Map 의 DungeonTheme. 여기는 그 테마의 방들이 어떤 무대(구역 세트)에서 펼쳐지는지
// 언리얼 ADungeonAreaSet 은 배경 / 바닥 / 소품 스프라이트를 카메라에 맞추는 연출 전용 액터 (복제 안 함)
//  -> Core 엔 무대 이름만. 실제 프리팹 / 스프라이트 배치는 Game 쪽이 이 Id 로 찾아 각 머신에서 띄움
using System;
using System.Collections.Generic;
using Terminus.Map;

namespace Terminus.Dungeon
{
    /// <summary>무대(구역 세트) 하나. 언리얼 TSubclassOf&lt;ADungeonAreaSet&gt; 자리 (Game 쪽 프리팹 키)</summary>
    [Serializable]
    public sealed class DungeonAreaSet
    {
        // Game 쪽이 프리팹을 찾는 키. 비어 있으면 '비워 둔 칸' (언리얼 None) -> 고를 때 건너뜀
        public string Id = "";

        public DungeonAreaSet() { }

        public DungeonAreaSet(string id)
        {
            Id = id ?? "";
        }

        public bool IsValid => !string.IsNullOrEmpty(Id);

        public DungeonAreaSet Clone() => (DungeonAreaSet)MemberwiseClone();

        public override string ToString() => Id;
    }

    // TMap 값에 배열을 바로 못 넣어서 감싸는 구조체 (언리얼 FDungeonAreaSetList)
    [Serializable]
    public sealed class DungeonAreaSetList
    {
        public List<DungeonAreaSet> Sets = new List<DungeonAreaSet>();

        public DungeonAreaSetList Clone() => new DungeonAreaSetList { Sets = Sets.ConvertAll(s => s?.Clone()) };
    }

    /// <summary>
    /// 던전 테마 하나의 무대 (기획서의 테마. 예: 슬라임 왕국).
    /// 테마 추가 = 이 데이터 하나 + 구역 세트 프리팹 몇 개. 코드 수정 없음
    /// </summary>
    [Serializable]
    public sealed class DungeonThemeData
    {
        // 지도 / 몬스터 / 이벤트가 보는 테마 (이름은 몬스터 DT 의 Theme_KR 과 같은 글자)
        public DungeonTheme Theme = new DungeonTheme();

        // 이 테마의 기본 무대. 여러 개 넣으면 방마다 랜덤 -> 같은 테마라도 방마다 조금씩 다르게 보임
        public List<DungeonAreaSet> AreaSets = new List<DungeonAreaSet>();

        // 방 타입별로 다른 무대를 쓰고 싶을 때만 (예: 상점, 휴식터). 없는 타입은 AreaSets 에서 고름
        public Dictionary<RoomType, DungeonAreaSetList> RoomTypeAreaSets = new Dictionary<RoomType, DungeonAreaSetList>();

        public DungeonThemeData() { }

        public DungeonThemeData(DungeonTheme theme)
        {
            Theme = theme ?? new DungeonTheme();
        }

        public string Id => Theme != null ? Theme.Id : "";
        public string DisplayName => Theme != null ? Theme.DisplayName : "";

        /// <summary>이 방 타입에 쓸 무대 하나. 후보가 없으면 null</summary>
        public DungeonAreaSet PickAreaSet(RoomType roomType, Random rng)
        {
            if (RoomTypeAreaSets != null && RoomTypeAreaSets.TryGetValue(roomType, out var typeSets) && typeSets != null)
            {
                var picked = PickValid(typeSets.Sets, rng);
                if (picked != null) return picked;
            }

            return PickValid(AreaSets, rng);
        }

        static DungeonAreaSet PickValid(List<DungeonAreaSet> candidates, Random rng)
        {
            // 비워 둔 칸(None)은 건너뜀
            var valid = new List<DungeonAreaSet>();
            if (candidates != null)
            {
                foreach (var set in candidates)
                {
                    if (set != null && set.IsValid) valid.Add(set);
                }
            }

            if (valid.Count == 0) return null;
            return valid[rng != null ? rng.Next(0, valid.Count) : 0];
        }

        public DungeonThemeData Clone()
        {
            var c = new DungeonThemeData
            {
                Theme = Theme,   // 테마는 에셋 참조였으니 같은 객체
                AreaSets = AreaSets.ConvertAll(s => s?.Clone()),
                RoomTypeAreaSets = new Dictionary<RoomType, DungeonAreaSetList>(),
            };
            foreach (var pair in RoomTypeAreaSets) c.RoomTypeAreaSets[pair.Key] = pair.Value?.Clone();
            return c;
        }
    }
}
