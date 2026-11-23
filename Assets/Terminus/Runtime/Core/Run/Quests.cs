// 퀘스트 규칙 (언리얼 Game/TerminusQuests 이식. 10-24 기획 엑셀 DT_Quest / DT_QuestRoom + 사용자 결정 2026-10-26)
//  - 테마가 시작되는 층(1 / 3 / 5층)의 퀘스트 방에서 3개 중 1개 수락 (1개는 내 직업 세력 것), 거절 가능
//  - 멀티는 사람마다 따로. 같은 방에서 잡은 몬스터는 그 방 사람 모두에게 셈
//  - 그 테마(2개 층) 동안만 유효. 다음 테마로 넘어가면 못 깬 퀘스트는 사라짐
//  - 보상 GiveCurrency: 대상 DungeonGold = 던전 재화 즉시, Gold = 정산 때 골드 (데이터가 둘을 나눠 적음)
//  - 특수 방(bInjectSpecialRoom): 지도의 Depth 줄 방 하나가 특수 방으로 바뀌고, 들어가면 SpawnMonster 가 나옴
//
// 상태는 플레이어 RunState(ActiveQuest / 층 방 입장 수 / QuestGold)에 있어서 세이브와 복제가 같이 됨. 판정은 서버만
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Run
{
    public static class Quests
    {
        static QuestRow ActiveRow(RunState s)
        {
            var q = s.ActiveQuest;
            return q == null || Ids.IsNone(q.Quest) || q.bCompleted ? null : GameData.FindQuestRow(q.Quest);
        }

        static QuestRow ActiveRow(RunPlayer p) => p != null ? ActiveRow(p.RunState) : null;

        static string FactionOf(RunPlayer p)
        {
            var row = GameData.FindCharacterClassRow(p.CharacterClass);
            return row != null ? CharacterNames.GetFactionName(row.Faction) : "";
        }

        // "2층" -> 2, "심층" -> 0. 1 / 2 는 테마 안의 층, 3 이상은 그 층 그대로 (몬스터 AppearFloor 와 같은 읽는 법)
        static bool MatchesFloor(int n, int floor)
        {
            int themeFloor = FloorRules.IsThemeEndFloor(floor) ? 2 : 1;
            return n > 0 && (n == floor || n == themeFloor);
        }

        static void GiveReward(RunPlayer p, QuestRewardType type, string target, int value, List<string> outLines)
        {
            switch (type)
            {
                case QuestRewardType.GiveCurrency:
                    if (target == "Gold")
                    {
                        p.ModifyRunState(s => s.QuestGold += value);
                        outLines.Add($"골드 {value} (정산 때)");
                    }
                    else
                    {
                        p.AddCurrency(value);
                        outLines.Add($"던전 재화 +{value}");
                    }
                    break;

                case QuestRewardType.GiveRelic:
                    p.MarkRelicsSeen(new[] { target });
                    outLines.Add(p.GainRelic(target)
                        ? $"유물 [{GameData.GetRelicName(target)}]"
                        : $"유물 [{GameData.GetRelicName(target)}] 은(는) 유물 칸이 가득 차 받지 못했습니다");
                    break;

                case QuestRewardType.GiveSkill:
                    outLines.Add(p.EquipEnhanceSkill(target)
                        ? $"스킬 [{GameData.GetSkillName(target)}]"
                        : $"스킬 [{GameData.GetSkillName(target)}] 은(는) 강화 칸이 가득 차 받지 못했습니다");
                    break;

                case QuestRewardType.UpgradeSkill:
                    for (int i = 0; i < Math.Max(1, value); ++i)
                    {
                        var candidates = p.UpgradableEnhanceSkills();
                        if (candidates.Count == 0)
                        {
                            outLines.Add("강화할 스킬이 없습니다");
                            break;
                        }
                        var skill = candidates[p.Rng.Next(0, candidates.Count)];
                        p.UpgradePickupSkill(skill);
                        outLines.Add($"{GameData.GetSkillName(skill)} +{p.GetPickupSkillLevel(skill)}");
                    }
                    break;
            }
        }

        // 진행도를 올리고(또는 정하고) 다 채우면 완료
        static void SetProgress(RunPlayer p, int newProgress)
        {
            var quest = ActiveRow(p);
            if (quest == null) return;

            int goal = Math.Max(1, quest.ConditionValue);
            int clamped = Math.Max(0, Math.Min(newProgress, goal));
            if (clamped == p.RunState.ActiveQuest.Progress) return;

            bool done = clamped >= goal;
            p.ModifyRunState(s =>
            {
                s.ActiveQuest.Progress = clamped;
                s.ActiveQuest.bCompleted = done;
            });

            if (!done)
            {
                p.Tell($"[퀘스트] {quest.QuestName} {clamped} / {goal}");
                return;
            }

            var lines = new List<string>();
            GiveReward(p, quest.ClearRewardType, quest.ClearRewardTargetID, quest.ClearRewardValue, lines);
            p.Tell($"[퀘스트 완료] {quest.QuestName} — {string.Join(", ", lines)}");
        }

        static void AddProgress(RunPlayer p, int delta)
        {
            if (delta > 0) SetProgress(p, p.RunState.ActiveQuest.Progress + delta);
        }

        // 언리얼 FMath::Clamp 그대로 (min > max 일 때도 같은 값이 나오게)
        static int UeClamp(int x, int min, int max) => x < min ? min : (x < max ? x : max);

        static string RoomTypeKey(RoomType type)
        {
            switch (type)
            {
                case RoomType.MONSTER: return "MonsterRoom";
                case RoomType.GUARDIAN: return "GuardianRoom";
                case RoomType.BOSS: return "BossRoom";
                case RoomType.EVENT: return "EventRoom";
                case RoomType.STORE: return "StoreRoom";
                case RoomType.BREAK: return "RestRoom";
                default: return "";
            }
        }

        // ============================================================ 후보 / 설명

        /// <summary>
        /// 퀘스트 방 후보: 지금 테마 이름 또는 계층 이름(표층 / 중층 / 심층) 퀘스트 중 가중치로 count 개, 1개는 내 세력
        /// </summary>
        public static List<string> MakeOffers(RunPlayer p, int floor, string themeName, Random rng, int count = 3)
        {
            var output = new List<string>();
            if (p == null || rng == null) return output;

            string tierName = FloorRules.GetTierName(floor);
            string myFaction = FactionOf(p);

            var mine = new List<KeyValuePair<string, int>>();
            var others = new List<KeyValuePair<string, int>>();
            foreach (var quest in GameData.Quests.Rows)
            {
                if (quest.AppearanceWeight <= 0) continue;
                if (quest.Theme != themeName && quest.Theme != tierName && quest.Theme != "Any") continue;
                (quest.Force == myFaction ? mine : others).Add(new KeyValuePair<string, int>(quest.RowName, quest.AppearanceWeight));
            }

            string PickWeighted(List<KeyValuePair<string, int>> pool)
            {
                int total = 0;
                foreach (var e in pool) total += e.Value;
                if (total <= 0) return "";
                int roll = rng.Next(1, total + 1);
                for (int i = 0; i < pool.Count; ++i)
                {
                    roll -= pool[i].Value;
                    if (roll <= 0)
                    {
                        var picked = pool[i].Key;
                        pool.RemoveAt(i);
                        return picked;
                    }
                }
                return "";
            }

            // 1개는 내 세력 것 (있으면), 나머지는 남은 것 전부에서
            var own = PickWeighted(mine);
            if (!Ids.IsNone(own)) output.Add(own);
            others.AddRange(mine);
            while (output.Count < count)
            {
                var next = PickWeighted(others);
                if (Ids.IsNone(next)) break;
                output.Add(next);
            }

            // 내 세력 것이 항상 첫 칸이면 티가 나서 섞음
            for (int i = output.Count - 1; i > 0; --i)
            {
                int j = rng.Next(0, i + 1);
                var t = output[i];
                output[i] = output[j];
                output[j] = t;
            }
            return output;
        }

        // "던전 재화 200", "골드 400 (정산 때)", "유물 [슬라임 킹의 왕관]", "픽업 스킬 2회 강화"
        public static string DescribeReward(QuestRewardType type, string target, int value)
        {
            switch (type)
            {
                case QuestRewardType.GiveCurrency:
                    return target == "Gold" ? $"골드 {value} (정산 때)" : $"던전 재화 {value}";
                case QuestRewardType.GiveRelic: return $"유물 [{GameData.GetRelicName(target)}]";
                case QuestRewardType.GiveSkill: return $"스킬 [{GameData.GetSkillName(target)}]";
                case QuestRewardType.UpgradeSkill: return $"픽업 스킬 {Math.Max(1, value)}회 강화";
                default: return "";
            }
        }

        // 카드 설명: 설명 + 세력 + 보상 (+ 수락 보상)
        public static string DescribeOffer(string questRow)
        {
            var quest = GameData.FindQuestRow(questRow);
            if (quest == null) return "";

            var text = quest.QuestDesc;
            text += $"\n\n{quest.Force} · 보상: {DescribeReward(quest.ClearRewardType, quest.ClearRewardTargetID, quest.ClearRewardValue)}";
            if (quest.AcceptRewardType != QuestRewardType.None)
            {
                text += $"\n수락하면 바로: {DescribeReward(quest.AcceptRewardType, quest.AcceptRewardTargetID, quest.AcceptRewardValue)}";
            }
            return text;
        }

        // 지도 화면 한 줄: "[퀘스트] 슬라임 학살자 0 / 1". 퀘스트가 없으면 빈 문자열
        public static string DescribeActive(RunState runState)
        {
            var q = runState?.ActiveQuest;
            var quest = q == null || Ids.IsNone(q.Quest) ? null : GameData.FindQuestRow(q.Quest);
            if (quest == null) return "";

            return q.bCompleted
                ? $"[퀘스트] {quest.QuestName} (완료)"
                : $"[퀘스트] {quest.QuestName} {q.Progress} / {Math.Max(1, quest.ConditionValue)}";
        }

        // ============================================================ 수락 / 만료

        /// <summary>
        /// 수락 (수락 보상, 지금 층이면 특수 방 넣기, 이미 이룬 조건이면 바로 완료).
        /// currentFloor: 지금 층 (언리얼은 레벨의 MapManager 에서 읽음. 0 이하 = 지도 없음 -> 층 도착 판정 건너뜀).
        /// injectSpecialRooms: 특수 방을 넣는 퀘스트면 부름 -> 유니티 쪽이 Quests.InjectSpecialRooms 로 지금 지도에 넣고 퍼뜨림
        /// </summary>
        public static void Accept(RunPlayer p, string questRow, int currentFloor = 0, Action injectSpecialRooms = null)
        {
            var quest = p != null ? GameData.FindQuestRow(questRow) : null;
            if (quest == null) return;

            p.ModifyRunState(s => s.ActiveQuest = new QuestProgress { Quest = questRow });

            var lines = new List<string>();
            GiveReward(p, quest.AcceptRewardType, quest.AcceptRewardTargetID, quest.AcceptRewardValue, lines);
            p.Tell($"[퀘스트 수락] {quest.QuestName}" + (lines.Count > 0 ? $" — 선지급: {string.Join(", ", lines)}" : ""));

            if (quest.bInjectSpecialRoom)
            {
                injectSpecialRooms?.Invoke();
            }

            // 수락할 때 이미 이룬 조건 (지금 층이 목표 층 이상)
            if (quest.ConditionType == QuestConditionType.ReachFloor && currentFloor > 0)
            {
                OnFloorReached(p, currentFloor);
            }
        }

        // 다음 테마로 넘어감: 못 깬 퀘스트는 사라짐
        public static void ExpireForNewTheme(RunPlayer p)
        {
            if (p == null) return;

            var q = p.RunState.ActiveQuest;
            if (q == null || Ids.IsNone(q.Quest)) return;

            if (!q.bCompleted)
            {
                var quest = GameData.FindQuestRow(q.Quest);
                if (quest != null)
                {
                    p.Tell($"[퀘스트 실패] {quest.QuestName} — 테마가 바뀌어 사라졌습니다.");
                }
            }
            p.ModifyRunState(s => s.ActiveQuest = new QuestProgress());
        }

        // ============================================================ 진행 사건 (서버)

        public static void OnMonstersKilled(RunPlayer p, IEnumerable<string> monsterRows)
        {
            var quest = ActiveRow(p);
            if (quest == null || quest.ConditionType != QuestConditionType.KillMonster) return;

            // 대상: MonsterID (M_SLI_B02) 또는 분류 (Guardian / Boss / Normal)
            var target = quest.ConditionTargetID;
            int count = 0;
            foreach (var row in monsterRows)
            {
                var monster = GameData.FindMonsterRow(row);
                if (monster == null) continue;
                if (monster.MonsterID == target || monster.MonsterCategory.ToString() == target || row == target) ++count;
            }
            AddProgress(p, count);
        }

        public static void OnRoomEntered(RunPlayer p, RoomType type)
        {
            if (p == null) return;

            // 한 층 안에서 방에 들어간 수 (BeginFloor 에서 0 으로)
            p.ModifyRunState(s =>
            {
                if (type == RoomType.MONSTER) ++s.FloorMonsterRooms;
                if (type == RoomType.GUARDIAN) ++s.FloorGuardianRooms;
            });

            var quest = ActiveRow(p);
            if (quest == null || quest.ConditionType != QuestConditionType.EnterRoomInSingleFloor) return;
            if (quest.ConditionTargetID != RoomTypeKey(type)) return;

            var st = p.RunState;
            SetProgress(p, type == RoomType.GUARDIAN ? st.FloorGuardianRooms : st.FloorMonsterRooms);
        }

        public static void OnRelicGained(RunPlayer p)
        {
            var quest = ActiveRow(p);
            if (quest != null && quest.ConditionType == QuestConditionType.AcquireRelic) AddProgress(p, 1);
        }

        public static void OnCurrencyEarned(RunPlayer p, int amount)
        {
            var quest = ActiveRow(p);
            if (quest != null && quest.ConditionType == QuestConditionType.EarnCurrency) AddProgress(p, amount);
        }

        public static void OnFloorReached(RunPlayer p, int floor)
        {
            var quest = ActiveRow(p);
            if (quest == null || quest.ConditionType != QuestConditionType.ReachFloor) return;

            // 언리얼 FCString::Atoi: 숫자가 아니면 0
            int.TryParse(quest.ConditionTargetID, out int target);
            if (target > 0 && floor >= target) SetProgress(p, quest.ConditionValue);
        }

        public static void OnSpecialRoomCleared(RunPlayer p, string specialRoom)
        {
            var quest = ActiveRow(p);
            if (quest == null || quest.ConditionType != QuestConditionType.CompleteSpecialRoom) return;
            if (quest.ConditionTargetID == specialRoom) AddProgress(p, 1);
        }

        // ============================================================ 특수 방

        /// <summary>
        /// 진행 중 퀘스트들의 특수 방을 이 층 지도(rooms)에 넣음 (수락했을 때 / 새 층 지도를 만든 뒤). 바뀐 게 있으면 true.
        /// 언리얼은 레벨의 MapManager.Rooms 를 직접 고치고 RunSubsystem.SetRooms + OnMapGenerated 를 불렀음
        /// -> 여기선 받은 목록만 고침. true 면 부른 쪽이 RunSession.SetRooms 하고 지도를 다시 퍼뜨릴 것
        /// </summary>
        public static bool InjectSpecialRooms(List<RoomNode> rooms, int floor, string themeName, IEnumerable<RunState> players, Random rng)
        {
            if (rooms == null || players == null || rng == null) return false;
            themeName = themeName ?? "";

            int lastRow = 0;
            foreach (var node in rooms) lastRow = Math.Max(lastRow, node.Row);
            int firstRow = FloorRules.HasQuestRoom(floor) ? 1 : 0;

            bool changed = false;
            foreach (var st in players)
            {
                var quest = st != null ? ActiveRow(st) : null;
                if (quest == null || !quest.bInjectSpecialRoom || Ids.IsNone(quest.SpecialRoomID)) continue;

                var special = GameData.FindQuestRoomRow(quest.SpecialRoomID);
                if (special == null || special.Theme != themeName || !MatchesFloor(special.Floor, floor)) continue;
                if (rooms.Exists(n => n.SpecialRoom == quest.SpecialRoomID)) continue;

                // Depth = 12칸 지도 기준 레벨 (1 = 퀘스트 줄, 12 = 보스). 퀘스트 줄이 없는 층은 한 칸 당겨짐
                int row = special.Depth >= 12 ? lastRow
                    : UeClamp(special.Depth - (FloorRules.HasQuestRoom(floor) ? 1 : 2), firstRow, lastRow);

                var candidates = new List<int>();
                for (int i = 0; i < rooms.Count; ++i)
                {
                    var n = rooms[i];
                    if (n.Row == row && n.Type != RoomType.QUEST && Ids.IsNone(n.SpecialRoom)) candidates.Add(i);
                }
                if (candidates.Count == 0) continue;

                var picked = rooms[candidates[rng.Next(0, candidates.Count)]];
                picked.SpecialRoom = quest.SpecialRoomID;
                if (picked.Type != RoomType.BOSS) picked.Type = RoomType.GUARDIAN;   // 특수 방 = 지정 몬스터와 싸우는 방
                changed = true;

                RunLog.Log($"[Quest] 특수 방 {quest.SpecialRoomID} -> {floor}층 {row}줄 {picked.RoomId}번 방");
            }
            return changed;
        }

        // 특수 방에서 나올 몬스터 (DT_Monster 행 이름, SpawnValue 마리)
        public static List<string> GetSpecialRoomMonsters(string specialRoom)
        {
            var output = new List<string>();
            var special = GameData.FindQuestRoomRow(specialRoom);
            if (special == null || Ids.IsNone(special.SpawnMonster)) return output;

            // SpawnMonster 는 MonsterID (M_SLI_G03) -> 행 이름으로
            string row = "";
            foreach (var monster in GameData.Monsters.Rows)
            {
                if (monster.MonsterID == special.SpawnMonster || monster.RowName == special.SpawnMonster)
                {
                    row = monster.RowName;
                    break;
                }
            }
            for (int i = 0; i < Math.Max(1, special.SpawnValue) && !Ids.IsNone(row); ++i) output.Add(row);
            return output;
        }
    }
}
