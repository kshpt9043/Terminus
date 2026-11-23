// 거점 강화 (언리얼 UBaseWidget + WBP_Base). 메인 메뉴 '거점' -> UILayer.MenuWindow
// 세력 -> 직업 -> 시설(연무장 / 훈련소) -> 강화. 골드로 사고 바로 프로필에 저장 (ProfileService.TryUpgradeStat / TryUpgradeSkill)
//  - 연무장: 스테이터스 줄마다 [단계/최대] +증가량 비용 [+]
//  - 훈련소: 기본 스킬 3장 (이름 / 설명 / 수치 변화 / 비용 [+]) + 아래 단계 칸
// 화면 틀(배경 / 우상단 골드·버튼 / 가운데 제목·안내·내용)은 WBP, 내용 카드는 C++ BuildXxxView
// ESC / 돌아가기 = 한 단계 뒤로 (세력 화면이면 닫기), 메인화면 = 닫기
using System.Collections.Generic;
using Terminus.Data;
using Terminus.Game.Save;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Menu
{
    public sealed class BaseView : VisualElement
    {
        enum View { Faction, Class, Facility, StatHall, SkillHall }

        // 카드 크기 (UBaseWidget CardSize) / 세력 화면에 띄울 세력 (황실은 거점이 없음) / 직업 화면 최소 카드 수
        static readonly Vector2 CardSize = new Vector2(220f, 320f);
        static readonly Faction[] Factions = { Faction.AdventurersGuild, Faction.MageTower, Faction.Religion };
        const int MinClassCards = 3;

        readonly Label _gold;
        readonly Label _title;
        readonly Label _message;
        readonly VisualElement _content;
        readonly Button _mainMenu;

        View _view = View.Faction;
        Faction _faction = Faction.AdventurersGuild;
        CharacterClass _class = CharacterClass.Fighter;
        readonly List<CharacterClass> _shownClasses = new List<CharacterClass>();

        public BaseView()
        {
            MenuStyle.Use(this, "Menu", "MenuWindows");
            AddToClassList("tm-fill");
            pickingMode = PickingMode.Position;

            // [1] 배경 (0.006995,0.005605,0.004391 / 0.98), 안쪽 48 / 32
            var background = MenuStyle.FullScreen(MenuStyle.WindowBackground);
            UiStyle.Padding(background, 48, 32);
            Add(background);
            var stage = new VisualElement();
            stage.style.flexGrow = 1;
            background.Add(stage);

            // [11] 가운데: 제목 / 안내 / 내용
            var column = UiStyle.Column();
            column.style.position = Position.Absolute;
            column.style.left = 0;
            column.style.right = 0;
            column.style.top = 0;
            column.style.bottom = 0;
            column.style.alignItems = Align.Center;
            stage.Add(column);
            _title = MenuStyle.Text("거점", UiStyle.ScreenTitle, UiColor.TextBody, true, TextAnchor.MiddleCenter);
            column.Add(_title);
            _message = MenuStyle.Text("", UiStyle.Aux, UiColor.TextFaint, true, TextAnchor.MiddleCenter);
            _message.style.marginTop = 8;
            column.Add(_message);
            _content = UiStyle.Row();
            _content.style.flexGrow = 1;
            _content.style.justifyContent = Justify.Center;
            _content.style.alignItems = Align.Center;
            column.Add(_content);

            // [3] 우상단: 보유 골드 + 돌아가기 / 메인화면
            var corner = UiStyle.Column();
            corner.style.position = Position.Absolute;
            corner.style.right = 0;
            corner.style.top = 0;
            stage.Add(corner);
            _gold = MenuStyle.Text("", 15, UiColor.Gold, true);
            corner.Add(_gold);
            corner.Add(CornerButton("돌아가기", Back));
            _mainMenu = CornerButton("메인화면", Close);
            corner.Add(_mainMenu);

            MenuStyle.OnEscape(this, Back);
            RegisterCallback<AttachToPanelEvent>(_ =>
            {
                var profile = LocalProfile.Service;
                profile.GoldChanged -= OnGoldChanged;
                profile.GoldChanged += OnGoldChanged;
                profile.UpgradesChanged -= Rebuild;
                profile.UpgradesChanged += Rebuild;
            });
            RegisterCallback<DetachFromPanelEvent>(_ =>
            {
                LocalProfile.Service.GoldChanged -= OnGoldChanged;
                LocalProfile.Service.UpgradesChanged -= Rebuild;
            });
        }

        static Button CornerButton(string label, System.Action onClick)
        {
            var b = MenuStyle.FlatButton(label, onClick);
            b.style.width = 170;
            b.style.height = 46;
            b.style.marginTop = 10;
            return b;
        }

        // ------------------------------------------------------------ 열기 / 화면 전환

        public void Open()
        {
            UIRoot.Instance?.Show(UILayer.MenuWindow, this);
            RefreshGold();
            ShowView(View.Faction);
            schedule.Execute(() => Focus());
        }

        public void Close()
        {
            RemoveFromHierarchy();
        }

        void Back()
        {
            switch (_view)
            {
                case View.Faction: Close(); break;
                case View.Class: ShowView(View.Faction); break;
                case View.Facility: ShowView(View.Class); break;
                default: ShowView(View.Facility); break;
            }
        }

        void ShowView(View view)
        {
            _view = view;
            SetMessage("");
            // 시안: 세력 / 직업 화면은 돌아가기만, 그 아래부터 메인화면도
            UiStyle.SetShown(_mainMenu, view != View.Faction && view != View.Class);
            Rebuild();
        }

        void Rebuild()
        {
            _content.Clear();
            switch (_view)
            {
                case View.Faction: BuildFactionView(); break;
                case View.Class: BuildClassView(); break;
                case View.Facility: BuildFacilityView(); break;
                case View.StatHall: BuildStatHallView(); break;
                case View.SkillHall: BuildSkillHallView(); break;
            }
        }

        // ------------------------------------------------------------ 화면별 내용

        void BuildFactionView()
        {
            _title.text = "거점";
            foreach (var f in Factions)
            {
                var faction = f;
                AddCard(CharacterNames.GetFactionName(f), () =>
                {
                    _faction = faction;
                    ShowView(View.Class);
                });
            }
        }

        void BuildClassView()
        {
            _title.text = CharacterNames.GetFactionName(_faction);
            _shownClasses.Clear();
            for (int i = 0; i < CharacterNames.ClassCount; ++i)
            {
                var c = (CharacterClass)i;
                var row = GameData.FindCharacterClassRow(c);
                if (row == null || row.Faction != _faction) continue;
                _shownClasses.Add(c);
                AddCard(row.DisplayName, () =>
                {
                    _class = c;
                    ShowView(View.Facility);
                });
            }
            // 시안: 직업이 적으면 '추가 예정'
            for (int i = _shownClasses.Count; i < MinClassCards; ++i) AddCard("추가 예정", null, false);
        }

        void BuildFacilityView()
        {
            _title.text = $"{CharacterNames.GetFactionName(_faction)} · {ClassName(_class)}";
            AddCard("연무장", () => ShowView(View.StatHall));
            AddCard("훈련소", () => ShowView(View.SkillHall));
        }

        void BuildStatHallView()
        {
            _title.text = $"연무장 · {ClassName(_class)}";

            var profile = LocalProfile.Service;
            var settings = GameData.Upgrades ?? UpgradeSettings.Default;
            var upgrades = profile.GetClassUpgrades(_class);
            int gold = profile.Gold;

            var panel = UiStyle.Panel(28f, 20f, UiColor.Board);
            _content.Add(panel);

            for (int i = 0; i < ClassUpgrades.StatCount; ++i)
            {
                var stat = (StatUpgrade)i;
                var rule = settings.FindStatRule(stat);
                if (rule == null) continue;

                int level = upgrades.GetStatLevel(stat);
                int cost = settings.GetStatCost(stat, level);
                bool max = cost < 0;

                var line = UiStyle.Row();
                line.style.marginTop = 5;
                line.style.marginBottom = 5;
                panel.Add(line);

                line.Add(Cell(UpgradeSettings.GetStatUpgradeName(stat), 140, UiStyle.Subtitle, UiColor.TextBody));
                line.Add(Cell($"[{level}/{rule.MaxLevel}]", 90, UiStyle.Subtitle, UiColor.TextBody));
                line.Add(Cell($"+{level * rule.AmountPerLevel}", 70, UiStyle.Aux, UiColor.TextAux));
                line.Add(Cell(max ? "최대" : $"{cost} 골드", 140, UiStyle.Subtitle,
                    max ? UiColor.TextAux : gold >= cost ? UiColor.Gold : UiColor.Alert));

                var plus = PlusButton(() =>
                {
                    if (LocalProfile.Service.TryUpgradeStat(_class, stat)) SetMessage($"{UpgradeSettings.GetStatUpgradeName(stat)} 강화 완료");
                    else SetMessage("골드가 부족합니다.", true);
                });
                plus.SetEnabled(!max && gold >= cost);
                line.Add(plus);
            }
        }

        void BuildSkillHallView()
        {
            _title.text = $"훈련소 · {ClassName(_class)}";

            var profile = LocalProfile.Service;
            var settings = GameData.Upgrades ?? UpgradeSettings.Default;
            var upgrades = profile.GetClassUpgrades(_class);
            int gold = profile.Gold;

            // 전투 HUD 기본 칸과 같은 순서
            var basic = GameData.FindBasicSkills(_class);
            for (int i = 0; i < ClassUpgrades.NumBasicSkills; ++i)
            {
                var skill = i < basic.Count ? basic[i] : null;
                int level = upgrades.GetSkillLevel(i);
                int cost = settings.GetSkillCost(level);
                bool max = cost < 0;

                var card = UiStyle.Column();
                card.style.marginLeft = 14;
                card.style.marginRight = 14;
                _content.Add(card);

                // 카드 몸통: 이름 / 설명 / 비용 + 강화 버튼
                var body = UiStyle.Panel(14f, 14f, UiColor.Card);
                body.style.width = CardSize.x;
                body.style.height = CardSize.y * 0.8f;
                card.Add(body);

                var nameText = MenuStyle.Text(skill != null ? skill.DisplayName_KR : $"기본 스킬 {i + 1}", 20, UiColor.Gold, true, TextAnchor.MiddleCenter);
                body.Add(nameText);

                string effect = skill == null ? ""
                    : skill.BaseValue != 0 ? $"수치 {skill.BaseValue + level} → {skill.BaseValue + level + (max ? 0 : 1)}"
                    : $"상태 수치 {skill.StatusValue + level} → {skill.StatusValue + level + (max ? 0 : 1)}";
                var desc = MenuStyle.Wrap((skill != null ? UiText.FixNewlines(skill.Description_KR) : "") + "\n\n" + (max ? $"수치 +{level} (최대)" : effect),
                    14, UiColor.TextBody);
                desc.style.marginTop = 10;
                desc.style.flexGrow = 1;
                body.Add(desc);

                var buy = UiStyle.Row();
                body.Add(buy);
                var costText = MenuStyle.Text(max ? "최대" : $"{cost} 골드", UiStyle.Body, max ? UiColor.TextAux : gold >= cost ? UiColor.Gold : UiColor.Alert);
                costText.style.flexGrow = 1;
                buy.Add(costText);
                int index = i;
                var plus = PlusButton(() =>
                {
                    if (LocalProfile.Service.TryUpgradeSkill(_class, index)) SetMessage("기본 스킬 강화 완료");
                    else SetMessage("골드가 부족합니다.", true);
                });
                plus.SetEnabled(!max && gold >= cost && skill != null);
                buy.Add(plus);

                // 카드 아래 단계 칸 (시안: 작은 네모. 채운 단계 = 금, 빈 단계 = 안쪽색 + 테두리)
                var pips = UiStyle.Row();
                pips.style.alignSelf = Align.Center;
                pips.style.marginTop = 10;
                card.Add(pips);
                for (int p = 0; p < settings.SkillMaxLevel; ++p)
                {
                    var pip = new VisualElement();
                    pip.style.width = 26;
                    pip.style.height = 26;
                    pip.style.marginLeft = 4;
                    pip.style.marginRight = 4;
                    pip.style.backgroundColor = p < level ? UiColor.Gold : UiColor.Inset;
                    UiStyle.Border(pip, 2f, p < level ? UiColor.GoldPressed : UiColor.Line);
                    pips.Add(pip);
                }
            }
        }

        // ------------------------------------------------------------ 공용

        // 시안: 회색 상자, 아래쪽에 이름
        void AddCard(string label, System.Action onClick, bool enabled = true)
        {
            var button = MenuStyle.FlatButton("", onClick);
            button.AddToClassList("tm-bigcard");
            button.style.width = CardSize.x;
            button.style.height = CardSize.y;
            button.style.marginLeft = 10;
            button.style.marginRight = 10;
            button.SetEnabled(enabled);

            var text = MenuStyle.Text(label, UiStyle.Section, enabled ? UiColor.TextBody : UiColor.TextFaint, false, TextAnchor.MiddleCenter);
            text.pickingMode = PickingMode.Ignore;
            text.style.position = Position.Absolute;
            text.style.left = 0;
            text.style.right = 0;
            text.style.bottom = 20;
            button.Add(text);
            _content.Add(button);
        }

        static Button PlusButton(System.Action onClick)
        {
            var b = MenuStyle.FlatButton(" + ", onClick, 16);
            b.style.paddingLeft = 14;
            b.style.paddingRight = 14;
            return b;
        }

        static VisualElement Cell(string text, float width, int size, Color color)
        {
            var l = MenuStyle.Text(text, size, color);
            l.style.width = width;
            return l;
        }

        void SetMessage(string text, bool error = false)
        {
            _message.text = text ?? "";
            _message.style.color = error ? UiColor.Alert : UiColor.TextFaint;
        }

        void OnGoldChanged(int gold, int delta) => RefreshGold();

        void RefreshGold()
        {
            _gold.text = "보유 골드: " + LocalProfile.Service.Gold.ToString("N0", System.Globalization.CultureInfo.InvariantCulture);
        }

        static string ClassName(CharacterClass c)
        {
            var row = GameData.FindCharacterClassRow(c);
            return row != null ? row.DisplayName : CharacterNames.GetClassName(c);
        }
    }
}
