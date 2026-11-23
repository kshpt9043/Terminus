// 전투 HUD - 입력 (언리얼 UCombatHUDWidget 의 OnSkillClicked / 드래그 / HandleTargetClicked / 우클릭 취소 / 행동 계획 공유)
// 언리얼은 FSlateApplication 으로 Shift 와 커서를 읽었음 -> 여기선 루트가 포인터 이벤트를 TrickleDown 으로 먼저 받아 기록
// (프로젝트가 새 Input System 전용이라 UnityEngine.Input 은 못 씀)
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
        // 스킬 칸을 누른 채 이만큼(화면 px) 움직이면 드래그로 봄. 그보다 덜 움직이고 놓으면 그냥 클릭
        const float SkillDragStartDistance = 10f;

        // 계획 보내기 최소 간격 (언리얼 PlanSendInterval)
        const float PlanSendInterval = 0.15f;

        // 대상을 고르는 중인 스킬 칸 (0~2 기본, 3~ 강화. -1 = 아님)
        int _pendingSkillIndex = -1;

        // ---- 포인터 (루트가 TrickleDown 으로 기록)
        Vector2 _pointerPos;          // 패널 좌표
        bool _shiftDown;

        // ---- 스킬 드래그 (칸을 누른 채 끌어서 대상에 놓기)
        int _pressedSkillIndex = -1;  // 누르고 있는 칸 (아직 드래그 전일 수도 있음)
        Vector2 _pressStartPos;
        bool _pressReleased;
        bool _draggingSkill;
        bool _skillPressConsumed;     // 이번 누름은 이미 끝났음 (취소 / 턴 넘어감). 손을 뗄 때까지 다시 드래그로 안 봄
        Label _dragGhost;             // 커서를 따라다니는 스킬 이름

        // ---- 행동 계획 공유: Shift 로 예약한 계획 (-1 = 없음). 예약한 사이클이 지나면 버림
        int _declaredSkillIndex = -1;
        CombatPlanTargetKind _declaredTargetKind;
        int _declaredMonsterId = -1;
        ulong _declaredPlayerId;
        int _declaredCycle = -1;

        // ---- 보낸 계획 / 보낼 계획 (언리얼 PC 의 SentPlan* / PendingPlan*)
        struct PlanKey
        {
            public int Skill;
            public CombatPlanTargetKind Kind;
            public int MonsterId;
            public ulong PlayerId;
            public bool Declared;

            public static PlanKey None => new PlanKey { Skill = -1, MonsterId = -1 };

            public bool SameAs(PlanKey o) => Skill == o.Skill && Kind == o.Kind && MonsterId == o.MonsterId && PlayerId == o.PlayerId && Declared == o.Declared;
        }
        PlanKey _sentPlan = PlanKey.None;
        PlanKey _wantedPlan = PlanKey.None;
        float _lastPlanSend = -1f;

        void RegisterInput()
        {
            // 누가 받든 커서 / Shift 를 먼저 기록 (칸 버튼이 포인터를 잡고 있어도 조상인 루트는 지나감)
            RegisterCallback<PointerMoveEvent>(e =>
            {
                _pointerPos = e.position;
                _shiftDown = e.shiftKey;
            }, TrickleDown.TrickleDown);
            RegisterCallback<PointerDownEvent>(e =>
            {
                _pointerPos = e.position;
                _shiftDown = e.shiftKey;
                // 우클릭 = 대상 고르기 취소 (어디를 눌러도)
                if (e.button == 1 && _pendingSkillIndex >= 0)
                {
                    CancelTargeting();
                    e.StopPropagation();
                }
            }, TrickleDown.TrickleDown);
            RegisterCallback<PointerUpEvent>(e =>
            {
                _pointerPos = e.position;
                _shiftDown = e.shiftKey;
                if (e.button == 0) _pressReleased = true;
            }, TrickleDown.TrickleDown);
        }

        void OnSlotPointerDown(ItemSlotWidget slot, PointerDownEvent e)
        {
            if (e.button != 0) return;
            _pointerPos = e.position;
            _shiftDown = e.shiftKey;
            // 비용 부족 / 내 턴 아님이면 칸이 안 눌림 (언리얼은 칸 버튼이 꺼져 있었음)
            if (!slot.IsUsable || slot.Kind == ItemSlotKind.Empty) return;
            _pressedSkillIndex = slot.SlotIndex;
            _pressStartPos = e.position;
            _pressReleased = false;
            _skillPressConsumed = false;
        }

        // 대상 고르는 중에만 바탕이 클릭을 받음: 우클릭 취소, 대상 칸을 빗나간 왼클릭은 스프라이트로 한 번 더 맞춰 봄
        void OnCatcherPointerDown(PointerDownEvent e)
        {
            if (_pendingSkillIndex < 0) return;
            if (e.button == 1)
            {
                CancelTargeting();
                e.StopPropagation();
                return;
            }
            if (e.button != 0) return;

            int tagIndex = FindTagBySprite(e.position);
            if (tagIndex >= 0 && UiStyle.IsShown(_tags[tagIndex].TargetButton))
            {
                HandleTargetClicked(_tags[tagIndex]);
                e.StopPropagation();
            }
        }

        // 평소엔 바탕이 클릭을 안 받게(버튼만 받음). 대상 고르는 중에만 바탕도 받아서 우클릭 취소가 오게
        void UpdateCatcher()
        {
            var wanted = _visible && _pendingSkillIndex >= 0 ? PickingMode.Position : PickingMode.Ignore;
            if (_catcher.pickingMode != wanted) _catcher.pickingMode = wanted;
        }

        // =====================================================================
        // 스킬 / 대상
        // =====================================================================

        void OnSkillClicked(int skillIndex)
        {
            var f = _frame;
            // 관전 중인 구역에선 못 씀
            if (!_visible || f.Area == null || f.Spectating || f.Me == null) return;

            // 같은 스킬을 다시 누르면 취소
            if (_pendingSkillIndex == skillIndex)
            {
                CancelTargeting();
                return;
            }

            bool shift = _shiftDown;

            // 예약한 스킬을 Shift + 클릭 = 예약 취소
            if (shift && _declaredSkillIndex == skillIndex)
            {
                ClearDeclaredPlan();
                return;
            }

            var skill = _myRun.GetCombatSkill(skillIndex);
            if (skill == null) return;

            if (skill.TargetType == TargetType.SingleEnemy || skill.TargetType == TargetType.SingleAlly)
            {
                // 대상을 골라야 하는 스킬 -> 대상 고르기 모드 (바탕이 우클릭을 받게 바로 전환)
                _pendingSkillIndex = skillIndex;
                UpdateCatcher();
                return;
            }

            CancelTargeting();

            // Shift: 쓰지 않고 예약만 공유
            if (shift)
            {
                DeclarePlan(skillIndex, null);
                return;
            }

            // 대상이 정해져 있는 스킬(자신 / 전체)은 바로 사용
            NetBus.Request(new UseSkillReq { SkillIndex = skillIndex, TargetIndex = -1 });
            NotifySkillUsed();
        }

        void CancelTargeting()
        {
            // 드래그 중이었으면 그것도 끝냄 (칸은 아직 눌려 있을 수 있음 -> 손을 뗄 때까지 같은 누름으로 다시 시작하지 않게)
            if (_draggingSkill)
            {
                _draggingSkill = false;
                _skillPressConsumed = true;
                if (_dragGhost != null) UiStyle.SetShown(_dragGhost, false);
            }

            if (_pendingSkillIndex < 0) return;
            _pendingSkillIndex = -1;
            UpdateCatcher();
        }

        void HandleTargetClicked(Tag tag)
        {
            if (_pendingSkillIndex < 0 || tag == null) return;

            // Shift: 쓰지 않고 이 대상으로 예약만 공유
            if (_shiftDown)
            {
                int skillIndex = _pendingSkillIndex;
                CancelTargeting();
                DeclarePlan(skillIndex, tag);
                return;
            }

            UseOn(_pendingSkillIndex, tag);
            CancelTargeting();
            NotifySkillUsed();
        }

        // 대상 번호: 몬스터는 MonsterInfo.Index, 플레이어는 구역 Occupants 순서 (전투 Players 와 같음)
        void UseOn(int skillIndex, Tag tag)
        {
            var area = _frame.Area;
            int targetIndex = -1;
            if (tag.Monster)
            {
                var m = area?.Monsters.Find(x => x.Id == tag.MonsterId);
                if (m != null) targetIndex = m.Index;
            }
            else
            {
                targetIndex = ClientCombat.OccupantIndex(area, tag.PlayerId);
            }
            if (targetIndex < 0) return;
            NetBus.Request(new UseSkillReq { SkillIndex = skillIndex, TargetIndex = targetIndex });
        }

        void HandleEndTurnClicked()
        {
            CancelTargeting();
            ClearDeclaredPlan();
            ClearCombatPlan();
            if (!_frame.MyTurn) return;
            NetBus.Request(new EndTurnReq());
        }

        // =====================================================================
        // 스킬 드래그: 칸을 누른 채 끌어서 대상 위에서 놓기
        // =====================================================================

        void UpdateSkillDrag()
        {
            if (_draggingSkill)
            {
                // 놓았으면 그 자리 대상에게
                if (_pressReleased || _pressedSkillIndex < 0)
                {
                    FinishSkillDrag();
                    return;
                }

                // 커서를 따라다니는 스킬 이름
                if (_dragGhost != null) Place(_dragGhost, _tagLayer.WorldToLocal(_pointerPos) + new Vector2(18f, 10f));
                return;
            }

            if (_pressedSkillIndex < 0) return;
            if (_pressReleased)
            {
                // 그냥 클릭이었음 (칸의 Clicked 가 처리)
                _pressedSkillIndex = -1;
                _skillPressConsumed = false;
                return;
            }
            if (_skillPressConsumed) return;

            // 조금 움직인 건 클릭으로 둠. 드래그로 쓸 수 있는 건 대상을 고르는 스킬(적 1명 / 아군 1명)만
            if (Vector2.Distance(_pointerPos, _pressStartPos) < SkillDragStartDistance) return;

            var skill = _myRun.GetCombatSkill(_pressedSkillIndex);
            if (skill == null || (skill.TargetType != TargetType.SingleEnemy && skill.TargetType != TargetType.SingleAlly)) return;
            // 관전 중 / 내 턴 아님이면 못 씀 (클릭과 같은 규칙)
            if (_frame.Spectating || !_frame.MyTurn) return;

            // 대상 고르기 모드로 들어감 -> 대상 칸이 보이고 동료 화면에도 계획이 보임
            _draggingSkill = true;
            _pendingSkillIndex = _pressedSkillIndex;
            UpdateCatcher();

            if (_dragGhost == null)
            {
                _dragGhost = UiStyle.Text("", UiStyle.Subtitle, UiColor.Gold, TextAnchor.MiddleLeft, true);
                _dragGhost.pickingMode = PickingMode.Ignore;
                _dragGhost.AddToClassList("ch-dragghost");
                UiStyle.AddShadow(_dragGhost);
                _content.Add(_dragGhost);
            }
            _dragGhost.text = skill.DisplayName_KR;
            _dragGhost.BringToFront();
            UiStyle.SetShown(_dragGhost, true);
            Place(_dragGhost, _tagLayer.WorldToLocal(_pointerPos) + new Vector2(18f, 10f));
        }

        void FinishSkillDrag()
        {
            int tagIndex = FindTargetTagUnderPointer();
            int skillIndex = _pendingSkillIndex;
            bool shift = _shiftDown;

            _pressedSkillIndex = -1;
            CancelTargeting();   // 드래그 / 대상 고르기 끝 (대상 밖에서 놓았으면 여기서 그냥 취소)

            if (skillIndex < 0 || tagIndex < 0 || tagIndex >= _tags.Count) return;

            // Shift: 쓰지 않고 이 대상으로 예약만 공유 (Shift + 클릭과 같음)
            if (shift)
            {
                DeclarePlan(skillIndex, _tags[tagIndex]);
                return;
            }

            UseOn(skillIndex, _tags[tagIndex]);
            NotifySkillUsed();
        }

        // 커서 아래에 있는(보이는) 대상 칸. 칸 버튼이 포인터를 잡고 있어 hover 가 안 바뀜 -> 화면 영역으로 직접 판정
        int FindTargetTagUnderPointer()
        {
            for (int i = 0; i < _tags.Count; ++i)
            {
                var target = _tags[i].TargetButton;
                if (target != null && UiStyle.IsShown(target) && target.worldBound.Contains(_pointerPos)) return i;
            }
            return -1;
        }

        // 화면 좌표의 스프라이트로 찾기 (AreaView.PickAt). 패널 좌표 -> 화면 픽셀 (왼쪽 아래 0,0)
        int FindTagBySprite(Vector2 panelPos)
        {
            var root = panel?.visualTree;
            if (root == null) return -1;
            var size = root.worldBound.size;
            if (size.x < 1f || size.y < 1f) return -1;
            var screen = new Vector2(panelPos.x / size.x * Screen.width, Screen.height - panelPos.y / size.y * Screen.height);
            if (!AreaView.PickAt(screen, out int monsterId, out ulong playerId, out bool isMonster)) return -1;

            for (int i = 0; i < _tags.Count; ++i)
            {
                var tag = _tags[i];
                if (isMonster ? (tag.Monster && tag.MonsterId == monsterId) : (!tag.Monster && tag.PlayerId == playerId)) return i;
            }
            return -1;
        }

        // =====================================================================
        // 행동 계획 공유 (표시는 CombatHud.Tags 의 UpdatePlanTexts)
        // =====================================================================

        void DeclarePlan(int skillIndex, Tag target)
        {
            var area = _frame.Area;
            if (area == null) return;

            _declaredSkillIndex = skillIndex;
            _declaredTargetKind = target == null ? CombatPlanTargetKind.None : target.Monster ? CombatPlanTargetKind.Monster : CombatPlanTargetKind.Player;
            _declaredMonsterId = target != null && target.Monster ? target.MonsterId : -1;
            _declaredPlayerId = target != null && !target.Monster ? target.PlayerId : 0;
            _declaredCycle = area.Cycle;
        }

        void ClearDeclaredPlan()
        {
            _declaredSkillIndex = -1;
            _declaredTargetKind = CombatPlanTargetKind.None;
            _declaredMonsterId = -1;
            _declaredPlayerId = 0;
            _declaredCycle = -1;
        }

        // 스킬을 실제로 썼을 때 (예약 / 공유 계획 지우기)
        void NotifySkillUsed()
        {
            ClearDeclaredPlan();
            ClearCombatPlan();
        }

        // 지금 계획(대상 고르는 중 / 예약)을 정함 -> 바뀐 것만 서버로
        void UpdateSharedPlan()
        {
            var f = _frame;
            var area = f.Area;

            // 계획은 내가 행동할 수 있을 때만 (관전 / 남의 차례 / 턴 끝냄 / 쓰러짐이면 지움)
            bool canPlan = f.MyTurn;
            if (!canPlan || (area != null && _declaredCycle != area.Cycle)) ClearDeclaredPlan();

            PlanKey wanted;
            if (!canPlan)
            {
                wanted = PlanKey.None;
            }
            else if (_pendingSkillIndex >= 0)
            {
                // 대상 고르는 중이면 마우스가 올라간 대상이 동료 화면에 실시간으로 보임
                wanted = new PlanKey { Skill = _pendingSkillIndex, MonsterId = -1 };
                int hovered = _draggingSkill ? FindTargetTagUnderPointer() : FindHoveredTargetTag();
                if (hovered >= 0)
                {
                    var tag = _tags[hovered];
                    wanted.Kind = tag.Monster ? CombatPlanTargetKind.Monster : CombatPlanTargetKind.Player;
                    wanted.MonsterId = tag.Monster ? tag.MonsterId : -1;
                    wanted.PlayerId = tag.Monster ? 0 : tag.PlayerId;
                }
            }
            else if (_declaredSkillIndex >= 0)
            {
                wanted = new PlanKey { Skill = _declaredSkillIndex, Kind = _declaredTargetKind, MonsterId = _declaredMonsterId, PlayerId = _declaredPlayerId, Declared = true };
            }
            else
            {
                wanted = PlanKey.None;
            }

            _wantedPlan = wanted;
            FlushPlanIfDue(false);
        }

        int FindHoveredTargetTag()
        {
            for (int i = 0; i < _tags.Count; ++i)
            {
                if (_tags[i].TargetHovered && UiStyle.IsShown(_tags[i].TargetButton)) return i;
            }
            return -1;
        }

        // 계획을 지움 (모아 둔 계획이 나중에 나가서 지운 걸 덮지 않게 바로)
        void ClearCombatPlan()
        {
            _wantedPlan = PlanKey.None;
            FlushPlanIfDue(true);
        }

        // 마우스를 대상 위로 빠르게 옮길 때 매번 보내지 않고, 간격이 지나면 마지막 값만
        void FlushPlanIfDue(bool force)
        {
            if (_wantedPlan.SameAs(_sentPlan)) return;
            // 혼자면 나눌 사람이 없음 (언리얼: 싱글은 계획 위젯 자체가 없음)
            if (NetworkRoot.IsSinglePlayer || !NetBus.IsConnected) return;

            float now = Time.realtimeSinceStartup;
            if (!force && _lastPlanSend >= 0f && now - _lastPlanSend < PlanSendInterval) return;

            _sentPlan = _wantedPlan;
            _lastPlanSend = now;
            NetBus.Request(new SetCombatPlanReq
            {
                SkillIndex = _sentPlan.Skill,
                TargetKind = _sentPlan.Kind,
                TargetMonsterId = _sentPlan.MonsterId,
                TargetPlayerId = _sentPlan.PlayerId,
                bDeclared = _sentPlan.Declared,
            });
        }
    }
}
