// 네트워크 메시지 (JsonUtility 로 보냄 -> [Serializable] + public 필드, Dictionary 금지)
// 이름은 언리얼 RPC 이름에서 Server_ / Client_ 를 뗀 것. Req = 클라 -> 서버, 나머지 = 서버 -> 클라
using System;
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Map;

namespace Terminus.Game.Net
{
    // ================================================================ 요청 (언리얼 Server_*)

    // 주점
    [Serializable] public sealed class SelectCharacterReq { public CharacterClass Class; }
    [Serializable] public sealed class SetReadyReq { public bool bReady; }
    [Serializable] public sealed class SetHardModeReq { public bool bHardMode; }
    [Serializable] public sealed class StartGameReq { }
    [Serializable] public sealed class SetNameReq { public string Name = ""; }
    // 거점 강화 (프로필은 각자 컴퓨터에 있음 -> 서버가 검증해서 씀)
    [Serializable] public sealed class ReportUpgradesReq { public List<ClassUpgrades> Upgrades = new List<ClassUpgrades>(); }

    // 던전 시작 고르기
    [Serializable] public sealed class ChooseStartSkillReq { public string SkillRow = ""; }
    [Serializable] public sealed class ChooseStartRelicsReq { public List<string> RelicRows = new List<string>(); }

    // 지도
    [Serializable] public sealed class SelectRoomReq { public int RoomId; }
    [Serializable] public sealed class CastFloorVoteReq { public FloorChoice Choice; }

    // 전투
    [Serializable] public sealed class UseSkillReq { public int SkillIndex; public int TargetIndex; }
    [Serializable] public sealed class EndTurnReq { }
    [Serializable] public sealed class FinishRoomRewardReq { public string ChosenSkill = ""; public int ReplaceSlot = -1; public string ChosenRelic = ""; public string ReplaceRelic = ""; }

    // 휴식 / 이벤트 / 상점 / 구출
    [Serializable] public sealed class RestTargetReq { public ulong TargetId; }
    [Serializable] public sealed class RestExploreReq { }
    [Serializable] public sealed class ChooseEventReq { public int Index; public int ReplaceSlot = -1; public string RelicRow = ""; }
    [Serializable] public sealed class ShopBuyRelicReq { public int Index; }
    [Serializable] public sealed class ShopBuySkillReq { public int Index; public int ReplaceSlot = -1; }
    [Serializable] public sealed class ShopSellRelicReq { public string RelicRow = ""; }
    [Serializable] public sealed class ShopUpgradeSkillReq { public string SkillRow = ""; }
    [Serializable] public sealed class ShopLeaveReq { }
    [Serializable] public sealed class ChooseRescueReq { public bool bIntervene; }

    // 채팅
    [Serializable] public sealed class SendChatReq { public string Text = ""; }

    // 테스트 (언리얼 Server_Debug*)
    [Serializable] public sealed class DebugWinCombatReq { }
    [Serializable] public sealed class DebugLoseCombatReq { }

    // ================================================================ 개인 알림 (언리얼 Client_*)

    [Serializable] public sealed class RoomSelectFailedMsg { public string Message = ""; }
    [Serializable] public sealed class FloorVoteRejectedMsg { public string Message = ""; }
    [Serializable] public sealed class FloorTitleMsg { public string Title = ""; public string Subtitle = ""; }
    [Serializable] public sealed class RoomRewardMsg { public RoomRewardOffer Offer = new RoomRewardOffer(); }
    [Serializable] public sealed class CloseRoomRewardMsg { }
    [Serializable] public sealed class ShowRestMsg { public List<ulong> Occupants = new List<ulong>(); public float HealRatio; public int ExploreMin; public int ExploreMax; public float RelicChance; }
    [Serializable] public sealed class RestResultMsg { public string Result = ""; }
    [Serializable] public sealed class CloseRestMsg { }
    [Serializable] public sealed class ShowShopMsg { public ShopState State = new ShopState(); }
    [Serializable] public sealed class CloseShopMsg { }
    [Serializable] public sealed class ShowEventMsg { public string Title = ""; public string Situation = ""; public List<EventOption> Options = new List<EventOption>(); }
    [Serializable] public sealed class EventResultMsg { public string Result = ""; }
    [Serializable] public sealed class CloseEventMsg { }
    [Serializable] public sealed class ShowRescueMsg { public bool bChooser; public List<string> WipedNames = new List<string>(); public bool bCanIntervene; public float HealthCost; public float Seconds; }
    [Serializable] public sealed class CloseRescueMsg { }
    [Serializable] public sealed class EnterRiftMsg { public List<string> WaitingFor = new List<string>(); public bool bFromSave; }
    [Serializable] public sealed class LeaveRiftMsg { }
    // 런 끝 -> 각자 정산 (정산 계산은 각자 프로필로)
    [Serializable] public sealed class RunEndedMsg { public string Message = ""; public bool bDeath; public SettlementAdjust Adjust = new SettlementAdjust(); public string RoomName = ""; public RunState Run = new RunState(); public int Floor; }

    // ================================================================ 전체 알림

    [Serializable] public sealed class ChatMsg { public string Sender = ""; public string Text = ""; public bool bSystem; }

    // 몬스터 연출 (언리얼 PlayIntentAnim). 피격 / 사망은 스냅샷 체력 변화로 화면이 알아서
    [Serializable] public sealed class MonsterActingMsg { public int AreaIndex; public int MonsterId; public string Kind = ""; }
    // 스킬 이펙트: 누가 쓰든 스킬 행 + 맞는 대상 (이펙트는 클라가 스킬 행으로 고름). Effect 가 있으면 그 이펙트 (반격 등 스킬 밖)
    [Serializable] public sealed class SkillUsedMsg { public int AreaIndex; public string SkillRow = ""; public string Effect = ""; public List<int> MonsterIds = new List<int>(); public List<ulong> PlayerIds = new List<ulong>(); }

    // ================================================================ 상태 스냅샷 (언리얼 복제 변수 + OnRep)

    [Serializable]
    public sealed class PlayerInfo
    {
        public ulong Id;
        public string Name = "";
        public CharacterClass Class;
        public bool bReady;
        public bool bHost;
        public bool bConnected = true;
        public int AreaIndex = -1;          // 들어가 있는 구역 (-1 = 지도)
        public RunState Run = new RunState();
        public bool bHasBattler;            // 던전 안 (전투 수치가 있음)
        public CharacterStats Stats = new CharacterStats();
        public CombatState Combat = new CombatState();
    }

    [Serializable]
    public sealed class PartySnapshot
    {
        public string Scene = "";
        public bool bHardMode;
        public string RoomName = "";
        public List<PlayerInfo> Players = new List<PlayerInfo>();

        public PlayerInfo Find(ulong id) => Players.Find(p => p.Id == id);
    }

    [Serializable]
    public sealed class MapSnapshot
    {
        public int Floor = 1;
        public int LastFloor = 6;
        public string ThemeId = "";
        public string ThemeName = "";
        public string TierName = "";
        public bool bRunEnded;
        public List<RoomNode> Rooms = new List<RoomNode>();
        public FloorVoteState Vote = new FloorVoteState();
    }

    [Serializable]
    public sealed class IntentInfo
    {
        public string SkillRow = "";
        public string Kind = "";   // Attack / Defend / Buff / Debuff
        public int Amount;
        public int HitCount = 1;
    }

    [Serializable]
    public sealed class MonsterInfo
    {
        public int Id;
        public int Index;           // 적 1명 스킬의 대상 번호
        public int Slot;
        public string Row = "";
        public string Name = "";
        public string VisualSource = "";
        public bool bHide;
        public CharacterStats Stats = new CharacterStats();
        public CombatState State = new CombatState();
        public bool bHasIntent;
        public IntentInfo Intent = new IntentInfo();
    }

    [Serializable]
    public sealed class AreaInfo
    {
        public int AreaIndex;
        public bool bInUse;
        public bool bCleared;
        public RoomNode Room = new RoomNode();
        public string SetId = "";
        public string Phase = "None";
        public int Cycle;
        public List<ulong> Occupants = new List<ulong>();
        public List<ulong> EndedTurn = new List<ulong>();
        public List<MonsterInfo> Monsters = new List<MonsterInfo>();
        public bool bBetrayal;
        public ulong BetrayerId;
        public bool bBetrayerTurn;
        public bool bBetrayerWon;        // 배신 전투 결과 (끝난 뒤에만 의미 있음)
    }

    [Serializable]
    public sealed class AreasSnapshot
    {
        public List<AreaInfo> Areas = new List<AreaInfo>();

        public AreaInfo Find(int areaIndex) => Areas.Find(a => a.AreaIndex == areaIndex);
    }
}
