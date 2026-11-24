// [서버] 던전 요청 처리 (언리얼 ATerminusPlayerController 의 Server_* 핸들러)
// 요청자는 늘 보낸 연결에서 꺼냄 -> 남의 플레이어로 행동할 수 없음
using System.Collections.Generic;
using System.Linq;
using Terminus.Dungeon;
using Terminus.Game.Net;
using Terminus.Map;

namespace Terminus.Game
{
    public sealed partial class DungeonServer
    {
        void RegisterRequests()
        {
            NetBus.OnRequest<ChooseStartSkillReq>((id, r) => With(id, p => p.Run.ChooseStartSkill(r.SkillRow)));
            NetBus.OnRequest<ChooseStartRelicsReq>((id, r) => With(id, p => p.Run.ChooseStartRelics(r.RelicRows ?? new List<string>())));

            NetBus.OnRequest<SelectRoomReq>(OnSelectRoom);
            NetBus.OnRequest<CastFloorVoteReq>(OnCastFloorVote);

            NetBus.OnRequest<UseSkillReq>((id, r) => WithCombat(id, (c, p) => c.HandleUseSkill(p, r.SkillIndex, r.TargetIndex)));
            NetBus.OnRequest<EndTurnReq>((id, r) => WithCombat(id, (c, p) => c.HandleEndTurn(p)));
            NetBus.OnRequest<FinishRoomRewardReq>((id, r) => WithCombat(id, (c, p) => c.HandleRewardFinished(p, r.ChosenSkill ?? "", r.ReplaceSlot, r.ChosenRelic ?? "", r.ReplaceRelic ?? "")));

            NetBus.OnRequest<RestTargetReq>((id, r) => With(id, p =>
            {
                if (_players.TryGetValue(r.TargetId, out var target)) _areas.HandleRestChoice(p, target);
            }));
            NetBus.OnRequest<RestExploreReq>((id, r) => With(id, p => _areas.HandleRestExplore(p)));
            NetBus.OnRequest<ChooseEventReq>((id, r) => With(id, p => _areas.HandleEventChoice(p, r.Index, r.ReplaceSlot, r.RelicRow ?? "")));
            NetBus.OnRequest<ShopBuyRelicReq>((id, r) => With(id, p => _areas.HandleShopBuyRelic(p, r.Index)));
            NetBus.OnRequest<ShopBuySkillReq>((id, r) => With(id, p => _areas.HandleShopBuySkill(p, r.Index, r.ReplaceSlot)));
            NetBus.OnRequest<ShopSellRelicReq>((id, r) => With(id, p => _areas.HandleShopSellRelic(p, r.RelicRow ?? "")));
            NetBus.OnRequest<ShopUpgradeSkillReq>((id, r) => With(id, p => _areas.HandleShopUpgradeSkill(p, r.SkillRow ?? "")));
            NetBus.OnRequest<ShopLeaveReq>((id, r) => With(id, p => _areas.HandleShopLeave(p)));
            NetBus.OnRequest<ChooseRescueReq>((id, r) => With(id, p => _areas.HandleRescueChoice(p, r.bIntervene)));

            NetBus.OnRequest<DebugWinCombatReq>((id, r) => WithCombat(id, (c, p) => c.DebugKillAllMonsters()));
            NetBus.OnRequest<DebugLoseCombatReq>((id, r) => WithCombat(id, (c, p) => c.DebugKillAllPlayers()));
            NetBus.OnRequest<DebugCycleSkillReq>((id, r) => WithCombat(id, (c, p) => c.DebugCycleSkill(p, r.Step)));
            RegisterCombatPlanRequests();
        }

        void With(ulong clientId, System.Action<DungeonPlayer> action)
        {
            if (_players.TryGetValue(clientId, out var p) && p.bConnected) action(p);
        }

        void WithCombat(ulong clientId, System.Action<DungeonCombat, DungeonPlayer> action)
        {
            With(clientId, p =>
            {
                if (_areas.GetCurrentArea(p)?.Combat is DungeonCombat combat) action(combat, p);
            });
        }

        // 언리얼 AMapManager::HandleSelectRoomRequest + CheckAllPlayersReadyAndStart
        void OnSelectRoom(ulong clientId, SelectRoomReq r)
        {
            if (!_players.TryGetValue(clientId, out var p) || !p.bConnected) return;

            var connected = _players.Values.Where(x => x.bConnected).ToList();
            bool hasDeparted = _rift.HasDepartedPlayers;
            var req = new RoomSelectRequest
            {
                RoomId = r.RoomId,
                RequesterRun = p.RunState,
                OtherSelectedRoomIds = connected.Where(x => x != p).Select(x => x.RunState.SelectedRoomId).ToList(),
                bFloorVoteActive = _floor.IsFloorVoteActive,
                bHasDepartedPlayers = hasDeparted,
                bAnyRoomInProgress = _areas.IsAnyRoomInProgress,
                bSinglePlayerRun = RoomSelectionRules.IsSinglePlayerRun(connected.Count, hasDeparted),
            };

            var result = RoomSelectionRules.EvaluateSelectRoom(_floor.Rooms, req);
            if (result.Outcome == RoomSelectOutcome.Rejected)
            {
                NetBus.Notify(clientId, new RoomSelectFailedMsg { Message = result.FailMessage });
                return;
            }
            if (result.ChangesSelection) p.Run.SetSelectedRoomId(result.NewSelectedRoomId);

            if (result.Outcome == RoomSelectOutcome.EnterNow)
            {
                _areas.EnterSelectedRooms(new[] { p }, _floor.Rooms, _floor.FloorTheme);
            }
            else if (result.Outcome == RoomSelectOutcome.Selected)
            {
                var departed = _rift.GetDepartedNames();
                var ready = RoomSelectionRules.CheckAllPlayersReady(connected.Select(x => x.RunState.SelectedRoomId).ToList(), departed, out var notice);
                if (ready == PartyReadyResult.Start) _areas.EnterSelectedRooms(connected, _floor.Rooms, _floor.FloorTheme);
                else if (ready == PartyReadyResult.WaitingForDeparted && !string.IsNullOrEmpty(notice)) _session.SystemChat(notice);
            }
        }

        void OnCastFloorVote(ulong clientId, CastFloorVoteReq r)
        {
            if (!_players.TryGetValue(clientId, out var p) || !p.bConnected) return;
            if (!_floor.CastFloorVote(clientId, r.Choice, Now))
            {
                NetBus.Notify(clientId, new FloorVoteRejectedMsg { Message = "고를 수 없는 선택지입니다." });
            }
        }
    }
}
