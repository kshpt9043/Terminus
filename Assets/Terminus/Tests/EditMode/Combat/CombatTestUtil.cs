// 전투 테스트 공용 (언리얼 Tests/SkillExecutorTests.cpp 의 namespace TerminusTest)
using Terminus.Combat;
using Terminus.Data;

namespace Terminus.Tests
{
    public static class CombatTestUtil
    {
        // 언리얼은 월드 없이 액터 + 컴포넌트만 만들었음 (권한 있는 액터) -> 여기선 그냥 new
        public static CombatStats MakeBattler(int maxHealth = 100, int attack = 0, int defense = 0, int evasion = 0)
        {
            var stats = new CombatStats();
            var b = new CharacterStats
            {
                MaxHealth = maxHealth,
                Attack = attack,
                Defense = defense,
                Evasion = evasion,
                MaxEnergy = 3,
                MaxSkillEnergy = 4,
            };
            stats.InitFrom(b);
            return stats;
        }

        public static int Health(CombatStats s) => s.State.Health;
        public static int Shield(CombatStats s) => s.State.Shield;
    }
}
