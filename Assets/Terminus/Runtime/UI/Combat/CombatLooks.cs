// 전투 HUD 공용: 스타일 시트 / 시안 색 / 상태이상 글자 칸 / 턴 띠 색 / 클라 쪽 전투 계산 (언리얼 CombatHUDWidget.cpp 의 익명 namespace)
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Combat
{
    /// <summary>시안 색 / 그림 (언리얼 CombatHUDWidget 익명 namespace + TerminusUIColors)</summary>
    static class CombatLooks
    {
        public const string SheetPath = "UI/Combat/CombatHud";
        static StyleSheet _sheet;
        static bool _sheetLoaded;

        /// <summary>CombatHud.uss 를 붙임 (Assets/Terminus/UI/Combat/Resources/UI/Combat/CombatHud.uss)</summary>
        public static void Use(VisualElement e)
        {
            if (!_sheetLoaded)
            {
                _sheetLoaded = true;
                _sheet = Resources.Load<StyleSheet>(SheetPath);
                if (_sheet == null) Debug.LogWarning("[UI] USS 없음: Resources/" + SheetPath);
            }
            if (_sheet != null && !e.styleSheets.Contains(_sheet)) e.styleSheets.Add(_sheet);
        }

        // ---- 그림 (/Game/UI/Textures/Combat/*)
        public static Texture2D Tex(string name) => UiTextures.LoadUnreal("/Game/UI/Textures/Combat/" + name);

        public static readonly string[] IntentTextureNames = { "T_Intent_Attack", "T_Intent_Defense", "T_Intent_Buff", "T_Intent_Debuff" };

        // ---- 색
        public static readonly Color MapText = UiColor.Hex("E8DFCB");       // TerminusUI::MapColor::Text
        public static readonly Color MapTextSub = UiColor.Hex("A89F8C");    // MapColor::TextSub
        public static readonly Color ShieldText = UiColor.Hex("A9C0D6");
        public static readonly Color ShieldFill = UiColor.Hex("8FA4B8");
        public static readonly Color ChipBack = UiColor.Hex("120F16");
        public static readonly Color ChipFill = UiColor.Hex("FF9A3A");
        public static readonly Color MonsterHealth = UiColor.Hex("E8323A");
        public static readonly Color PlayerHealth = UiColor.Hex("5FC45A");
        public static readonly Color StatusCell = UiColor.Hex("16131C");
        public static readonly Color EnergyTextColor = UiColor.FromLinear(0.332452f, 0.552011f, 1f);       // WBP EnergyText
        public static readonly Color SkillEnergyTextColor = UiColor.FromLinear(0.584078f, 0.351533f, 1f);  // WBP SkillEnergyText

        // 턴 띠 단계별 색 (글자 / 띠 바탕 / 띠 진하기 / 위아래 선)
        public enum PhaseStyle { Mine, Waiting, Enemy, Defeat, Betrayer, Victory }

        public struct PhaseLook
        {
            public Color Text;
            public Color Band;
            public Color Line;
        }

        static readonly PhaseLook[] PhaseLooks =
        {
            new PhaseLook { Text = UiColor.Hex("FFEDA0"), Band = UiColor.Hex("403212", 0.88f), Line = UiColor.Hex("FFD978") },   // 내 턴
            new PhaseLook { Text = UiColor.Hex("A89F8C"), Band = UiColor.Hex("1C1A22", 0.72f), Line = UiColor.Hex("5A5562") },   // 다른 플레이어를 기다리는 중
            new PhaseLook { Text = UiColor.Hex("FFC2B8"), Band = UiColor.Hex("4A1012", 0.88f), Line = UiColor.Hex("FF4A3A") },   // 적의 턴
            new PhaseLook { Text = UiColor.Hex("8C8796"), Band = UiColor.Hex("0E0D10", 0.88f), Line = UiColor.Hex("6A6470") },   // 패배
            new PhaseLook { Text = UiColor.Hex("FFD2CC"), Band = UiColor.Hex("5A0A0E", 0.92f), Line = UiColor.Hex("FF3A3A") },   // 배신자의 턴
            new PhaseLook { Text = UiColor.Hex("FFD978"), Band = UiColor.Hex("403212", 0.88f), Line = UiColor.Hex("FFD978") },   // 승리
        };

        public static PhaseLook GetPhaseLook(PhaseStyle style) => PhaseLooks[(int)style];

        // 상태이상 글자 칸 (전투 시안): 이로움 금 / 청, 해로움 적 / 보라
        public static void GetStatusLook(StatusEffect type, out string letter, out Color color)
        {
            string hex;
            switch (type)
            {
                case StatusEffect.Counter: letter = "반"; hex = "FFD978"; break;
                case StatusEffect.Mark: letter = "급"; hex = "FFD978"; break;
                case StatusEffect.Evasion: letter = "회"; hex = "7FB8E8"; break;
                case StatusEffect.Poison: letter = "중"; hex = "B07CD8"; break;
                case StatusEffect.Brave: letter = "용"; hex = "FFD978"; break;
                case StatusEffect.Protection: letter = "가"; hex = "7FB8E8"; break;
                case StatusEffect.Ice: letter = "얼"; hex = "B07CD8"; break;
                case StatusEffect.Lava: letter = "용"; hex = "FF5A4A"; break;
                case StatusEffect.Metal: letter = "강"; hex = "7FB8E8"; break;
                case StatusEffect.Acid: letter = "산"; hex = "FF5A4A"; break;
                case StatusEffect.Fear: letter = "공"; hex = "B07CD8"; break;
                case StatusEffect.Fusion: letter = "퓨"; hex = "FFD978"; break;
                case StatusEffect.Vitality: letter = "활"; hex = "FFD978"; break;
                case StatusEffect.Reflux: letter = "역"; hex = "FF5A4A"; break;
                case StatusEffect.InternalInjury: letter = "내"; hex = "FF5A4A"; break;
                case StatusEffect.Immortality: letter = "불"; hex = "7FB8E8"; break;
                case StatusEffect.Absorption: letter = "흡"; hex = "FFD978"; break;
                case StatusEffect.Solid: letter = "견"; hex = "7FB8E8"; break;
                case StatusEffect.Indomitable: letter = "불"; hex = "FFD978"; break;
                case StatusEffect.IronWall: letter = "철"; hex = "7FB8E8"; break;
                // 같은 그룹이라도 테마 이름 첫 글자로 (색은 그룹과 같게)
                case StatusEffect.Dark: letter = "어"; hex = "FF5A4A"; break;
                case StatusEffect.Flooding: letter = "침"; hex = "B07CD8"; break;
                case StatusEffect.Contrarywind: letter = "풍"; hex = "B07CD8"; break;
                case StatusEffect.Spirit: letter = "정"; hex = "FFD978"; break;
                case StatusEffect.Blindness: letter = "실"; hex = "FFD978"; break;
                default: letter = "?"; hex = "A89F8C"; break;
            }
            color = UiColor.Hex(hex);
        }

        /// <summary>"허약(얼음) 1 · 남은 2사이클\n수치만큼 공격 스텟이 하락한다."</summary>
        public static string StatusTip(StatusInstance status)
        {
            var tip = StatusInfo.GetDisplayName(status.Type);
            if (status.Value > 0) tip += " " + status.Value;
            if (status.Duration > 0) tip += " · 남은 " + status.Duration + "사이클";
            var desc = StatusInfo.GetDescription(status.Type);
            if (!string.IsNullOrEmpty(desc)) tip += "\n" + desc;
            return tip;
        }

        /// <summary>요소 네 변 테두리</summary>
        public static void Border(VisualElement e, float width, Color color) => UiStyle.Border(e, width, color);
    }

    /// <summary>
    /// 스냅샷(CharacterStats + CombatState)으로 클라 쪽 CombatStats 를 만들어 Core 계산을 그대로 씀
    /// (SkillExecutor.PreviewAmount / GetEffectiveStat, GetDisplayStatuses, GetEffectiveEnergyCost). 언리얼은 복제된 컴포넌트를 그대로 읽었음
    /// </summary>
    static class ClientCombat
    {
        public static CombatStats MakeStats(CharacterStats stats, CombatState state)
        {
            var s = new CombatStats { HasAuthority = false };
            s.ApplyReplicatedStats(stats ?? new CharacterStats());
            s.ApplyReplicatedState(state ?? new CombatState());
            return s;
        }

        public static bool IsPlayerTurn(AreaInfo area) => area != null && area.Phase == "PlayerTurn";

        /// <summary>이 사람이 지금 행동할 차례인가 (DungeonCombat.IsTurnOf: 플레이어 턴이고, 배신 전투면 그 사람 편 차례)</summary>
        public static bool IsTurnOf(AreaInfo area, ulong playerId)
        {
            if (!IsPlayerTurn(area)) return false;
            return !area.bBetrayal || ((playerId == area.BetrayerId) == area.bBetrayerTurn);
        }

        /// <summary>같은 편인가 (보통 전투면 플레이어끼리는 늘 같은 편)</summary>
        public static bool IsSameSide(AreaInfo area, ulong a, ulong b) =>
            area == null || !area.bBetrayal || ((a == area.BetrayerId) == (b == area.BetrayerId));

        public static bool HasEndedTurn(AreaInfo area, ulong playerId) => area != null && area.EndedTurn.Contains(playerId);

        public static int OccupantIndex(AreaInfo area, ulong playerId) => area != null ? area.Occupants.IndexOf(playerId) : -1;

        /// <summary>
        /// 배신 전투 결과 (DungeonCombat.DidBetrayerWin). 스냅샷에 없어서 체력으로 추정: 배신자가 살아 있으면 배신자 승리.
        /// TODO: AreaInfo 에 bBetrayerWon 이 생기면 그걸로
        /// </summary>
        // 서버가 결과를 보내 줌 (AreaInfo.bBetrayerWon = DungeonCombat.DidBetrayerWin)
        public static bool GuessBetrayerWon(AreaInfo area) => area.bBetrayerWon;

        /// <summary>
        /// 공격 예고 숫자. 턴 시작 때 정한 값 대신 지금 몬스터 상태로 다시 계산
        /// -> 허약 / 용기가 걸리거나 풀리면 바로 바뀜. 공포(공격자 -25%)도 반영 (ApplyDamage 와 같은 반올림)
        /// </summary>
        public static int LiveIntentAmount(IntentInfo intent, CombatStats monsterStats)
        {
            var skill = GameData.FindSkillRow(intent.SkillRow);
            if (skill == null || monsterStats == null) return intent.Amount;
            int amount = SkillExecutor.PreviewAmount(skill, monsterStats);
            if (monsterStats.HasStatus(StatusEffect.Fear)) amount = CombatMath.RoundToInt(amount * 0.75f);
            return Mathf.Max(0, amount);
        }

        public static string IntentNumber(IntentInfo intent, CombatStats monsterStats)
        {
            if (intent.Kind != "Attack") return "";
            int amount = LiveIntentAmount(intent, monsterStats);
            return intent.HitCount > 1 ? amount + "x" + intent.HitCount : amount.ToString();
        }

        public static int IntentIconIndex(IntentInfo intent)
        {
            switch (intent.Kind)
            {
                case "Attack": return 0;
                case "Defend": return 1;
                case "Buff": return 2;
                default: return 3;
            }
        }

        /// <summary>
        /// 한 대 예상 피해 (언리얼 PartyComms::EstimateHit). 공포 / 급소 / 불굴 / 철벽.
        /// 공격자 유물 '보호막 대상 피해 +%' 는 서버 값이라 클라에선 0 -> 예상치가 조금 작게 나올 수 있음 (언리얼과 같음)
        /// </summary>
        public static int EstimateHit(int amount, CombatStats caster, CombatStats target)
        {
            if (amount <= 0) return 0;
            if (caster != null && caster.HasStatus(StatusEffect.Fear)) amount = CombatMath.RoundToInt(amount * 0.75f);
            if (target != null && target.HasStatus(StatusEffect.Mark)) amount = CombatMath.RoundToInt(amount * 1.25f);
            if (caster != null && target != null && target.State.Shield > 0 && caster.GetShieldBonusPercent() > 0)
            {
                amount = CombatMath.RoundToInt(amount * (100 + caster.GetShieldBonusPercent()) / 100f);
            }
            if (target != null && amount > 0 && target.HasStatus(StatusEffect.Indomitable)) amount = 1;
            if (target != null) amount = Mathf.Max(0, amount - target.GetStatusValue(StatusEffect.IronWall));
            return amount;
        }
    }

    /// <summary>
    /// 받은 행동 계획 (언리얼 UPartyCommsWidget::Plans). 알림 처리기는 한 번만 붙임 (씬마다 HUD 가 새로 생겨도 쌓이지 않게)
    /// 표시는 CombatHud 가 (언리얼은 UPartyCommsWidget 이 머리 위에 그렸음)
    /// </summary>
    static class CombatPlanBoard
    {
        static readonly Dictionary<ulong, CombatPlanMsg> Plans = new Dictionary<ulong, CombatPlanMsg>();
        static bool _hooked;

        public static IEnumerable<CombatPlanMsg> All => Plans.Values;

        public static void Hook()
        {
            if (_hooked) return;
            _hooked = true;
            NetBus.On<CombatPlanMsg>(plan =>
            {
                if (plan == null) return;
                if (plan.IsValid()) Plans[plan.PlannerId] = plan;
                else Plans.Remove(plan.PlannerId);
            });
        }

        public static void Clear() => Plans.Clear();

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics()
        {
            _hooked = false;
            Plans.Clear();
        }
    }
}
