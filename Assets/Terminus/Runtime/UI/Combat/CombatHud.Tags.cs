// 전투 HUD - 캐릭터 / 몬스터를 따라다니는 표시 (언리얼 UCombatHUDWidget::RebuildTags / UpdateTags + UPartyCommsWidget 의 계획 / 예상 피해)
//  - 머리 위: 행동 예고 (어두운 판 + 픽셀 아이콘 + 굵은 숫자) / 이번 턴을 끝낸 동료 "대기" / 동료의 행동 계획 + 예상 피해
//  - 발밑: 보호막 줄 4px / 체력 줄 7px (주황 줄이 잠깐 남았다가 따라옴) / 숫자 / 상태이상 글자 칸 (마우스를 올리면 효과)
//  - 대상 선택 칸 (대상 고르는 중일 때만, 스프라이트 위 = 스프라이트를 눌러도 됨), 몬스터 몸 위 정보 카드 (툴팁)
// 자리는 AreaView 가 알려 주는 스프라이트 영역(월드)을 카메라로 화면에 옮긴 것
using System.Collections.Generic;
using Terminus.Combat;
using Terminus.Data;
using Terminus.Game.Client;
using Terminus.Game.Net;
using Terminus.UI.Common;
using UnityEngine;
using UnityEngine.UIElements;

namespace Terminus.UI.Combat
{
    public sealed partial class CombatHud
    {
        // 대상 선택 칸 최소 크기 (언리얼 TargetButtonSize 90x110). 스프라이트가 더 크면 스프라이트 크기
        static readonly Vector2 TargetMinSize = new Vector2(90f, 110f);

        // 캐릭터 / 몬스터 하나를 따라다니는 표시 묶음 (언리얼 FBattlerTag)
        sealed class Tag
        {
            public bool Monster;
            public int MonsterId = -1;     // MonsterInfo.Id
            public ulong PlayerId;         // PlayerInfo.Id

            public VisualElement Head;     // 머리 위 세로 줄 (아래 가운데 정렬): 계획 / 예고 or 대기
            public VisualElement CommsBox;
            public Label PlanText;
            public Label EstimateText;
            public VisualElement IntentBox;
            public VisualElement IntentIcon;
            public Label IntentText;
            public VisualElement ReadyBadge;

            public VisualElement HealthBox; // 발밑 (위 가운데 정렬)
            public VisualElement ShieldFill;
            public VisualElement ChipFill;
            public VisualElement BarFill;
            public Label HealthText;
            public Label ShieldText;
            public VisualElement StatusWrap;

            public VisualElement TargetButton;
            public bool TargetHovered;
            public VisualElement HoverArea;

            // 체력 줄 뒤의 주황 줄: 맞으면 잠깐 멈췄다가 따라 줄어듦 (격투 게임식)
            public float ChipPercent = -1f;
            public float ChipHold;
            public float LastPercent = -1f;

            public string ShownStatuses = "";
            public int ShownIntentIcon = -1;
        }

        readonly List<Tag> _tags = new List<Tag>();
        string _tagKey = "";

        void ClearTags()
        {
            foreach (var tag in _tags)
            {
                tag.Head?.RemoveFromHierarchy();
                tag.HealthBox?.RemoveFromHierarchy();
                tag.TargetButton?.RemoveFromHierarchy();
                if (tag.HoverArea != null)
                {
                    UiTooltip.Detach(tag.HoverArea);
                    tag.HoverArea.RemoveFromHierarchy();
                }
            }
            _tags.Clear();
            _tagKey = "";
        }

        // 몬스터 / 플레이어 구성이 바뀌었으면(새 방, 스폰, 난입) 표시 다시 만들기
        void RebuildTagsIfNeeded(AreaInfo area)
        {
            var key = "";
            foreach (var m in area.Monsters) key += "M" + m.Id + ",";
            foreach (var id in area.Occupants) key += "P" + id + ",";
            if (key == _tagKey) return;

            ClearTags();
            _tagKey = key;
            foreach (var m in area.Monsters) _tags.Add(MakeTag(true, m.Id, 0));
            foreach (var id in area.Occupants) _tags.Add(MakeTag(false, -1, id));
        }

        Tag MakeTag(bool monster, int monsterId, ulong playerId)
        {
            var tag = new Tag { Monster = monster, MonsterId = monsterId, PlayerId = playerId };

            // ---- 발밑: 보호막 줄 4px / 체력 줄 7px / 숫자 / 상태이상 칸. 상태이상 칸만 마우스를 받음
            var box = new VisualElement { pickingMode = PickingMode.Ignore };
            box.AddToClassList("ch-tag-health");

            var shieldTrack = new VisualElement { pickingMode = PickingMode.Ignore };
            shieldTrack.AddToClassList("ch-tag-shieldtrack");
            tag.ShieldFill = new VisualElement { pickingMode = PickingMode.Ignore };
            tag.ShieldFill.AddToClassList("ch-tag-shieldfill");
            shieldTrack.Add(tag.ShieldFill);
            box.Add(shieldTrack);

            // 체력 줄 = 주황 줄(뒤, 바탕 포함) 위에 체력 줄(바탕 없음)
            var barStack = new VisualElement { pickingMode = PickingMode.Ignore };
            barStack.AddToClassList("ch-tag-bar");
            tag.ChipFill = new VisualElement { pickingMode = PickingMode.Ignore };
            tag.ChipFill.AddToClassList("ch-tag-bar__chip");
            tag.BarFill = new VisualElement { pickingMode = PickingMode.Ignore };
            tag.BarFill.AddToClassList("ch-tag-bar__fill");
            tag.BarFill.style.backgroundColor = monster ? CombatLooks.MonsterHealth : CombatLooks.PlayerHealth;
            barStack.Add(tag.ChipFill);
            barStack.Add(tag.BarFill);
            box.Add(barStack);

            var numberRow = UiStyle.Row();
            numberRow.pickingMode = PickingMode.Ignore;
            numberRow.AddToClassList("ch-tag-numbers");
            tag.HealthText = UiStyle.Text("", 13, CombatLooks.MapText, TextAnchor.MiddleLeft, true);
            tag.HealthText.pickingMode = PickingMode.Ignore;
            UiStyle.AddShadow(tag.HealthText);
            tag.ShieldText = UiStyle.Text("", 13, CombatLooks.ShieldText, TextAnchor.MiddleLeft, true);
            tag.ShieldText.pickingMode = PickingMode.Ignore;
            tag.ShieldText.style.marginLeft = 6f;
            UiStyle.AddShadow(tag.ShieldText);
            numberRow.Add(tag.HealthText);
            numberRow.Add(tag.ShieldText);
            box.Add(numberRow);

            tag.StatusWrap = new VisualElement { pickingMode = PickingMode.Ignore };
            tag.StatusWrap.AddToClassList("ch-tag-statuses");
            box.Add(tag.StatusWrap);

            _tagLayer.Add(box);
            tag.HealthBox = box;

            // ---- 머리 위: 계획 / 예상 피해 + 행동 예고(몬스터) or 대기(플레이어)
            var head = new VisualElement { pickingMode = PickingMode.Ignore };
            head.AddToClassList("ch-tag-head");

            tag.CommsBox = new VisualElement { pickingMode = PickingMode.Ignore };
            tag.CommsBox.AddToClassList("ch-tag-comms");
            tag.PlanText = UiStyle.Text("", 14, UiColor.TextSub, TextAnchor.MiddleCenter);
            tag.PlanText.pickingMode = PickingMode.Ignore;
            UiStyle.AddShadow(tag.PlanText, 0.9f);
            tag.EstimateText = UiStyle.Text("", 15, UiColor.GoldHover, TextAnchor.MiddleCenter);
            tag.EstimateText.pickingMode = PickingMode.Ignore;
            UiStyle.AddShadow(tag.EstimateText, 0.9f);
            tag.CommsBox.Add(tag.PlanText);
            tag.CommsBox.Add(tag.EstimateText);
            head.Add(tag.CommsBox);

            if (monster)
            {
                // 어두운 판 + 픽셀 아이콘 + 굵은 숫자 (검은 외곽선)
                tag.IntentBox = new VisualElement { pickingMode = PickingMode.Ignore };
                tag.IntentBox.AddToClassList("ch-tag-intent");
                tag.IntentIcon = new VisualElement { pickingMode = PickingMode.Ignore };
                tag.IntentIcon.AddToClassList("ch-tag-intent__icon");
                tag.IntentText = UiStyle.Text("", 20, UiColor.Hex("FFE3DE"), TextAnchor.MiddleLeft, true);
                tag.IntentText.pickingMode = PickingMode.Ignore;
                tag.IntentText.AddToClassList("ch-tag-intent__number");
                tag.IntentBox.Add(tag.IntentIcon);
                tag.IntentBox.Add(tag.IntentText);
                head.Add(tag.IntentBox);
            }
            else
            {
                // 이번 턴을 끝내면 머리 위 "대기"
                tag.ReadyBadge = new VisualElement { pickingMode = PickingMode.Ignore };
                tag.ReadyBadge.AddToClassList("ch-tag-ready");
                var readyText = UiStyle.Text("대기", 12, UiColor.Hex("BFF0B8"), TextAnchor.MiddleCenter, true);
                readyText.pickingMode = PickingMode.Ignore;
                tag.ReadyBadge.Add(readyText);
                head.Add(tag.ReadyBadge);
            }
            _tagLayer.Add(head);
            tag.Head = head;

            // ---- 몬스터 정보 카드: 몸 위 투명 칸에 툴팁으로. 대상 고르는 중엔 숨김 (대상 칸이 같은 자리)
            if (monster)
            {
                var hover = new VisualElement { pickingMode = PickingMode.Position };
                hover.AddToClassList("ch-tag-hoverarea");
                int id = monsterId;
                UiTooltip.Attach(hover, () => BuildMonsterCard(id));
                _tagLayer.Add(hover);
                tag.HoverArea = hover;
            }

            // ---- 대상 선택 칸 (대상 고르는 중일 때만 보임). 스프라이트 위에 덮어서 스프라이트를 눌러도 고름
            var target = new VisualElement { pickingMode = PickingMode.Position };
            target.AddToClassList("ch-target");
            target.AddManipulator(new Clickable(() => HandleTargetClicked(tag)));
            target.RegisterCallback<PointerEnterEvent>(_ => tag.TargetHovered = true);
            target.RegisterCallback<PointerLeaveEvent>(_ => tag.TargetHovered = false);
            _tagLayer.Add(target);
            tag.TargetButton = target;

            HideTag(tag);
            return tag;
        }

        static void HideTag(Tag tag)
        {
            UiStyle.SetShown(tag.Head, false);
            UiStyle.SetShown(tag.HealthBox, false);
            UiStyle.SetShown(tag.TargetButton, false);
            if (tag.HoverArea != null) UiStyle.SetShown(tag.HoverArea, false);
        }

        // 월드 -> TagLayer 좌표 (패널 배율까지 반영)
        bool Project(Vector3 world, out Vector2 local)
        {
            local = default;
            var cam = AreaView.ViewCamera;
            if (cam == null || panel == null) return false;
            var p = RuntimePanelUtils.CameraTransformWorldToPanel(panel, world, cam);
            local = _tagLayer.WorldToLocal(p);
            return true;
        }

        static void Place(VisualElement e, Vector2 pos)
        {
            e.style.left = pos.x;
            e.style.top = pos.y;
        }

        static void PlaceRect(VisualElement e, Rect r)
        {
            e.style.left = r.x;
            e.style.top = r.y;
            e.style.width = r.width;
            e.style.height = r.height;
        }

        void UpdateTags(float dt)
        {
            var f = _frame;
            var area = f.Area;
            bool playerTurn = ClientCombat.IsPlayerTurn(area);

            // 대상 고르는 중이면 어느 편을 고르는지
            bool pickEnemy = false, pickAlly = false;
            if (_pendingSkillIndex >= 0)
            {
                var pending = _myRun.GetCombatSkill(_pendingSkillIndex);
                if (pending != null)
                {
                    pickEnemy = pending.TargetType == TargetType.SingleEnemy;
                    pickAlly = pending.TargetType == TargetType.SingleAlly;
                }
            }

            // 드래그 중이면 커서 아래 대상을 진하게
            int dragHoverTag = _draggingSkill ? FindTargetTagUnderPointer() : -1;

            for (int tagIndex = 0; tagIndex < _tags.Count; ++tagIndex)
            {
                var tag = _tags[tagIndex];
                MonsterInfo m = null;
                PlayerInfo p = null;
                CombatStats stats = null;
                if (tag.Monster)
                {
                    m = area.Monsters.Find(x => x.Id == tag.MonsterId);
                    if (m != null) stats = ClientCombat.MakeStats(m.Stats, m.State);
                }
                else
                {
                    p = ClientState.Party.Find(tag.PlayerId);
                    if (p != null && p.bHasBattler) stats = ClientCombat.MakeStats(p.Stats, p.Combat);
                }
                bool alive = stats != null && !stats.IsDead() && !(m != null && m.bHide);

                Bounds b;
                bool hasBounds = tag.Monster ? AreaView.TryGetMonsterBounds(tag.MonsterId, out b) : AreaView.TryGetPlayerBounds(tag.PlayerId, out b);
                if (!hasBounds
                    || !Project(new Vector3(b.center.x, b.max.y, 0f), out var headPos)
                    || !Project(new Vector3(b.center.x, b.min.y, 0f), out var feetPos)
                    || !Project(b.min, out var minPos)
                    || !Project(b.max, out var maxPos))
                {
                    HideTag(tag);
                    continue;
                }

                // ---- 머리 위 행동 예고 (아이콘 + 숫자)
                bool headAny = false;
                if (tag.IntentBox != null)
                {
                    bool visible = alive && m.bHasIntent;
                    UiStyle.SetShown(tag.IntentBox, visible);
                    if (visible)
                    {
                        headAny = true;
                        int icon = ClientCombat.IntentIconIndex(m.Intent);
                        if (icon != tag.ShownIntentIcon)
                        {
                            tag.ShownIntentIcon = icon;
                            var tex = _intentTextures[icon];
                            tag.IntentIcon.style.backgroundImage = tex != null ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
                        }
                        var number = ClientCombat.IntentNumber(m.Intent, stats);
                        UiStyle.SetLine(tag.IntentText, number);
                        tag.IntentText.style.color = UiColor.Hex(m.Intent.Kind == "Attack" ? "FFE3DE" : "F0E8D6");
                    }
                }

                // ---- 플레이어가 이번 턴을 끝냈으면 머리 위 "대기"
                if (tag.ReadyBadge != null)
                {
                    bool done = alive && playerTurn && ClientCombat.HasEndedTurn(area, tag.PlayerId);
                    UiStyle.SetShown(tag.ReadyBadge, done);
                    headAny |= done;
                }

                // ---- 동료의 행동 계획 / 예상 피해 (언리얼 UPartyCommsWidget::UpdateTags)
                headAny |= UpdatePlanTexts(tag, area, stats, alive && playerTurn);

                UiStyle.SetShown(tag.Head, headAny);
                if (headAny) Place(tag.Head, headPos + new Vector2(0f, -6f));

                // ---- 발밑 체력
                UiStyle.SetShown(tag.HealthBox, alive);
                if (alive)
                {
                    var state = stats.State;
                    int maxHealth = Mathf.Max(1, stats.Stats.MaxHealth);
                    float percent = Mathf.Clamp01((float)state.Health / maxHealth);
                    tag.BarFill.style.width = Length.Percent(percent * 100f);

                    // 깎이면 0.45초 멈췄다가 초당 최대 체력의 70% 속도로 따라 줄어듦. 회복은 바로 맞춤
                    if (tag.ChipPercent < 0f || percent >= tag.ChipPercent) tag.ChipPercent = percent;
                    else if (tag.LastPercent >= 0f && percent < tag.LastPercent) tag.ChipHold = 0.45f;
                    else if (tag.ChipHold > 0f) tag.ChipHold -= dt;
                    else tag.ChipPercent = Mathf.Max(percent, tag.ChipPercent - dt * 0.7f);
                    tag.LastPercent = percent;
                    tag.ChipFill.style.width = Length.Percent(tag.ChipPercent * 100f);

                    UiStyle.SetTextIfChanged(tag.HealthText, state.Health + "/" + maxHealth);
                    // 보호막: 체력 줄 위 4px 회청색 줄 (최대 체력 대비) + 숫자 옆 "+6"
                    UiStyle.SetLine(tag.ShieldText, state.Shield > 0 ? "+" + state.Shield : "");
                    float shieldWidth = state.Shield > 0 ? Mathf.Clamp(90f * state.Shield / maxHealth, 2f, 90f) : 0f;
                    tag.ShieldFill.style.width = shieldWidth;

                    RefreshStatusIcons(tag, stats.GetDisplayStatuses());
                    Place(tag.HealthBox, feetPos + new Vector2(0f, 4f));
                }

                // ---- 대상 선택 칸. 배신 전투면 적 = 상대 편 플레이어, 아군 = 같은 편
                bool selectable = alive && ((pickEnemy && tag.Monster) || (pickAlly && !tag.Monster));
                if (area.bBetrayal && !tag.Monster && f.Me != null)
                {
                    bool sameSide = ClientCombat.IsSameSide(area, tag.PlayerId, f.Me.Id);
                    selectable = alive && ((pickEnemy && !sameSide) || (pickAlly && sameSide));
                }

                var rect = Rect.MinMaxRect(Mathf.Min(minPos.x, maxPos.x), Mathf.Min(minPos.y, maxPos.y), Mathf.Max(minPos.x, maxPos.x), Mathf.Max(minPos.y, maxPos.y));
                if (rect.width < TargetMinSize.x) rect = new Rect(rect.center.x - TargetMinSize.x * 0.5f, rect.y, TargetMinSize.x, rect.height);
                if (rect.height < TargetMinSize.y) rect = new Rect(rect.x, rect.center.y - TargetMinSize.y * 0.5f, rect.width, TargetMinSize.y);

                UiStyle.SetShown(tag.TargetButton, selectable);
                if (!selectable) tag.TargetHovered = false;
                if (selectable)
                {
                    PlaceRect(tag.TargetButton, rect);
                    tag.TargetButton.style.opacity = tagIndex == dragHoverTag || tag.TargetHovered ? 1f : 0.55f;
                    tag.TargetButton.EnableInClassList("ch-target--hot", tagIndex == dragHoverTag);
                }

                if (tag.HoverArea != null)
                {
                    bool showHover = alive && !selectable && !_draggingSkill;
                    UiStyle.SetShown(tag.HoverArea, showHover);
                    if (showHover) PlaceRect(tag.HoverArea, rect);
                }
            }
        }

        // 상태이상 글자 칸 (종류 + 수치가 그대로면 다시 안 그림)
        static void RefreshStatusIcons(Tag tag, List<StatusInstance> statuses)
        {
            var key = "";
            foreach (var s in statuses)
            {
                if (s.Type == StatusEffect.None) continue;
                key += (int)s.Type + ":" + s.Value + ":" + s.Duration + ",";
            }
            if (key == tag.ShownStatuses) return;
            tag.ShownStatuses = key;

            var wrap = tag.StatusWrap;
            foreach (var child in wrap.Children()) UiTooltip.Detach(child);
            wrap.Clear();
            foreach (var s in statuses)
            {
                if (s.Type == StatusEffect.None) continue;
                CombatLooks.GetStatusLook(s.Type, out var letter, out var color);

                // [글자 칸 16px] 수치
                var item = UiStyle.Row();
                item.AddToClassList("ch-status");
                item.pickingMode = PickingMode.Position;

                var cell = new VisualElement { pickingMode = PickingMode.Ignore };
                cell.AddToClassList("ch-status__cell");
                CombatLooks.Border(cell, 1f, color);
                var letterText = UiStyle.Text(letter, 8, color, TextAnchor.MiddleCenter, true);
                letterText.pickingMode = PickingMode.Ignore;
                letterText.AddToClassList("ch-status__letter");
                cell.Add(letterText);
                item.Add(cell);

                // 수치가 없는 상태(불사 / 불굴 등)는 남은 턴
                int shown = s.Value > 0 ? s.Value : s.Duration;
                var count = UiStyle.Text(shown > 0 ? shown.ToString() : "", 11, CombatLooks.MapText, TextAnchor.MiddleLeft, true);
                count.pickingMode = PickingMode.Ignore;
                count.style.marginLeft = 2f;
                UiStyle.AddShadow(count);
                item.Add(count);

                // 마우스를 올리면 "허약(얼음) 1 · 남은 2사이클 / 수치만큼 공격 스텟이 하락한다."
                UiTooltip.SetText(item, CombatLooks.StatusTip(s));
                wrap.Add(item);
            }
        }

        // 몬스터 정보 카드 (11-03 피드백: 이름 / 체력 / 상태만. 공격 방어 스텟과 다음 행동은 안 보여 줌)
        VisualElement BuildMonsterCard(int monsterId)
        {
            var area = AreaView.ShownArea;
            var m = area?.Monsters.Find(x => x.Id == monsterId);
            if (m == null) return null;
            var stats = ClientCombat.MakeStats(m.Stats, m.State);

            var card = new VisualElement { pickingMode = PickingMode.Ignore };
            CombatLooks.Use(card);
            card.AddToClassList("ch-card");

            var row = GameData.FindMonsterRow(m.Row);
            var title = UiStyle.Text(row != null ? row.DisplayName_KR : (string.IsNullOrEmpty(m.Name) ? m.Row : m.Name), 17, UiColor.Gold, TextAnchor.UpperLeft, true);
            card.Add(title);

            var state = stats.State;
            var body = "체력 " + state.Health + "/" + stats.Stats.MaxHealth;
            if (state.Shield > 0) body += "   보호막 " + state.Shield;

            // 상태이상 (같은 상태가 쌓였으면 한 줄로 합쳐서)
            bool header = false;
            foreach (var s in stats.GetDisplayStatuses())
            {
                if (!header) { body += "\n\n상태"; header = true; }
                body += "\n· " + StatusInfo.GetDisplayName(s.Type);
                if (s.Value > 0) body += " " + s.Value;
                if (s.Duration > 0) body += " (남은 " + s.Duration + "사이클)";
                var desc = StatusInfo.GetDescription(s.Type);
                if (!string.IsNullOrEmpty(desc)) body += ": " + desc;
            }
            var bodyText = UiStyle.WrapText(body, 13, UiColor.TextBody, TextAnchor.UpperLeft);
            bodyText.AddToClassList("ch-card__body");
            card.Add(bodyText);
            return card;
        }

        // =====================================================================
        // 행동 계획 표시 (언리얼 UPartyCommsWidget::UpdateTags 의 계획 / 예상 피해)
        // =====================================================================

        bool UpdatePlanTexts(Tag tag, AreaInfo area, CombatStats targetStats, bool show)
        {
            var planLabel = "";
            int estimated = 0;
            if (show)
            {
                foreach (var plan in CombatPlanBoard.All)
                {
                    if (!IsPlanShown(plan, area) || !PlanTouches(plan, area, tag)) continue;
                    if (planLabel.Length > 0) planLabel += "\n";
                    planLabel += DescribePlan(plan);

                    if (plan.Effect == CombatPlanEffect.Damage && IsEnemyOf(area, plan.PlannerId, tag))
                    {
                        var planner = ClientState.Party.Find(plan.PlannerId);
                        var caster = planner != null && planner.bHasBattler ? ClientCombat.MakeStats(planner.Stats, planner.Combat) : null;
                        estimated += ClientCombat.EstimateHit(plan.Amount, caster, targetStats) * Mathf.Max(1, plan.HitCount);
                    }
                }

                // 대상을 고르는 중인 사람은 자기 머리 위에 "(대상 고르는 중)"
                if (!tag.Monster)
                {
                    foreach (var plan in CombatPlanBoard.All)
                    {
                        if (!IsChoosing(plan) || plan.PlannerId != tag.PlayerId || !IsPlanShown(plan, area)) continue;
                        if (planLabel.Length > 0) planLabel += "\n";
                        planLabel += DescribePlan(plan);
                    }
                }
            }
            UiStyle.SetLine(tag.PlanText, planLabel);

            var estimateLabel = "";
            bool lethal = false;
            if (estimated > 0 && targetStats != null)
            {
                var state = targetStats.State;
                lethal = estimated >= state.Health + state.Shield;
                estimateLabel = state.Shield > 0
                    ? $"예상 피해 약 {estimated} / 체력 {state.Health} (+{state.Shield})"
                    : $"예상 피해 약 {estimated} / 체력 {state.Health}";
                if (lethal) estimateLabel += "  처치!";
            }
            tag.EstimateText.style.color = lethal ? UiColor.Alert : UiColor.GoldHover;
            UiStyle.SetLine(tag.EstimateText, estimateLabel);

            bool any = planLabel.Length > 0 || estimateLabel.Length > 0;
            UiStyle.SetShown(tag.CommsBox, any);
            return any;
        }

        static bool IsChoosing(CombatPlanMsg plan) =>
            plan.TargetKind == CombatPlanTargetKind.None && (plan.TargetType == TargetType.SingleEnemy || plan.TargetType == TargetType.SingleAlly);

        // 몬스터는 늘 적. 플레이어끼리는 배신 전투에서 편이 갈릴 때만 적
        static bool IsEnemyOf(AreaInfo area, ulong viewerId, Tag tag)
        {
            if (tag.Monster) return true;
            return area.bBetrayal && !ClientCombat.IsSameSide(area, viewerId, tag.PlayerId);
        }

        static bool PlanTouches(CombatPlanMsg plan, AreaInfo area, Tag tag)
        {
            switch (plan.TargetType)
            {
                case TargetType.AllEnemies:
                    return IsEnemyOf(area, plan.PlannerId, tag);
                case TargetType.AllAllies:
                    return !tag.Monster && !IsEnemyOf(area, plan.PlannerId, tag);
                default:
                    // 자신 / 1명 대상: 서버가 넣어 준 대상. 아직 고르는 중이면 여기선 아님
                    if (plan.TargetKind == CombatPlanTargetKind.Monster) return tag.Monster && tag.MonsterId == plan.TargetMonsterId;
                    if (plan.TargetKind == CombatPlanTargetKind.Player) return !tag.Monster && tag.PlayerId == plan.TargetPlayerId;
                    return false;
            }
        }

        static bool IsPlanShown(CombatPlanMsg plan, AreaInfo area) =>
            plan.IsValid() && plan.AreaIndex == area.AreaIndex
            && ClientCombat.IsPlayerTurn(area)
            && plan.Cycle == area.Cycle
            && ClientCombat.IsTurnOf(area, plan.PlannerId)
            && !ClientCombat.HasEndedTurn(area, plan.PlannerId)
            && area.Occupants.Contains(plan.PlannerId);

        static string DescribePlan(CombatPlanMsg plan)
        {
            var skill = GameData.FindSkillRow(plan.SkillRow);
            var skillName = skill != null ? skill.DisplayName_KR : plan.SkillRow;

            var value = "";
            switch (plan.Effect)
            {
                case CombatPlanEffect.Damage:
                    value = plan.HitCount > 1 ? $" 피해 {plan.Amount}x{plan.HitCount}" : $" 피해 {plan.Amount}";
                    break;
                case CombatPlanEffect.Shield: value = $" 보호막 {plan.Amount}"; break;
                case CombatPlanEffect.Heal: value = $" 회복 {plan.Amount}"; break;
            }

            return (plan.bDeclared ? "[예약] " : "") + ClientState.NameOf(plan.PlannerId) + ": " + skillName + value
                + (IsChoosing(plan) ? " (대상 고르는 중)" : "");
        }
    }
}
