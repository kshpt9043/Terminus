// 체력 보호막 에너지를 들고 있는 전투용 상태 (언리얼 Combat/CombatStatsComponent 이식)
// 언리얼에선 UActorComponent + 복제였음 -> 여기선 규칙 / 계산만 하는 일반 클래스.
// 복제(Stats / State 의 Replicated, OnRep_State)는 Game 쪽 NetworkBehaviour 가 맡는다:
//  - 서버: 이 클래스의 동사(ApplyDamage 등)를 부르고, OnCombatStateChanged 를 받아 State 를 NetworkVariable 로 보냄
//  - 클라: 받은 값을 ApplyReplicatedState / ApplyReplicatedStats 로 넣음 (OnRep_State 와 같은 알림)
using System;
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Combat
{
    /// <summary>
    /// 플레이어랑 몬스터가 범용으로 쓰는 전투 상태.
    ///
    /// 피해 계산 순서 (ApplyDamage):
    ///  공격자 공포 -25% -> 내 급소 +25% -> 보호막 대상 추가 피해(공격자 유물 %) -> 공격자 실명 1 고정 -> 불굴 / 강철 1 고정 -> 철벽 수치만큼 감소
    ///  -> 보호막 -> 체술(회피만큼 감소) -> 체력
    ///  -> 불사(체력 1 아래로 안 내려감) -> 흡수(깎인 만큼 % 회복) -> 반격(공격자에게 수치만큼)
    /// </summary>
    public sealed class CombatStats
    {
        // 엔진 없는 Core 라 UE_LOG 대신. Game 쪽이 Debug.LogWarning 등을 꽂음 (안 꽂으면 조용히 버림)
        public static Action<string> LogWarning;

        // 로그에 찍을 주인 이름 (언리얼 GetNameSafe(GetOwner()))
        public string DebugName = "";

        // 언리얼 HasAuth() (주인 액터의 HasAuthority). 서버 = true. false 면 동사가 전부 아무것도 안 함 (언리얼과 같음)
        // TODO(Game): NetworkBehaviour 가 IsServer 로 맞춰 줄 것. 클라는 ApplyReplicatedState 로만 값을 받음
        public bool HasAuthority = true;

        // 클래스가 준 고정 스텟. 언리얼 const& 반환이었음 -> 밖에서 고치지 말 것 (최대 체력은 ModifyMaxHealth)
        public CharacterStats Stats { get; private set; } = new CharacterStats();

        // 전투 중에만 사는 현재값. 밖에서 고치지 말 것 (복제는 ApplyReplicatedState)
        public CombatState State { get; private set; } = new CombatState();

        // --- 알림 (언리얼 OnCombatStateChanged / OnCombatDied 는 BP 용 동적 델리게이트였음)
        public event Action OnCombatStateChanged;
        // 언리얼 OnCombatDied + OnDiedNative 를 하나로 (누가 죽었는지 같이 줌)
        public event Action<CombatStats> OnDied;

        // --- 서버 전용 사건 알림 (유물 발동용). 클라에선 안 불림
        public event Action<CombatStats, int, CombatStats> OnDamaged;   // (Self, HealthLost, Instigator) 체력이 실제로 깎였을 때 (보호막만 깎이면 안 불림)
        public event Action<CombatStats, int> OnShieldGained;           // 보호막이 늘었을 때
        public event Action<CombatStats, int> OnHitShieldTarget;        // 내가 보호막 있는 대상을 때렸을 때 (Amount = 그 한 대의 피해)
        public event Action<CombatStats, CombatStats> OnCountered;      // (반격한 쪽 = 나, 반격 맞은 쪽) 반격이 터졌을 때
        public event Action<CombatStats, int> OnHealed;                 // 체력이 실제로 회복됐을 때
        public event Action<CombatStats, int> OnEnergySpent;            // 에너지를 썼을 때
        public event Action<CombatStats, int> OnSkillEnergySpent;       // 스킬 에너지를 썼을 때

        // 체력이 0 이 되는 순간 물어봄. true 를 돌려주면 죽지 않은 걸로 (부활 유물 등. 돌려준 쪽이 체력을 채워야 함)
        // 언리얼 단일 바인딩 델리게이트(DECLARE_DELEGATE_RetVal) -> 하나만 꽂힘. 풀 땐 null
        public Func<CombatStats, bool> PreventDeath;

        bool _bEvasionReduction;
        bool _bOwnTurn;   // OnTurnStart ~ OnTurnEnd 사이 (이때 걸린 상태는 이번 턴 끝에 안 줄어듦)
        int _shieldBonusPercent;

        public void InitFrom(CharacterStats inStats)
        {
            if (!HasAuthority)
            {
                return;
            }

            Stats = inStats.Clone();

            State = new CombatState();
            State.Health = Stats.MaxHealth;
            State.Energy = Stats.MaxEnergy;

            // 연무장 '스킬 에너지' 강화만큼 갖고 시작
            State.SkillEnergy = CombatMath.Clamp(Stats.StartSkillEnergy, 0, Math.Max(0, Stats.MaxSkillEnergy));

            NotifyStateChanged();
        }

        // State 값 물어보는 함수라서 무조건 InitFrom 이후에 불려야함
        public bool IsDead() => State.Health <= 0;

        // ------------------------------------------------------------ 전투 함수

        /// <summary>
        /// Instigator = 때린 쪽 (공포 / 반격 / 타격 판정에 씀). 중독처럼 때린 쪽이 없으면 null.
        /// 돌려주는 값 = 실제로 깎인 체력 (흡혈 스킬이 "체력을 깎았으면" 을 볼 때)
        /// </summary>
        public int ApplyDamage(int amount, CombatStats instigator = null)
        {
            if (!HasAuthority)
            {
                return 0;
            }
            if (amount <= 0)
            {
                return 0;
            }
            if (IsDead())
            {
                return 0;
            }

            // ① 공격자가 공포면 최종 데미지 -25%, 내가 급소면 받는 데미지 +25%
            if (instigator != null && instigator.HasStatus(StatusEffect.Fear))
            {
                amount = CombatMath.RoundToInt(amount * 0.75f);
            }
            if (HasStatus(StatusEffect.Mark))
            {
                amount = CombatMath.RoundToInt(amount * 1.25f);
            }

            // 보호막 있는 대상을 때림: 공격자 유물(파쇄의 권갑 등) 만큼 +%, 그리고 OnHitShield 유물 알림
            bool bHadShield = State.Shield > 0;
            if (instigator != null && instigator != this && bHadShield && instigator._shieldBonusPercent > 0)
            {
                amount = CombatMath.RoundToInt(amount * (100 + instigator._shieldBonusPercent) / 100f);
            }

            // ② 공격자 실명: 한 번에 주는 데미지 1 고정. 불굴 그룹(불굴 / 강철 / 정령화): 한 번에 받는 데미지 1 고정. 철벽: 수치만큼 줄어듦
            if (amount > 0 && instigator != null && instigator != this && instigator.HasStatus(StatusEffect.Blindness))
            {
                amount = 1;
            }
            if (amount > 0 && HasStatus(StatusEffect.Indomitable))
            {
                amount = 1;
            }
            amount = Math.Max(0, amount - GetStatusValue(StatusEffect.IronWall));

            // ③ 보호막 먼저, 남은 만큼 체력
            //    체술: 보호막이 받고 남은 피해를 회피만큼 줄임. 회피 이하면 0 (노션 캐릭터 10-23)
            int absorbed = Math.Min(State.Shield, amount);
            State.Shield -= absorbed;
            int remain = amount - absorbed;
            if (_bEvasionReduction)
            {
                remain = Math.Max(0, remain - GetEvasion());
            }
            int healthLost = Math.Min(State.Health, remain);

            // ④ 불사: 체력이 1 아래로 내려가지 않음
            if (HasStatus(StatusEffect.Immortality) && State.Health - healthLost < 1)
            {
                healthLost = Math.Max(0, State.Health - 1);
            }

            State.Health -= healthLost;
            NotifyStateChanged();

            if (healthLost > 0)
            {
                // ⑤ 흡수: 깎인 체력의 수치% 만큼 회복 (죽는 한 대면 회복 안 함)
                int absorbPercent = GetStatusValue(StatusEffect.Absorption);
                if (absorbPercent > 0 && !IsDead())
                {
                    Heal(CombatMath.RoundToInt(healthLost * absorbPercent / 100f));
                }

                OnDamaged?.Invoke(this, healthLost, instigator);
            }

            // ⑥ 반격: 맞으면(보호막으로 막았어도) 때린 쪽에게 수치만큼 1 회. 반격끼리 끝없이 주고받지 않게 공격자는 비움
            int counterDamage = GetStatusValue(StatusEffect.Counter);
            if (instigator != null && instigator != this && counterDamage > 0 && amount > 0)
            {
                OnCountered?.Invoke(this, instigator);
                instigator.ApplyDamage(counterDamage, null);
            }

            // 위에서 이미 죽은 건 걸렀으니 여기서 죽어 있으면 방금 이 한 대로 죽은 것
            // 부활 유물 등이 막으면(PreventDeath 가 true) 안 죽은 걸로
            if (IsDead())
            {
                bool bPrevented = PreventDeath != null && PreventDeath(this) && !IsDead();
                if (!bPrevented)
                {
                    NotifyDied();
                }
            }

            if (instigator != null && instigator != this && bHadShield)
            {
                instigator.OnHitShieldTarget?.Invoke(instigator, amount);
            }
            return healthLost;
        }

        public void AddShield(int amount)
        {
            if (!HasAuthority)
            {
                return;
            }
            if (IsDead())
            {
                return;
            }
            if (amount <= 0)
            {
                return;
            }

            State.Shield += amount;
            NotifyStateChanged();
            OnShieldGained?.Invoke(this, amount);
        }

        public void Heal(int amount)
        {
            if (!HasAuthority)
            {
                return;
            }
            if (IsDead())
            {
                return;
            }
            if (amount <= 0)
            {
                return;
            }

            int before = State.Health;
            State.Health = Math.Min(State.Health + amount, Stats.MaxHealth);
            NotifyStateChanged();

            // 실제로 오른 만큼만 알림 (가득 차 있으면 회복이 아님)
            if (State.Health > before)
            {
                OnHealed?.Invoke(this, State.Health - before);
            }
        }

        public bool SpendEnergy(int cost)
        {
            if (!HasAuthority || cost < 0)
            {
                return false;
            }

            if (State.Energy < cost)
            {
                return false;
            }

            State.Energy -= cost;
            State.EnergySpentCounter += cost;

            int gained = State.EnergySpentCounter / 2;
            if (gained > 0)
            {
                State.EnergySpentCounter %= 2;
                State.SkillEnergy = Math.Min(State.SkillEnergy + gained, Stats.MaxSkillEnergy);
            }
            NotifyStateChanged();

            if (cost > 0)
            {
                // 역류: 에너지를 쓸 때마다 수치만큼 피해
                int reflux = GetStatusValue(StatusEffect.Reflux);
                if (reflux > 0)
                {
                    ApplyDamage(reflux);
                }
                OnEnergySpent?.Invoke(this, cost);
            }
            return true;
        }

        // 강화 스킬 비용. 모자라면 false 하고 아무것도 안 함
        public bool SpendSkillEnergy(int cost)
        {
            if (!HasAuthority || cost < 0 || State.SkillEnergy < cost)
            {
                return false;
            }

            if (cost > 0)
            {
                State.SkillEnergy -= cost;
                NotifyStateChanged();

                // 내상: 스킬 에너지를 쓸 때마다 수치만큼 피해
                int injury = GetStatusValue(StatusEffect.InternalInjury);
                if (injury > 0)
                {
                    ApplyDamage(injury);
                }
                OnSkillEnergySpent?.Invoke(this, cost);
            }
            return true;
        }

        // 즉시 에너지 회복 (신의 축복 같은 스킬). bAllowOverMax 면 최대치를 넘길 수 있음 (보조 배터리 유물: 3 -> 4)
        public void AddEnergy(int amount, bool bAllowOverMax = false)
        {
            if (!HasAuthority || amount <= 0)
            {
                return;
            }

            State.Energy = bAllowOverMax ? State.Energy + amount : Math.Min(State.Energy + amount, Math.Max(State.Energy, Stats.MaxEnergy));
            NotifyStateChanged();
        }

        public void RefillEnergy()
        {
            if (!HasAuthority)
            {
                return;
            }

            State.Energy = Stats.MaxEnergy;
            NotifyStateChanged();
        }

        // 턴 끝에 남은 에너지 버림 (기획: 에너지는 매 턴 종료 시 0)
        public void DrainEnergy()
        {
            if (!HasAuthority || State.Energy == 0)
            {
                return;
            }

            State.Energy = 0;
            NotifyStateChanged();
        }

        // 보호막 버림 (용어 설명: 보호막은 사이클 종료 시 사라진다). 언제 부를지는 전투 진행이 정함
        public void ClearShield()
        {
            if (!HasAuthority || State.Shield == 0)
            {
                return;
            }

            State.Shield = 0;
            NotifyStateChanged();
        }

        /// <summary>
        /// Duration -1 = 전투 끝까지 (사이클이 지나도 안 줄어듦)
        /// 같은 상태(그룹)를 또 걸면 덮어쓰지 않고 따로 쌓임 (11-03 피드백): 걸 때마다 항목 하나, 수치 / 남은 사이클은 항목마다 따로
        /// 효과는 살아 있는 항목 수치의 합. 예) 중독 2(2사이클) + 2(1사이클) + 1(3사이클) -> 5 / 3 / 1
        /// </summary>
        public void ApplyStatus(StatusEffect type, int value, int duration)
        {
            if (!HasAuthority)
            {
                return;
            }
            if (IsDead())
            {
                return;
            }
            // 반동 피해는 스킬의 SelfDamage 칸이 바로 처리함. 상태로 남길 게 없음
            if (type == StatusEffect.None || type == StatusEffect.SelfDamage)
            {
                return;
            }
            // 퓨전은 걸리는 순간 중독 용암 얼음 중 하나로 바뀌는 거라 저장할 상태가 아님
            // -> 스킬 실행 쪽이 셋 중 하나를 골라서 넘겨야 함. 여기 오면 호출부 버그
            if (type == StatusEffect.Fusion)
            {
                LogWarning?.Invoke("[Status] 퓨전이 그대로 들어옴. 스킬 쪽에서 중독/용암/얼음 중 하나로 바꿔서 넘길 것");
                return;
            }
            // 0 은 걸 게 없음. -1 은 전투 끝까지
            if (duration == 0)
            {
                return;
            }

            // 걸 때마다 뒤에 하나씩 쌓음 (큐). 수치 / 남은 사이클은 항목마다 따로 줄고, 효과는 합으로 계산
            // 화면은 GetDisplayStatuses 가 같은 그룹을 한 칸으로 합쳐 보여 줌
            var newStatus = new StatusInstance
            {
                Type = type,
                Value = value,
                Duration = duration,
                bSkipNextTick = _bOwnTurn,
            };
            State.Statuses.Add(newStatus);

            NotifyStateChanged();
        }

        public void RemoveStatus(StatusEffect type)
        {
            if (!HasAuthority)
            {
                return;
            }

            StatusEffect group = StatusInfo.GetGroup(type);
            if (State.Statuses.RemoveAll(s => StatusInfo.GetGroup(s.Type) == group) > 0)
            {
                NotifyStateChanged();
            }
        }

        // 전투가 끝날 때: 상태이상과 보호막을 모두 지움
        public void ClearCombatEffects()
        {
            if (!HasAuthority || (State.Statuses.Count == 0 && State.Shield == 0))
            {
                return;
            }

            State.Statuses.Clear();
            State.Shield = 0;
            _bOwnTurn = false;
            NotifyStateChanged();
        }

        // 최대 체력 변경 (유물). 올리면 현재 체력도 같이 오르고, 내리면 현재 체력이 최대치를 넘지 않게
        public void ModifyMaxHealth(int delta)
        {
            if (!HasAuthority || delta == 0)
            {
                return;
            }

            Stats.MaxHealth = Math.Max(1, Stats.MaxHealth + delta);
            if (!IsDead())
            {
                State.Health = CombatMath.Clamp(State.Health + Math.Max(0, delta), 1, Stats.MaxHealth);
            }
            NotifyStateChanged();
        }

        // 죽은 상태에서 체력을 되돌림 (부활 유물). Ratio = 최대 체력 대비
        public void Revive(float healthRatio)
        {
            if (!HasAuthority)
            {
                return;
            }

            State.Health = CombatMath.Clamp(CombatMath.RoundToInt(Stats.MaxHealth * healthRatio), 1, Stats.MaxHealth);
            NotifyStateChanged();
        }

        // ------------------------------------------------------------ 상태 조회

        // 같은 그룹 항목 수치의 합
        public int GetStatusValue(StatusEffect type)
        {
            // 같은 그룹 항목이 여러 개 쌓여 있으면 전부 더함
            StatusEffect group = StatusInfo.GetGroup(type);
            int total = 0;
            foreach (var s in State.Statuses)
            {
                if (StatusInfo.GetGroup(s.Type) == group) total += s.Value;
            }
            return total;
        }

        // 화면용: 같은 그룹을 한 칸으로 합침 (수치 합, 남은 사이클은 가장 긴 것, 이름은 마지막에 걸린 것). 복사본을 돌려줌
        public List<StatusInstance> GetDisplayStatuses()
        {
            var output = new List<StatusInstance>();
            foreach (var s in State.Statuses)
            {
                if (s.Type == StatusEffect.None) continue;
                StatusEffect group = StatusInfo.GetGroup(s.Type);
                StatusInstance merged = output.Find(m => StatusInfo.GetGroup(m.Type) == group);
                if (merged == null)
                {
                    output.Add(s.Clone());
                    continue;
                }
                merged.Type = s.Type;   // 나중에 걸린 테마 이름
                merged.Value += s.Value;
                merged.Duration = (merged.Duration < 0 || s.Duration < 0) ? -1 : Math.Max(merged.Duration, s.Duration);
            }
            return output;
        }

        // 수치가 0 인 상태(급소 / 공포 / 불굴)도 있어서 걸려 있는지는 따로 봄
        // 둘 다 그룹 기준: HasStatus(Ice) 는 허약(침수)도 참
        public bool HasStatus(StatusEffect type) => FindStatus(type) != null;

        // 과욕(산성): 에너지를 쓰는 스킬은 소모량 +1
        public int GetEffectiveEnergyCost(int baseCost)
        {
            // 과욕(산성): 에너지만 적용 -> 에너지를 쓰는 스킬만 +1
            return (baseCost > 0 && HasStatus(StatusEffect.Acid)) ? baseCost + 1 : baseCost;
        }

        // 체술 (암살자 기본 유물). 켜져 있으면 보호막이 받고 남은 피해를 회피(스텟 + 민첩)만큼 줄임
        // 데미지 계산은 서버에서만 하니까 복제 안 함. 유물 목록을 만들 때 전투 진행 쪽이 켜고 끔
        public void SetEvasionReduction(bool bEnable) => _bEvasionReduction = bEnable;

        public int GetEvasion()
        {
            // 기본 / 연무장 회피 + 민첩(은신 같은 스킬)
            return Math.Max(0, Stats.Evasion + GetStatusValue(StatusEffect.Evasion));
        }

        // 보호막 있는 대상을 때릴 때 피해 +% (무도가 기본 유물 BonusDamageOnShield). 체술처럼 전투 진행 쪽이 정함
        public void SetShieldBonusPercent(int percent) => _shieldBonusPercent = Math.Max(0, percent);
        public int GetShieldBonusPercent() => _shieldBonusPercent;

        // ------------------------------------------------------------ 턴 / 사이클
        // 상태이상은 각자 자기 턴 기준 (11-04 피드백). 1사이클 = 주인의 자기 턴 하나

        // 자기 턴 시작: 중독 피해
        public void OnTurnStart()
        {
            if (!HasAuthority)
            {
                return;
            }
            _bOwnTurn = true;

            // 중독: 자기 턴 시작 때 쌓인 수치 합만큼 (11-04 피드백: 예전엔 턴 끝)
            int poison = GetStatusValue(StatusEffect.Poison);
            if (poison > 0)
            {
                ApplyDamage(poison);
            }
            // ApplyDamage에 끝났음을 알리는 델리게이트 있어서 여기선 안부름
        }

        // 자기 턴 끝: 상태이상 남은 사이클 1 감소 (OnCycleEnd). 이번 자기 턴에 걸린 건 안 줄임
        public void OnTurnEnd()
        {
            if (!HasAuthority)
            {
                return;
            }
            _bOwnTurn = false;
            OnCycleEnd();
        }

        // 남은 사이클 1 감소 + 다 된 것 지움
        public void OnCycleEnd()
        {
            if (!HasAuthority)
            {
                return;
            }
            if (State.Statuses.Count == 0)
            {
                return;
            }

            // 지속 효과 전부 1씩 깎기. 음수(-1)는 전투 끝까지라 안 깎음
            foreach (var s in State.Statuses)
            {
                if (s.bSkipNextTick)
                {
                    s.bSkipNextTick = false;   // 이번 자기 턴에 걸린 것 -> 다음 자기 턴 끝부터 줄어듦
                    continue;
                }
                if (s.Duration > 0)
                {
                    s.Duration -= 1;
                }
            }

            // 뒤에서부터 검사해야 당겨지는 문제가 없음
            for (int i = State.Statuses.Count - 1; i >= 0; --i)
            {
                if (State.Statuses[i].Duration == 0)
                {
                    State.Statuses.RemoveAt(i);
                }
            }

            NotifyStateChanged();
        }

        // ------------------------------------------------------------ 복제 (Game 쪽이 부름)

        /// <summary>언리얼 Stats 복제 대응. 클라에서 서버 값을 그대로 넣음 (알림 없음, 언리얼도 OnRep 없었음)</summary>
        public void ApplyReplicatedStats(CharacterStats replicated)
        {
            Stats = replicated.Clone();
        }

        /// <summary>언리얼 OnRep_State 대응. 클라에서 서버 값을 넣고 알림</summary>
        public void ApplyReplicatedState(CombatState replicated)
        {
            CombatState oldState = State;
            State = replicated.Clone();
            NotifyStateChanged();

            // 클라는 ApplyDamage 가 안 돌아서 복제로 알아채야 함
            // 이전엔 살아 있었는데 지금 죽었으면 -> 죽은 순간
            if (oldState.Health > 0 && IsDead())
            {
                NotifyDied();
            }
        }

        // ------------------------------------------------------------ 내부

        void NotifyStateChanged()
        {
            OnCombatStateChanged?.Invoke();
        }

        void NotifyDied()
        {
            LogWarning?.Invoke($"[Died] {DebugName} ({(HasAuthority ? "서버" : "클라")})");

            OnDied?.Invoke(this);
        }

        StatusInstance FindStatus(StatusEffect type)
        {
            StatusEffect group = StatusInfo.GetGroup(type);
            return State.Statuses.Find(s => StatusInfo.GetGroup(s.Type) == group);
        }
    }

    /// <summary>언리얼 FMath 중 전투가 쓰는 것. 반올림 규칙이 C# Math.Round(은행가 반올림)와 달라서 따로 둠</summary>
    public static class CombatMath
    {
        // FMath::RoundToInt = FloorToInt(F + 0.5f). 0.5 는 항상 위로 (-2.5 -> -2)
        public static int RoundToInt(float f) => (int)Math.Floor(f + 0.5f);

        // FMath::Clamp. Math.Clamp 와 달리 Min > Max 여도 예외 안 냄
        public static int Clamp(int x, int min, int max) => x < min ? min : (x < max ? x : max);
    }
}
