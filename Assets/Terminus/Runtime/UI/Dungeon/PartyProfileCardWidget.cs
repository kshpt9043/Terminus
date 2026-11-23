// 지도 상단바의 참가자 프로필 카드 한 장 (언리얼 UPartyProfileCardWidget / WBP_ProfilCard). 초상화, 이름, 직업, 체력, 상태 점
//
// WBP 구조: ScaleBox > SizeBox 224x52 > CardBorder (#3A253D + 1px #24162A, 안쪽 6 6 10 6)
//   [SizeBox 40x40 (오른쪽 10) > Border (#4A3B4D + 1px #1A1218, 안쪽 1) > Portrait]
//   [VerticalBox (채움, 가운데)
//      NameRow: NameText (14, 채움, 잘림) / ClassText (12, 왼쪽 6 위 1) / StatusDot (8x8 둥근, 왼쪽 6)
//      SizeBox 높이 6 (위 4) > HealthBar (#1A1218 바탕, #FF1A4C 채움) (+ HealthText 숨김)]
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Dungeon
{
    public sealed class PartyProfileCardWidget : VisualElement
    {
        // 내 카드 테두리색 (다른 사람 카드는 WBP 에서 정한 테두리 그대로)
        public Color LocalOutlineColor = UiColor.FromLinear(0.584f, 0.371f, 0.107f);   // #C9A45C

        // 쓰러졌을 때 카드 불투명도 (흑백 + 흐림)
        public float DownOpacity = 0.55f;

        enum Status { Ready, InCombat, Down, Unknown }

        readonly VisualElement _cardBorder;
        readonly VisualElement _portrait;
        readonly Label _nameText;
        readonly Label _classText;
        readonly VisualElement _statusDot;
        readonly VisualElement _healthFill;

        ulong _playerId;
        string _shownName;
        CharacterClass? _shownClass;
        int _shownHealth = int.MinValue;
        int _shownMaxHealth = int.MinValue;
        Status _shownStatus = Status.Unknown;
        bool? _shownLocal;

        public ulong PlayerId => _playerId;

        public PartyProfileCardWidget()
        {
            pickingMode = PickingMode.Ignore;
            style.width = 224f;
            style.height = 52f;
            style.flexShrink = 0;

            _cardBorder = new VisualElement { name = "CardBorder", pickingMode = PickingMode.Ignore };
            _cardBorder.style.flexGrow = 1;
            _cardBorder.style.flexDirection = FlexDirection.Row;
            _cardBorder.style.alignItems = Align.Center;
            DungeonStyle.Box(_cardBorder, MapColor.PartyPlate, MapColor.PartyEdge);
            _cardBorder.style.paddingLeft = 6f;
            _cardBorder.style.paddingTop = 6f;
            _cardBorder.style.paddingRight = 10f;
            _cardBorder.style.paddingBottom = 6f;
            Add(_cardBorder);

            var portraitFrame = new VisualElement { pickingMode = PickingMode.Ignore };
            portraitFrame.style.width = 40f;
            portraitFrame.style.height = 40f;
            portraitFrame.style.flexShrink = 0;
            portraitFrame.style.marginRight = 10f;
            DungeonStyle.Box(portraitFrame, UiColor.Hex("4A3B4D"), UiColor.Hex("1A1218"));
            UiStyle.Padding(portraitFrame, 1f, 1f);
            _portrait = new VisualElement { name = "Portrait", pickingMode = PickingMode.Ignore };
            _portrait.style.flexGrow = 1;
            _portrait.style.backgroundSize = new BackgroundSize(BackgroundSizeType.Cover);
            portraitFrame.Add(_portrait);
            _cardBorder.Add(portraitFrame);

            var info = new VisualElement { pickingMode = PickingMode.Ignore };
            info.style.flexGrow = 1;
            info.style.flexShrink = 1;
            info.style.justifyContent = Justify.Center;
            _cardBorder.Add(info);

            var nameRow = new VisualElement { name = "NameRow", pickingMode = PickingMode.Ignore };
            nameRow.style.flexDirection = FlexDirection.Row;
            nameRow.style.alignItems = Align.Center;
            info.Add(nameRow);

            _nameText = UiStyle.Text("", 14, MapColor.Text, TextAnchor.MiddleLeft);
            _nameText.style.flexGrow = 1;
            _nameText.style.flexShrink = 1;
            _nameText.style.overflow = Overflow.Hidden;
            _nameText.style.textOverflow = TextOverflow.Clip;
            nameRow.Add(_nameText);

            _classText = UiStyle.Text("", 12, MapColor.TextSub, TextAnchor.MiddleLeft);
            _classText.style.marginLeft = 6f;
            _classText.style.marginTop = 1f;
            _classText.style.flexShrink = 0;
            nameRow.Add(_classText);

            _statusDot = new VisualElement { name = "StatusDot", pickingMode = PickingMode.Ignore };
            _statusDot.style.width = 8f;
            _statusDot.style.height = 8f;
            _statusDot.style.marginLeft = 6f;
            _statusDot.style.flexShrink = 0;
            UiStyle.Radius(_statusDot, 4f);
            _statusDot.style.backgroundColor = MapColor.GreenMana;
            nameRow.Add(_statusDot);

            var bar = new VisualElement { name = "HealthBar", pickingMode = PickingMode.Ignore };
            bar.style.height = 6f;
            bar.style.marginTop = 4f;
            bar.style.backgroundColor = MapColor.HealthBack;
            info.Add(bar);
            _healthFill = new VisualElement { pickingMode = PickingMode.Ignore };
            _healthFill.style.height = Length.Percent(100);
            _healthFill.style.width = Length.Percent(72);
            _healthFill.style.backgroundColor = MapColor.Health;
            bar.Add(_healthFill);
        }

        public void SetPlayer(ulong playerId)
        {
            _playerId = playerId;
            _shownName = null;
            _shownClass = null;
            _shownLocal = null;
            _shownHealth = _shownMaxHealth = int.MinValue;   // 사람이 바뀌면 체력도 다시 그림
            _shownStatus = Status.Unknown;
            Refresh();
        }

        /// <summary>파티 스냅샷이 바뀔 때 (언리얼 NativeTick)</summary>
        public void Refresh()
        {
            var p = ClientState.Party.Find(_playerId);
            if (p == null) return;

            if (p.Name != _shownName || p.Class != _shownClass) RefreshStatic(p);
            RefreshHealth(p);
        }

        void RefreshStatic(PlayerInfo p)
        {
            _shownName = p.Name;
            _shownClass = p.Class;

            _nameText.text = p.Name;
            var row = GameData.FindCharacterClassRow(p.Class);
            _classText.text = row != null ? row.DisplayName : "";

            // 직업 일러스트 (DT_CharacterClass Illustration). 아직 유니티에 그림이 없음 -> 자리만 남기고 숨김 (카드 폭이 사람마다 달라지지 않게)
            Texture2D tex = row != null && !string.IsNullOrEmpty(row.Illustration) ? Resources.Load<Texture2D>("UI/Portraits/" + row.Illustration) : null;
            if (tex != null) _portrait.style.backgroundImage = new StyleBackground(tex);
            _portrait.style.visibility = tex != null ? Visibility.Visible : Visibility.Hidden;

            // 내 카드는 금 테두리
            bool local = p.Id == ClientState.MyId;
            if (_shownLocal != local)
            {
                _shownLocal = local;
                UiStyle.Border(_cardBorder, 1f, local ? LocalOutlineColor : MapColor.PartyEdge);
            }
        }

        void RefreshHealth(PlayerInfo p)
        {
            // 최대 체력은 런 스텟. 현재 체력은 배틀러가 전투에서 들고 있는 값 (아직 없으면 가득 찬 걸로)
            int maxHealth = p.Run != null && p.Run.bStatsInitialized ? p.Run.Stats.MaxHealth : 0;
            int health = maxHealth;
            if (p.bHasBattler && p.Stats != null && p.Stats.MaxHealth > 0)
            {
                maxHealth = p.Stats.MaxHealth;
                health = p.Combat != null ? p.Combat.Health : maxHealth;
            }

            RefreshStatus(p, maxHealth > 0 && health <= 0);

            if (health == _shownHealth && maxHealth == _shownMaxHealth) return;
            _shownHealth = health;
            _shownMaxHealth = maxHealth;
            _healthFill.style.width = Length.Percent(maxHealth > 0 ? Mathf.Clamp01((float)health / maxHealth) * 100f : 0f);
        }

        void RefreshStatus(PlayerInfo p, bool down)
        {
            // 쓰러짐 > 전투 중(던전 구역 안) > 준비
            var status = down ? Status.Down : p.AreaIndex >= 0 ? Status.InCombat : Status.Ready;
            if (status == _shownStatus) return;
            _shownStatus = status;

            _statusDot.style.backgroundColor = status == Status.Down ? MapColor.TextSub
                : status == Status.InCombat ? MapColor.BlueTorch : MapColor.GreenMana;

            // 쓰러짐: 흑백 + 흐림 (UMG 엔 흑백 효과가 없어 회색 틴트로 -> 여기선 흐림만 + 글자 회색)
            style.opacity = status == Status.Down ? DownOpacity : 1f;
            var gray = new Color(0.55f, 0.55f, 0.55f, 1f);
            _nameText.style.color = status == Status.Down ? MapColor.Text * gray : MapColor.Text;
            _classText.style.color = status == Status.Down ? MapColor.TextSub * gray : MapColor.TextSub;
        }
    }
}
