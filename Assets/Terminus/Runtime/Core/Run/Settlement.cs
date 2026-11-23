// 정산 규칙 (언리얼 PlayerController::Client_BeginSettlement 의 정산 만들기 + Widgets/Settlement/SettlementWidget 의 골드 계산 이식)
// 화면 / 팝업 / 세션 나가기는 유니티 쪽. 여기는 숫자와 목록만
using System;
using System.Collections.Generic;
using System.Linq;
using Terminus.Data;

namespace Terminus.Run
{
    // 정산 완료 결과 (언리얼 HandleConfirm 이 요약 글에 쓰던 값)
    public sealed class SettlementResult
    {
        public int RelicGold = 0;                            // 안 고른 유물 판매
        public int BonusGold = 0;                            // 퀘스트 골드 / 배신 분배
        public List<string> NewRelics = new List<string>();  // 창고에 새로 들어간 유물 (이미 있던 건 빠짐)
        public string NewSkill = "";                         // 새 보유 스킬 (없으면 "")
        public int GoldAfter = 0;                            // 정산 뒤 보유 골드
    }

    public static class SettlementRules
    {
        /// <summary>
        /// 런 결과로 정산 대기를 만듦 (언리얼 Client_BeginSettlement). 각자 자기 컴퓨터에서 부름 -> 그 다음 profile.BeginSettlement.
        /// 정산 내용은 바로 내 프로필에 저장 (방장이 먼저 나가 끊겨도 메인 화면에서 이어서 정산)
        /// </summary>
        public static PendingSettlement MakeFromRun(ProfileService profile, RunState runState, SettlementAdjust adjust,
            string message, string roomName, int floor, bool bDeath)
        {
            adjust = adjust ?? new SettlementAdjust();

            // 배신 결과: 잃은 보스 보상은 빼고, 빼앗은 보스 유물은 더함
            var runRelics = new List<string>(runState.Relics);
            var runSkills = new List<string>(runState.EnhanceSkills);
            foreach (var row in adjust.LostRelics) runRelics.RemoveAll(r => r == row);
            foreach (var row in adjust.LostSkills) runSkills.RemoveAll(r => r == row);
            foreach (var row in adjust.ExtraRelics)
            {
                if (!runRelics.Contains(row)) runRelics.Add(row);
            }

            var settlement = profile.MakeSettlement(runRelics, runState.StartRelics, runSkills, message, roomName, floor, bDeath);
            settlement.BonusGold = adjust.BonusGold;
            settlement.BonusReason = adjust.BonusReason ?? "";

            // 퀘스트 골드 보상 (대상 Gold). 깬 퀘스트 보상이라 전멸해도 줌
            if (runState.QuestGold > 0)
            {
                settlement.BonusGold += runState.QuestGold;
                settlement.BonusReason = string.IsNullOrEmpty(settlement.BonusReason)
                    ? "퀘스트 보상"
                    : settlement.BonusReason + " · 퀘스트 보상";
            }

            // 하드 모드에서 죽으면 들고 간 창고 유물이 창고(도감)에서 사라짐 (사용자 결정 10-08)
            if (bDeath && runState.bHardMode)
            {
                settlement.LostStoredRelics = new List<string>(runState.StartRelics);
            }
            return settlement;
        }

        // 안 고른 유물을 팔아서 받는 골드 (하나도 안 골라도 됨 = 전부 판매)
        public static int SaleGold(PendingSettlement settlement, ICollection<string> kept)
        {
            long total = 0;
            for (int i = 0; i < settlement.Relics.Count; ++i)
            {
                if (kept.Contains(settlement.Relics[i]) || i >= settlement.RelicGold.Count) continue;
                total += Math.Max(0, settlement.RelicGold[i]);
            }
            return (int)Math.Min(total, int.MaxValue);
        }

        // 고른 것 중 창고에 이미 있는 유물 -> 보관해도 아무 일도 없음, 파는 게 이득 (화면 안내용)
        public static List<string> AlreadyStored(ProfileService profile, IEnumerable<string> kept)
        {
            var output = new List<string>();
            foreach (var row in kept)
            {
                if (profile.StoredRelics.Contains(row)) output.Add(row);
            }
            return output;
        }

        // 누를 때마다 보관 <-> 판매 (언리얼 HandleRelicClicked)
        public static void ToggleKept(List<string> kept, string relicRow)
        {
            if (!kept.Remove(relicRow)) kept.Add(relicRow);
        }

        /// <summary>
        /// 정산 완료 (언리얼 HandleConfirm): 고른 유물 보관, 나머지 판매 골드 + 보너스 골드를 프로필에. 정산 대기가 없으면 null
        /// </summary>
        public static SettlementResult Confirm(ProfileService profile, IList<string> kept)
        {
            if (!profile.HasPendingSettlement) return null;

            var settlement = profile.GetPendingSettlement();
            var result = new SettlementResult();
            foreach (var row in kept)
            {
                if (!profile.StoredRelics.Contains(row)) result.NewRelics.Add(row);
            }
            bool newSkill = !Ids.IsNone(settlement.KeptSkill) && !profile.OwnedSkills.Contains(settlement.KeptSkill);
            result.NewSkill = newSkill ? settlement.KeptSkill : "";
            result.RelicGold = SaleGold(settlement, kept);
            result.BonusGold = Math.Max(0, settlement.BonusGold);   // 퀘스트 골드 / 배신 분배

            profile.FinishSettlement(kept, result.RelicGold + result.BonusGold);
            result.GoldAfter = profile.Gold;
            return result;
        }
    }
}
