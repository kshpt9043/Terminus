// 던전 전투: 몬스터 고르기 / 스폰 / 스킬 고르기 (언리얼 DungeonCombatComponent.cpp 의 '몬스터 스폰')
using System;
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Dungeon
{
    public sealed partial class DungeonCombat
    {
        /// <summary>방 타입 / 테마에 맞는 몬스터 행 고르기 (언리얼 PickMonsterRows)</summary>
        internal List<string> PickMonsterRows(RoomType roomType, DungeonTheme theme, int maxCount)
        {
            var output = new List<string>();
            if (maxCount <= 0 || GameData.Monsters.Count == 0) return output;

            var category =
                roomType == RoomType.GUARDIAN ? MonsterCategory.Guardian :
                roomType == RoomType.BOSS ? MonsterCategory.Boss :
                                                MonsterCategory.Normal;

            // 테마 이름 = 몬스터 Theme_KR (예: "슬라임 왕국")
            string themeName = theme != null ? theme.DisplayName ?? "" : "";

            // 같은 분류 + 같은 테마. 테마에 하나도 없으면 테마 무시
            List<string> PoolOf(MonsterCategory inCategory, bool bWarn)
            {
                var sameCategory = new List<string>();
                var sameCategoryAndTheme = new List<string>();
                foreach (var row in GameData.Monsters.Rows)
                {
                    if (row.MonsterCategory != inCategory) continue;

                    sameCategory.Add(row.RowName);
                    if (themeName.Length > 0 && row.Theme_KR == themeName)
                    {
                        sameCategoryAndTheme.Add(row.RowName);
                    }
                }

                if (sameCategoryAndTheme.Count == 0 && bWarn)
                {
                    LogWarning?.Invoke($"[Combat] 테마 '{themeName}' 의 {inCategory} 몬스터가 없어 테마 무시하고 고름 (테마 DisplayName 과 DT_Monster Theme_KR 을 맞출 것)");
                }
                return sameCategoryAndTheme.Count > 0 ? sameCategoryAndTheme : sameCategory;
            }

            var pool = PoolOf(category, true);
            if (pool.Count == 0) return output;

            // 보스: AppearFloor "1층" / "2층" = 테마의 첫째 / 둘째 층 (리자드처럼 "3층" / "4층" 으로 적힌 건 그 층 그대로)
            // 맞는 게 없으면 테마 보스 전체에서
            if (category == MonsterCategory.Boss)
            {
                int floor = CurrentFloor;
                int themeFloor = FloorRules.IsThemeEndFloor(floor) ? 2 : 1;

                var forFloor = pool.FindAll(row =>
                {
                    var monster = GameData.FindMonsterRow(row);
                    int n = monster != null ? DungeonMonster.ParseAppearFloor(monster.AppearFloor) : 0;   // "2층" -> 2, "심층" -> 0
                    return n > 0 && (n == themeFloor || n == floor);
                });
                if (forFloor.Count > 0) pool = forFloor;
            }

            // 가디언: 그 테마 가디언이 전부 한 번씩 나오기 전엔 중복으로 안 나옴 (기획 10-07). 다 나왔으면 다시 처음부터
            // 이번 런에 나온 가디언은 런 세션이 들고 있고 세이브에도 들어감
            if (category == MonsterCategory.Guardian)
            {
                var run = _host?.RunSession;
                if (run != null)
                {
                    var fresh = pool.FindAll(row => !Contains(run.SeenGuardians, row));
                    if (fresh.Count == 0)
                    {
                        run.ForgetSeenGuardians(pool);
                        fresh = pool;
                    }

                    string picked = fresh[RandRange(0, fresh.Count - 1)];
                    run.AddSeenGuardian(picked);
                    output.Add(picked);
                }
            }

            // 몬스터방 1~3 마리, 가디언 / 보스는 1 마리
            if (output.Count == 0)
            {
                int count = category == MonsterCategory.Normal
                    ? CombatMath.Clamp(RandRange(_settings.NormalMonsterCount.X, _settings.NormalMonsterCount.Y), 1, maxCount)
                    : 1;

                for (int i = 0; i < count; ++i)
                {
                    output.Add(pool[RandRange(0, pool.Count - 1)]);
                }
            }

            // 같이 나오는 몬스터 (슬라임 킹의 가디언, 족장의 일반 몬스터 등). 남은 슬롯만큼
            if (category != MonsterCategory.Normal
                && _settings.MonsterCompanions != null
                && _settings.MonsterCompanions.TryGetValue(output[0], out var rule)
                && rule != null)
            {
                string leader = output[0];
                var companionPool = Ids.IsNone(rule.FixedRow)
                    ? PoolOf(rule.Category, false).FindAll(row => row != leader)
                    : new List<string> { rule.FixedRow };

                for (int i = 0; i < rule.Count && output.Count < maxCount && companionPool.Count > 0; ++i)
                {
                    string companion = companionPool[RandRange(0, companionPool.Count - 1)];
                    if (GameData.FindMonsterRow(companion) == null) break;
                    output.Add(companion);
                }
                LogInfo?.Invoke($"[Combat] {leader} 와 함께 {output.Count - 1} 마리");
            }
            return output;
        }

        static bool Contains(IReadOnlyList<string> list, string value)
        {
            for (int i = 0; i < list.Count; ++i)
            {
                if (list[i] == value) return true;
            }
            return false;
        }

        void SpawnMonsters(RoomNode room, DungeonTheme theme)
        {
            var rows = _nextForcedMonsters.Count > 0
                ? new List<string>(_nextForcedMonsters)
                : PickMonsterRows(room.Type, theme, Math.Max(1, NumMonsterSlots));

            // 기획(10-07): 테마의 두 번째 층 몬스터 / 가디언(보스 제외)은 기본 체력 +30%, 공방 +1
            bool bSecondFloorOfTheme = _host != null && FloorRules.IsThemeEndFloor(_host.CurrentFloor);

            // 기획: 멀티 체력 보정은 그 방에 들어온 인원 기준
            int partySize = Math.Max(1, _players.Count);
            var bonusTable = _settings.PartyHealthBonus;
            float partyBonus = bonusTable != null && partySize - 1 < bonusTable.Count ? bonusTable[partySize - 1]
                : (bonusTable != null && bonusTable.Count > 0 ? bonusTable[bonusTable.Count - 1] : 0f);

            for (int i = 0; i < rows.Count; ++i)
            {
                var row = GameData.FindMonsterRow(rows[i]);
                if (row == null) continue;

                // 스텟: 층 보정 -> 인원 체력 보정 -> 개체 랜덤 (기획 순서)
                var stats = row.ToStats();
                bool bBoss = row.MonsterCategory == MonsterCategory.Boss;
                if (bSecondFloorOfTheme && !bBoss)
                {
                    stats.MaxHealth = Math.Max(1, CombatMath.RoundToInt(stats.MaxHealth * (1f + _settings.SecondFloorHealthBonus)));
                    stats.Attack += _settings.SecondFloorStatBonus;
                    stats.Defense += _settings.SecondFloorStatBonus;
                }

                // 멀티: 방에 들어온 플레이어 1명당 공방 +1 (보스 포함, 혼자여도 +1)
                stats.Attack += _settings.PartyStatBonusPerPlayer * partySize;
                stats.Defense += _settings.PartyStatBonusPerPlayer * partySize;

                // 체력 랜덤은 보스 포함 (사용자 결정 10-08), 공방 랜덤은 보스 제외
                float healthScale = 1f + partyBonus;
                healthScale *= 1f + FRandRange(_settings.MonsterHealthRandom.X, _settings.MonsterHealthRandom.Y);
                healthScale *= _nextSpawnHealthScale;   // 이벤트 '쉬는 가디언' 등
                if (!bBoss)
                {
                    stats.Attack += RandRange(0, _settings.MonsterStatRandomMax);
                    stats.Defense += RandRange(0, _settings.MonsterStatRandomMax);
                }
                stats.MaxHealth = Math.Max(1, CombatMath.RoundToInt(stats.MaxHealth * healthScale));

                var combatStats = new CombatStats { DebugName = rows[i] };
                combatStats.InitFrom(stats);

                var monster = new DungeonMonster(_nextMonsterId++, _monsters.Count, i, rows[i], row, combatStats);
                _monsters.Add(monster);

                LogInfo?.Invoke($"[Combat]   {rows[i]} 스폰: 체력 {stats.MaxHealth} 공격 {stats.Attack} 방어 {stats.Defense}");
                MonsterSpawned?.Invoke(monster);
            }
        }

        /// <summary>몬스터 행의 스킬 중 가중치 랜덤 (비어 있으면 FallbackMonsterSkills). 언리얼 PickMonsterSkill</summary>
        internal string PickMonsterSkill(string monsterRow)
        {
            var candidates = DungeonMonster.GetSkillSlots(GameData.FindMonsterRow(monsterRow));

            if (candidates.Count == 0)
            {
                // DT_Monster 의 스킬 칸이 비어 있음 -> 임시 스킬
                if (_warnedFallbackRows.Add(monsterRow ?? ""))
                {
                    LogWarning?.Invoke($"[Combat] {monsterRow}: DT_Monster 에 스킬이 비어 있어 임시 스킬(FallbackMonsterSkills)을 씀");
                }

                if (_settings.FallbackMonsterSkills != null)
                {
                    foreach (var pair in _settings.FallbackMonsterSkills)
                    {
                        if (pair != null && pair.Weight > 0) candidates.Add(pair);
                    }
                }
            }

            int total = 0;
            foreach (var c in candidates) total += c.Weight;
            if (total <= 0) return "";

            int roll = RandRange(1, total);
            foreach (var c in candidates)
            {
                roll -= c.Weight;
                if (roll <= 0) return c.Skill;
            }
            return candidates[candidates.Count - 1].Skill;
        }
    }
}
