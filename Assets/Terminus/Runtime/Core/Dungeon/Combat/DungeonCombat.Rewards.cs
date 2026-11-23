// 던전 전투: 몬스터 / 가디언 / 보스방 클리어 보상 (언리얼 DungeonCombatComponent.cpp 의 '몬스터방 보상')
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;
using Terminus.Run;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonCombat
    {
        // 보상 받을 사람 하나 (언리얼 FPendingReward. TMap<약한 PS, ...> 대신 순서 있는 목록)
        sealed class PendingReward
        {
            public DungeonPlayer Player;
            public List<string> Offers = new List<string>();        // 보여 준 스킬 후보
            public List<string> RelicOffers = new List<string>();   // 보여 준 유물 후보
            public bool bDone = false;
        }

        readonly List<PendingReward> _pendingRewards = new List<PendingReward>();

        PendingReward FindPendingReward(DungeonPlayer p) => p == null ? null : _pendingRewards.Find(r => r.Player == p);

        // 이 사람이 보상 화면을 보고 있는가 (아직 '다음으로' 를 안 누름)
        public bool IsWaitingForReward(DungeonPlayer p)
        {
            var reward = FindPendingReward(p);
            return reward != null && !reward.bDone;
        }

        void StartRoomRewards()
        {
            _pendingRewards.Clear();

            // 방 종류별 보상 (사용자 결정 2026-10-06)
            //  몬스터: 재화 + 픽업 스킬 후보 중 하나 / 가디언: 유물 하나 / 보스: 유물 하나 + 픽업 스킬 하나
            bool bMonster = _currentRoomType == RoomType.MONSTER;
            bool bGuardian = _currentRoomType == RoomType.GUARDIAN;
            bool bBoss = _currentRoomType == RoomType.BOSS;
            var currencyRange = bBoss ? _settings.BossRewardCurrency : (bGuardian ? _settings.GuardianRewardCurrency : _settings.MonsterRewardCurrency);
            int skillCount = bMonster ? _settings.RewardSkillChoices : (bBoss ? _settings.BossRewardSkillChoices : 0);
            int relicCount = (bGuardian || bBoss) ? 1 : 0;

            foreach (var p in _players)
            {
                // 언리얼: PS 나 PC 가 없으면(나간 사람) 건너뜀
                if (!IsValid(p) || p.Run == null) continue;

                var offer = new RoomRewardOffer { RoomType = _currentRoomType };

                // 던전 재화는 바로 지급
                offer.Currency = RandRange(Math.Min(currencyRange.X, currencyRange.Y), Math.Max(currencyRange.X, currencyRange.Y));
                if (offer.Currency > 0)
                {
                    p.Run.AddCurrency(offer.Currency);
                }

                if (skillCount > 0) offer.SkillOffers = PickRewardSkills(p, skillCount);
                if (relicCount > 0)
                {
                    offer.RelicOffers = PickRewardRelics(p, relicCount);
                    p.Run.MarkRelicsSeen(offer.RelicOffers);   // 안 골라도 이번 런엔 다시 안 나옴
                }

                _pendingRewards.Add(new PendingReward
                {
                    Player = p,
                    Offers = new List<string>(offer.SkillOffers),
                    RelicOffers = new List<string>(offer.RelicOffers),
                });

                LogInfo?.Invoke($"[Reward] {_currentRoomId}번 방({_currentRoomType}) {p.Name}: 재화 {offer.Currency}, 스킬 후보 {offer.SkillOffers.Count}개, 유물 후보 {offer.RelicOffers.Count}개");
                RoomRewardOffered?.Invoke(p, offer);
            }

            FinishRewardsIfAllDone();   // 받을 사람이 없으면 바로 끝
        }

        // 지금 계층 등급 (1~2층 표층, 3~4층 중층, 5~6층 심층)
        static RelicTier RelicTierOfFloor(int floor) => floor >= 5 ? RelicTier.Deep : (floor >= 3 ? RelicTier.Mid : RelicTier.Surface);

        static SkillTier SkillTierOfFloor(int floor) => floor >= 5 ? SkillTier.Deep : (floor >= 3 ? SkillTier.Mid : SkillTier.Surface);

        /// <summary>
        /// 유물 후보: 공용이거나 내 직업, 지금 계층 등급(표층 / 중층 / 심층), 이번 런에 안 나온 것 중 Count 개 (휴식터 탐색도 씀)
        /// </summary>
        public List<string> PickRewardRelics(DungeonPlayer p, int count)
        {
            var pool = new List<string>();
            if (p == null || p.Run == null || count <= 0) return pool;

            var tier = RelicTierOfFloor(CurrentFloor);

            foreach (var relic in GameData.Relics.Rows)
            {
                if (relic.RelicTier != tier) continue;

                // 공용이거나 내 직업 것 (몬스터 유물 / 다른 직업 X), 이번 런에 이미 나왔던 건 빼고 (가진 것 포함)
                if (!RunPlayer.CanClassHoldRelic(p.Run.CharacterClass, relic)) continue;
                if (Contains(p.Run.Relics, relic.RowName) || p.Run.HasSeenRelic(relic.RowName)) continue;

                pool.Add(relic.RowName);
            }

            Shuffle(pool);
            DungeonCombatListUtil.Truncate(pool, Math.Min(pool.Count, count));
            return pool;
        }

        // 보상 스킬 후보 (언리얼 PickRewardSkills)
        List<string> PickRewardSkills(DungeonPlayer p, int count)
        {
            var pool = new List<string>();
            if (p == null || p.Run == null) return pool;

            // 지금 층의 티어 (테마 하나 = 2개 층: 1~2 표층, 3~4 중층, 5~6 심층)
            var maxTier = SkillTierOfFloor(CurrentFloor);

            var equipped = p.Run.RunState.EnhanceSkills;

            foreach (var skill in GameData.Skills.Rows)
            {
                // 기획: 공용 스킬 + 내 직업 전용 스킬 (기본 / 몬스터 스킬은 안 나옴)
                if (!ProfileService.IsEquippableSkill(skill.RowName, p.Run.CharacterClass)) continue;

                // 픽업 스킬 = 스킬 에너지를 쓰는 스킬 (사용자 결정)
                if (skill.SkillEnergyCost <= 0) continue;
                if (_settings.bRewardExcludeEventSkills && skill.SkillCategory == SkillCategory.Event) continue;
                if (_settings.bRewardExcludeHigherTier && skill.SkillTier > maxTier) continue;
                if (!_settings.bRewardIncludeEquippedSkills && equipped.Contains(skill.RowName)) continue;

                pool.Add(skill.RowName);
            }

            // 섞어서 앞에서부터
            Shuffle(pool);
            DungeonCombatListUtil.Truncate(pool, Math.Min(pool.Count, Math.Max(1, count)));
            return pool;
        }

        // 언리얼 그대로의 뒤에서부터 섞기 (Swap(i, RandRange(0, i)))
        void Shuffle(List<string> pool)
        {
            for (int i = pool.Count - 1; i > 0; --i)
            {
                int j = RandRange(0, i);
                var temp = pool[i];
                pool[i] = pool[j];
                pool[j] = temp;
            }
        }

        /// <summary>
        /// 몬스터방 보상을 마침 (플레이어가 '다음으로'). chosenSkill = 고른 보상 스킬 ("" = 안 고름), replaceSlot = 칸이 꽉 찼을 때 바꿀 칸
        /// 구역의 모든 플레이어가 마치면 구역 클리어 -> 지도
        /// 보상 마침 (PC 의 Server_FinishRoomReward). 보여 준 후보 중에서만 인정
        /// </summary>
        public void HandleRewardFinished(DungeonPlayer p, string chosenSkill, int replaceSlot, string chosenRelic, string replaceRelic)
        {
            var reward = FindPendingReward(p);
            if (reward == null || reward.bDone || p.Run == null) return;

            // 보여 준 후보 중 하나만 인정
            if (!Ids.IsNone(chosenSkill))
            {
                if (reward.Offers.Contains(chosenSkill))
                {
                    if (!p.Run.EquipEnhanceSkill(chosenSkill, replaceSlot))
                    {
                        LogWarning?.Invoke($"[Reward] {p.Name}: 강화 칸이 꽉 찼는데 바꿀 칸이 없어 '{chosenSkill}' 를 못 넣음");
                    }
                    else if (_currentRoomType == RoomType.BOSS)
                    {
                        p.Run.RecordBossReward(chosenSkill, "");   // 배신 결과용
                    }
                }
                else
                {
                    LogWarning?.Invoke($"[Reward] {p.Name}: 후보에 없던 스킬 '{chosenSkill}' 요청 무시");
                }
            }

            // 유물: 보여 준 후보만. 칸이 꽉 찼으면 고른 유물(직업 기본 유물 제외)을 버리고 받음
            if (!Ids.IsNone(chosenRelic))
            {
                if (reward.RelicOffers.Contains(chosenRelic))
                {
                    var discard = Ids.IsNone(replaceRelic) ? null : GameData.FindRelicRow(replaceRelic);
                    if (discard != null && discard.RelicTier != RelicTier.Basic && Contains(p.Run.Relics, replaceRelic)
                        && p.Run.Relics.Count >= p.Run.GetRelicCapacity())
                    {
                        p.Run.RemoveRelic(replaceRelic);
                    }

                    if (!p.Run.GainRelic(chosenRelic))
                    {
                        LogWarning?.Invoke($"[Reward] {p.Name}: 유물 '{chosenRelic}' 를 못 받음 (칸이 꽉 참 등)");
                    }
                    else if (_currentRoomType == RoomType.BOSS)
                    {
                        p.Run.RecordBossReward("", chosenRelic);   // 배신 결과용
                    }
                }
                else
                {
                    LogWarning?.Invoke($"[Reward] {p.Name}: 후보에 없던 유물 '{chosenRelic}' 요청 무시");
                }
            }

            reward.bDone = true;
            FinishRewardsIfAllDone();
        }

        void FinishRewardsIfAllDone()
        {
            foreach (var reward in _pendingRewards)
            {
                // 나간 사람은 기다리지 않음
                if (IsValid(reward.Player) && !reward.bDone) return;
            }

            _pendingRewards.Clear();
            SetTimer(CombatStep.ClearArea, _settings.RewardFinishDelay);
        }
    }
}
