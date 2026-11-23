// 런이 끝나도 남는 영구 데이터 (언리얼 Game/TerminusProfileSubsystem.h 의 FPendingSettlement / UTerminusProfileSave 이식)
// 세이브에 들어가는 클래스: [Serializable] + public 필드 + List 만 (유니티 JsonUtility 가 읽을 수 있게)
using System.Collections.Generic;
using Terminus.Data;

namespace Terminus.Run
{
    // 아직 안 끝낸 정산 (던전 탈출 직후 저장 -> 메인 화면에서 정산). 순서: 유물 보관 -> 완료 퀘스트 -> 최종 골드
    // 사용자 결정 10-08: 던전에서 얻은 유물 중 하나만 골라 창고로, 나머지는 사라짐 (유물 판매 없음. 노션 정산 페이지의 판매와 다름)
    //   -> 10-26 결정으로 바뀜: 원하는 만큼 보관하고, 안 고른 유물은 자동 판매 (RelicGold 주석)
    // 방장이 먼저 나가서 끊기거나 게임이 꺼져도 다음 메인 화면에서 이어서 정산하게 프로필에 저장
    [System.Serializable]
    public sealed class PendingSettlement
    {
        public bool bValid = false;

        // 전멸로 끝남 (기획 사망 순서도): 스킬은 못 가져감. 유물은 탈출과 똑같이 보관 / 판매 (노션 10-26: 사망도 퇴장 경우)
        public bool bDeath = false;

        // "던전을 탈출했습니다." 같은 안내 / 던전 이름 / 몇 층까지 갔는지
        public string Reason = "";
        public string RoomName = "";
        public int Floor = 0;

        // 고를 수 있는 유물: 던전에서 얻은 것 (직업 기본 유물 / 몬스터 유물 / 시작 유물 제외)
        public List<string> Relics = new List<string>();

        // Relics 와 같은 순서의 판매 골드 (DT 정산 판매가, 못 파는 건 0)
        // 노션(10-26) + 사용자 결정(10-26): 원하는 만큼 보관하고, 안 고른 유물은 자동 판매 (DT 정산 판매가, 못 파는 건 0)
        public List<int> RelicGold = new List<int>();

        // 하드 모드에서 죽음: 창고에서 들고 간 시작 유물이 창고(도감)에서 지워짐 (사용자 결정 10-08)
        public List<string> LostStoredRelics = new List<string>();

        // 추가 골드와 이유 (배신자 패배: 배신자 보스 유물 판매 분배 / 퀘스트 골드 보상)
        public int BonusGold = 0;
        public string BonusReason = "";

        // 장착 중인 픽업 스킬 중 아직 없는 것 하나 랜덤 (전부 있으면 "" = 건너뜀)
        public string KeptSkill = "";

        public PendingSettlement Clone()
        {
            var c = (PendingSettlement)MemberwiseClone();
            c.Relics = new List<string>(Relics);
            c.RelicGold = new List<int>(RelicGold);
            c.LostStoredRelics = new List<string>(LostStoredRelics);
            return c;
        }
    }

    /// <summary>
    /// 런이 끝나도 남는 플레이어 영구 데이터. 이 컴퓨터의 세이브 슬롯에 저장됨 (멀티면 각자 자기 컴퓨터의 것을 씀)
    /// </summary>
    [System.Serializable]
    public sealed class ProfileSave
    {
        // 유니티 이식 때 추가. 언리얼 프로필 세이브엔 버전이 없었음 (형식이 바뀌면 여기로 이전 판단)
        public const int CurrentVersion = 1;
        public int Version = CurrentVersion;

        // 영구 소유한 스킬 (DT_Skill 행 이름). 던전을 클리어하면 그 던전에서 쓴 스킬 중 하나를 여기에 얻음
        // 런 시작 때 이 중에서 강화 스킬을 하나 골라 장착
        public List<string> OwnedSkills = new List<string>();

        // 골드. 던전 밖 재화 (정산에서 얻고 훈련소 / 직업의 탑에서 씀). 던전 재화와 달리 판이 끝나도 남음
        public int Gold = 0;

        // 창고에 보관한 유물 (DT_Relic 행 이름). 정산에서 팔지 않고 '보유'를 고른 것
        // 도감처럼 개수 제한 없이 종류별로 하나씩. 던전에 들어갈 때 이 중 1개를 들고 갈 수 있음 (기획 UI 레퍼런스 > 게임 시작)
        public List<string> StoredRelics = new List<string>();

        // 거점 강화 (직업마다). 연무장 스텟 / 훈련소 기본 스킬 단계
        public List<ClassUpgrades> Upgrades = new List<ClassUpgrades>();

        // 아직 안 끝낸 정산 (있으면 메인 화면이 정산 화면부터 띄움)
        public PendingSettlement PendingSettlement = new PendingSettlement();
    }
}
