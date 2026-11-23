// 던전 안의 플레이어 한 명 (언리얼 ATerminusPlayerState + 빙의한 ATerminusBattler 의 CombatStats 를 묶은 것)
// 던전 전투(Dungeon/Combat)와 구역 / 방(Dungeon/Areas)이 같이 쓰는 공용 모델. 네트워크 쪽은 Id 로 연결 (NGO ClientId)
using Terminus.Combat;
using Terminus.Data;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public sealed class DungeonPlayer
    {
        public DungeonPlayer(ulong id, RunPlayer run, CombatStats battler)
        {
            Id = id;
            Run = run;
            Battler = battler;
            if (run != null && battler != null) run.Battler = new CombatStatsRunBattler(battler);
        }

        // 언리얼 APlayerState::GetPlayerId 자리. 게임 쪽이 정함 (NGO ClientId)
        public ulong Id { get; }

        // 런 규칙 / 상태 (언리얼 ATerminusPlayerState 의 순수 부분)
        public RunPlayer Run { get; }

        // 전투 수치 (언리얼 ATerminusBattler 의 UCombatStatsComponent)
        public CombatStats Battler { get; }

        public string Name => Run != null ? Run.PlayerName : "";

        public RunState RunState => Run?.RunState;

        // 접속 중인가 (나간 사람은 false. 언리얼 GameMode 의 Departed 목록)
        public bool bConnected = true;

        public bool IsDead() => Battler == null || Battler.IsDead();

        public override string ToString() => $"{Name}#{Id}";
    }

    /// <summary>RunPlayer 가 전투 밖에서 배틀러 수치를 바꿀 때 (유물 획득 / 이벤트) CombatStats 로 연결</summary>
    public sealed class CombatStatsRunBattler : IRunBattler
    {
        readonly CombatStats _stats;

        public CombatStatsRunBattler(CombatStats stats)
        {
            _stats = stats;
        }

        public void ModifyMaxHealth(int delta) => _stats.ModifyMaxHealth(delta);

        public void Heal(int amount) => _stats.Heal(amount);

        public void ModifyAttackDefense(int attackDelta, int defenseDelta)
        {
            // 언리얼: InitFrom(NewStats) 후 Revive(Health / MaxHealth)
            var s = _stats.Stats.Clone();
            float ratio = s.MaxHealth > 0 ? (float)_stats.State.Health / s.MaxHealth : 1f;
            s.Attack += attackDelta;
            s.Defense += defenseDelta;
            _stats.InitFrom(s);
            _stats.Revive(ratio);
        }

        public void ReviveFull() => _stats.Revive(1f);
    }
}
