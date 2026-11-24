// 파티 소통 도구 (채팅 말고): 핑 / 퀵챗 / 재촉 + 행동 계획 예상 피해 (언리얼 Comms/PartyCommsTypes.h / .cpp)
// 행동 계획 공유 메시지(FCombatPlan)는 전투 쪽이 Game/Net/NetMessages.Combat.cs + DungeonServer.CombatPlan.cs 로 옮김
//
// 판정과 전달은 전부 서버. 클라는 요청만 보내고, 서버가 받을 사람을 골라 알림으로 돌림
//  - 계획 / 핑: 같은 구역 + 같은 편만 (배신 전투에서 상대 편에 새지 않게)
//  - 퀵챗: 채팅과 같이 모두에게
//  - 재촉: 아직 턴을 안 끝낸 같은 구역 사람에게만
//
// 언리얼은 대상을 AActor(배틀러) 포인터로 넘겼음 -> 여기선 CommsTarget (플레이어 번호 또는 몬스터 번호)
using System;
using Terminus.Combat;
using Terminus.Data;

namespace Terminus.Comms
{
    // 핑 종류. 적에게는 앞 둘, 아군(나 포함)에게는 뒤 둘
    public enum PingKind
    {
        Focus,      // 집중 공격
        Danger,     // 위험
        NeedGuard,  // 보호 필요
        NeedHeal,   // 회복 필요
    }

    // 퀵챗 문구. 순서 = 메뉴 번호 (1 부터). 문구는 PartyComms.GetQuickChatText
    public enum QuickChat
    {
        Good,
        Thanks,
        Wait,
        Sorry,
        Help,
        Go,
    }

    /// <summary>핑 대상 배틀러 (언리얼 AActor* 자리). 플레이어 번호 또는 몬스터 번호. 둘 다 아니면 없음</summary>
    [Serializable]
    public sealed class CommsTarget
    {
        public bool bPlayer = false;
        public ulong PlayerId = 0;
        public int MonsterId = -1;

        public static CommsTarget None => new CommsTarget();
        public static CommsTarget Player(ulong id) => new CommsTarget { bPlayer = true, PlayerId = id };
        public static CommsTarget Monster(int id) => new CommsTarget { MonsterId = id };

        public bool IsNone => !bPlayer && MonsterId < 0;
        public bool IsMonster => !bPlayer && MonsterId >= 0;

        public bool Same(CommsTarget other)
        {
            if (other == null) return IsNone;
            if (IsNone || other.IsNone) return IsNone && other.IsNone;
            return bPlayer == other.bPlayer && (bPlayer ? PlayerId == other.PlayerId : MonsterId == other.MonsterId);
        }

        public CommsTarget Clone() => (CommsTarget)MemberwiseClone();

        public override string ToString() => IsNone ? "None" : bPlayer ? "P" + PlayerId : "M" + MonsterId;
    }

    /// <summary>핑 한 개</summary>
    [Serializable]
    public sealed class PartyPing
    {
        public ulong Sender = 0;
        public PingKind Kind = PingKind.Focus;

        // 핑을 찍은 배틀러 (몬스터 / 플레이어)
        public CommsTarget Target = new CommsTarget();

        public PartyPing Clone()
        {
            var c = (PartyPing)MemberwiseClone();
            c.Target = Target != null ? Target.Clone() : new CommsTarget();
            return c;
        }
    }

    public static class PartyComms
    {
        public const int PingKindCount = 4;
        public const int QuickChatCount = 6;

        // 채팅창에 넣는 핑 안내 줄의 머리 (채팅창이 이걸 보고 [핑] 태그를 색으로 그림)
        public const string PingPrefix = "[핑] ";

        public static bool IsValidPingKind(PingKind kind) => (int)kind >= 0 && (int)kind < PingKindCount;
        public static bool IsValidQuickChat(QuickChat kind) => (int)kind >= 0 && (int)kind < QuickChatCount;

        public static string GetQuickChatText(QuickChat kind)
        {
            switch (kind)
            {
                case QuickChat.Good: return "좋아요!";
                case QuickChat.Thanks: return "고마워요!";
                case QuickChat.Wait: return "잠깐만요!";
                case QuickChat.Sorry: return "미안해요!";
                case QuickChat.Help: return "도와주세요!";
                case QuickChat.Go: return "가자!";
                default: return "";
            }
        }

        public static string GetPingText(PingKind kind)
        {
            switch (kind)
            {
                case PingKind.Focus: return "집중 공격";
                case PingKind.Danger: return "위험";
                case PingKind.NeedGuard: return "보호 필요";
                case PingKind.NeedHeal: return "회복 필요";
                default: return "";
            }
        }

        /// <summary>채팅 / 전투 시안 핑 색 (sRGB 16진수, # 없음)</summary>
        public static string GetPingColorHex(PingKind kind)
        {
            switch (kind)
            {
                case PingKind.Focus: return "FF5A4A";
                case PingKind.Danger: return "FFD84A";
                case PingKind.NeedGuard: return "4ED1C8";
                case PingKind.NeedHeal: return "6FD86A";
                default: return "FFFFFF";
            }
        }

        // 적에게 찍는 핑인가 (집중 공격 / 위험)
        public static bool IsEnemyPing(PingKind kind) => kind == PingKind.Focus || kind == PingKind.Danger;

        // 퀵챗 문구와 같은 말인가 (채팅창 말풍선 표시)
        public static bool IsQuickChatText(string text)
        {
            if (string.IsNullOrEmpty(text)) return false;
            for (int i = 0; i < QuickChatCount; ++i)
            {
                if (GetQuickChatText((QuickChat)i) == text) return true;
            }
            return false;
        }

        /// <summary>채팅창 핑 안내 줄 "[핑] 홍길동 -> 적 2: 집중 공격"</summary>
        public static string FormatPingLine(string senderName, string targetName, PingKind kind) =>
            $"{PingPrefix}{senderName} -> {targetName}: {GetPingText(kind)}";

        /// <summary>핑 안내 줄이면 핑 종류 (문구 끝으로 판단, 못 찾으면 Focus). 아니면 false</summary>
        public static bool FindPingKind(string text, out PingKind kind)
        {
            kind = PingKind.Focus;
            if (string.IsNullOrEmpty(text) || !text.StartsWith(PingPrefix, StringComparison.Ordinal)) return false;
            for (int i = 0; i < PingKindCount; ++i)
            {
                var pingText = GetPingText((PingKind)i);
                if (pingText.Length > 0 && text.EndsWith(pingText, StringComparison.Ordinal))
                {
                    kind = (PingKind)i;
                    return true;
                }
            }
            return true;
        }

        /// <summary>
        /// 한 대가 실제로 몇 들어갈지 어림. CombatStats.ApplyDamage 의 ① ② 단계와 같은 순서
        /// (공격자 공포 -25% -> 대상 급소 +25% -> 보호막 대상 유물 보너스 -> 불굴 / 강철 1 고정 -> 철벽 감소). 보호막 / 회피는 안 봄
        /// </summary>
        public static int EstimateHit(int amount, CombatStats caster, CombatStats target)
        {
            if (amount <= 0) return 0;

            if (caster != null && caster.HasStatus(StatusEffect.Fear))
            {
                amount = CombatMath.RoundToInt(amount * 0.75f);
            }
            if (target != null && target.HasStatus(StatusEffect.Mark))
            {
                amount = CombatMath.RoundToInt(amount * 1.25f);
            }
            // 공격자 유물: 보호막 있는 대상 피해 +% (서버 값이라 클라에선 0 -> 예상치가 조금 작게 나올 수 있음)
            if (caster != null && target != null && target.State.Shield > 0 && caster.GetShieldBonusPercent() > 0)
            {
                amount = CombatMath.RoundToInt(amount * (100 + caster.GetShieldBonusPercent()) / 100f);
            }
            // 공격자 실명: 주는 피해 1
            if (caster != null && amount > 0 && caster.HasStatus(StatusEffect.Blindness))
            {
                amount = 1;
            }
            // 불굴 그룹 (불굴 / 강철 / 정령화)
            if (target != null && amount > 0 && target.HasStatus(StatusEffect.Indomitable))
            {
                amount = 1;
            }
            if (target != null)
            {
                amount = Math.Max(0, amount - target.GetStatusValue(StatusEffect.IronWall));
            }
            return amount;
        }

        /// <summary>예상 피해 줄 "예상 피해 약 14 / 체력 15 (+3)  처치!"</summary>
        public static string DescribeEstimate(int estimatedDamage, int health, int shield, out bool bLethal)
        {
            bLethal = false;
            if (estimatedDamage <= 0) return "";
            bLethal = estimatedDamage >= health + shield;
            var label = shield > 0
                ? $"예상 피해 약 {estimatedDamage} / 체력 {health} (+{shield})"
                : $"예상 피해 약 {estimatedDamage} / 체력 {health}";
            if (bLethal) label += "  처치!";
            return label;
        }

        public static string NudgeToastText(string senderName) => $"{senderName} 님이 기다리고 있어요! 턴을 끝내 주세요";
        public static string NudgeChatText(string senderName) => $"{senderName} 님이 재촉합니다.";
    }
}
