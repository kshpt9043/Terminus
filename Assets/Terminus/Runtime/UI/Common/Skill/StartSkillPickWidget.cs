// 런 시작 강화 스킬 고르기 (언리얼 UStartSkillPickWidget + WBP_StartSkillPick, 여는 규칙은 PC::OpenStartSkillPick)
// 던전에 들어와 지도를 보기 전에 화면 전체를 덮고 뜸
//  - 보유 스킬(프로필 GetUsableOwnedSkills) 이 3개 이상이면 섞어서 3장, 1~2개면 그만큼
//  - 보유 스킬이 없으면 "보유한 스킬이 없습니다" + "아무 키나 누르면 지도로 이동합니다", 0.4초 뒤부터 아무 키 / 클릭 = 건너뜀
//  - 카드를 누르면 서버에 ChooseStartSkillReq 를 보내고 스스로 닫힘 (건너뛰면 SkillRow = "")
//
//   StartSkillPickWidget.OpenForLocalPlayer();               // 보통은 이것만 (UILayer.Pick 에 띄움)
//   var w = new StartSkillPickWidget(); w.SetChoices(rows);  // 직접 고를 목록을 줄 때
using System;
using System.Collections.Generic;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.Game.Save;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Common
{
    public sealed class StartSkillPickWidget : VisualElement
    {
        /// <summary>카드 사이 간격</summary>
        public float CardSpacing = 24f;

        /// <summary>보유 스킬 없음 화면에서 입력을 받기 시작하기까지 (초). 직전 클릭 / 키가 바로 넘겨버리지 않게</summary>
        public float AnyKeyDelay = 0.4f;

        /// <summary>최대 카드 수 (PC::OpenStartSkillPick 의 3장)</summary>
        public const int MaxCards = 3;

        readonly Label _titleText;
        readonly Label _messageText;
        readonly VisualElement _cardBox;
        bool _chosen;
        bool _noSkills;
        float _shownTime;

        /// <summary>고르고 닫힌 뒤 (행 이름. 건너뛰면 ""). 서버 요청은 이미 보낸 뒤</summary>
        public event Action<string> Finished;

        /// <summary>true 면 Finish 때 NetBus 로 ChooseStartSkillReq 를 보냄 (기본). 미리보기 / 테스트면 false</summary>
        public bool SendRequest = true;

        public StartSkillPickWidget()
        {
            UiStyle.Use(this, "Common", "Skill");
            AddToClassList("tc-startskill");
            pickingMode = PickingMode.Position;   // 아래 지도를 못 누르게
            focusable = true;

            _titleText = UiStyle.Title("");
            _titleText.AddToClassList("tc-startskill__title");
            Add(_titleText);

            _cardBox = new VisualElement { name = "CardBox", pickingMode = PickingMode.Ignore };
            _cardBox.AddToClassList("tc-startskill__cards");
            Add(_cardBox);

            _messageText = UiStyle.Text("", UiStyle.Subtitle, UiColor.TextSub, TextAnchor.MiddleCenter);
            _messageText.AddToClassList("tc-startskill__message");
            Add(_messageText);

            RegisterCallback<KeyDownEvent>(e => { if (TrySkip()) e.StopPropagation(); });
            RegisterCallback<PointerDownEvent>(e => { TrySkip(); e.StopPropagation(); });
            RegisterCallback<AttachToPanelEvent>(_ => schedule.Execute(() => Focus()));
        }

        /// <summary>
        /// 내 프로필의 쓸 수 있는 보유 스킬로 띄움 (UILayer.Pick). 이미 골랐거나 아직 던전 밖이면 null.
        /// 화면은 ClientState.PartyChanged 에서 Me.Run.bStartSkillChosen == false 일 때 한 번 부르면 됨
        /// </summary>
        public static StartSkillPickWidget OpenForLocalPlayer()
        {
            var me = ClientState.Me;
            if (me == null || me.Run == null || me.Run.bStartSkillChosen) return null;

            var pool = new List<string>(LocalProfile.Service.GetUsableOwnedSkills(me.Class));
            // 3개 이상이면 그 중 3장, 1~2개면 전부. 완전 첫판(없음)이면 빈 채로 -> 안내 화면
            for (int i = pool.Count - 1; i > 0; --i)
            {
                int j = UnityEngine.Random.Range(0, i + 1);
                (pool[i], pool[j]) = (pool[j], pool[i]);
            }
            if (pool.Count > MaxCards) pool.RemoveRange(MaxCards, pool.Count - MaxCards);

            var w = new StartSkillPickWidget();
            w.SetChoices(pool);
            UIRoot.Instance?.Show(UILayer.Pick, w);
            return w;
        }

        /// <summary>보여줄 카드 (DT_Skill 행 이름들). 비어 있으면 "보유 스킬 없음" 안내</summary>
        public void SetChoices(IReadOnlyList<string> skillRows)
        {
            int count = skillRows?.Count ?? 0;
            _noSkills = count == 0;
            _chosen = false;
            _shownTime = Time.realtimeSinceStartup;

            _titleText.text = _noSkills ? "보유한 스킬이 없습니다"
                : count > 1 ? "강화 스킬을 하나 골라 장착하세요"
                : "강화 스킬을 장착하세요";

            _messageText.text = "아무 키나 누르면 지도로 이동합니다";
            UiStyle.SetShown(_messageText, _noSkills);

            _cardBox.Clear();
            UiStyle.SetShown(_cardBox, !_noSkills);
            for (int i = 0; i < count; ++i)
            {
                var card = new SkillCardWidget();
                card.SetSkill(skillRows[i]);
                card.Clicked += Finish;
                card.style.marginLeft = i == 0 ? 0f : CardSpacing * 0.5f;
                card.style.marginRight = i == count - 1 ? 0f : CardSpacing * 0.5f;
                _cardBox.Add(card);
            }
        }

        // 보유 스킬 없음 화면에서 아무 입력이나 -> 건너뛰고 지도로. 처리했으면 true
        bool TrySkip()
        {
            if (!_noSkills || _chosen) return false;
            if (Time.realtimeSinceStartup - _shownTime < AnyKeyDelay) return true;   // 먹기만 하고 아직 안 넘김
            Finish("");
            return true;
        }

        /// <summary>서버에 결과 보내고 닫기 ("" = 건너뜀). 한 번만</summary>
        public void Finish(string skillRow)
        {
            if (_chosen) return;
            _chosen = true;
            skillRow ??= "";
            if (SendRequest) NetBus.Request(new ChooseStartSkillReq { SkillRow = skillRow });
            RemoveFromHierarchy();
            Finished?.Invoke(skillRow);
        }
    }
}
