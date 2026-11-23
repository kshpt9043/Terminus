// [클라] 내가 있는 구역의 무대: 왼쪽 플레이어, 오른쪽 몬스터 (언리얼 ADungeonArea 의 슬롯 위치 + 배틀러 / 몬스터 액터)
// 스냅샷 체력 변화로 피격 / 사망 연출, MonsterActingMsg 로 공격 / 방어 / 스페셜 연출
using System.Collections.Generic;
using Terminus.Art;
using Terminus.Game.Net;
using UnityEngine;

namespace Terminus.Game.Client
{
    public sealed class AreaView : MonoBehaviour
    {
        // 화면 비율 기준 자리 (아래쪽 35% 는 전투 HUD 가 덮음). 언리얼 구역의 플레이어 / 몬스터 슬롯 위치 자리
        const float PlayerViewX = 0.33f;
        const float PlayerViewStep = 0.085f;
        const float MonsterViewX = 0.57f;
        const float MonsterViewStep = 0.145f;
        const float GroundViewY = 0.52f;

        static Vector3 ViewToWorld(float vx, float vy)
        {
            var cam = Camera.main;
            if (cam == null) return new Vector3((vx - 0.5f) * 10f, (vy - 0.5f) * 6f, 0);
            var p = cam.ViewportToWorldPoint(new Vector3(vx, vy, Mathf.Abs(cam.transform.position.z)));
            p.z = 0;
            return p;
        }

        sealed class Actor
        {
            public GameObject Root;
            public SpriteRenderer Sprite;
            public FlipbookPlayer Player;
            public MonsterAnimator MonsterAnim;
            public Transform HealthFill;
            public Transform ShieldFill;
            public int LastHealth = int.MinValue;
            public bool Dead;
        }

        readonly Dictionary<ulong, Actor> _players = new Dictionary<ulong, Actor>();
        readonly Dictionary<int, Actor> _monsters = new Dictionary<int, Actor>();
        int _shownArea = -1;
        static Sprite _white;

        public static AreaView Create()
        {
            var go = new GameObject("AreaView");
            return go.AddComponent<AreaView>();
        }

        void OnEnable()
        {
            ClientState.AreasChanged += Refresh;
            ClientState.PartyChanged += Refresh;
            NetBus.On<MonsterActingMsg>(OnMonsterActing);
            Refresh();
        }

        void OnDisable()
        {
            ClientState.AreasChanged -= Refresh;
            ClientState.PartyChanged -= Refresh;
        }

        void OnMonsterActing(MonsterActingMsg msg)
        {
            if (this == null || msg.AreaIndex != _shownArea) return;
            if (!_monsters.TryGetValue(msg.MonsterId, out var a) || a.MonsterAnim == null) return;
            switch (msg.Kind)
            {
                case "Attack": a.MonsterAnim.PlayAction(MonsterAction.Attack); break;
                case "Defend": a.MonsterAnim.PlayAction(MonsterAction.Defend); break;
                default: a.MonsterAnim.PlayAction(MonsterAction.Special); break;
            }
        }

        void Refresh()
        {
            if (this == null) return;
            var area = ClientState.MyArea;
            int areaIndex = area != null ? area.AreaIndex : -1;
            if (areaIndex != _shownArea || area == null || !area.bInUse)
            {
                ClearAll();
                _shownArea = area != null && area.bInUse ? areaIndex : -1;
            }
            if (_shownArea < 0) return;

            // 플레이어
            var seen = new HashSet<ulong>();
            for (int i = 0; i < area.Occupants.Count; ++i)
            {
                var id = area.Occupants[i];
                var info = ClientState.Party.Find(id);
                if (info == null) continue;
                seen.Add(id);
                if (!_players.TryGetValue(id, out var actor))
                {
                    actor = MakeActor("P_" + info.Name, Vector3.zero, false);
                    var visual = Resources.Load<MonsterVisualSet>("Visuals/Characters/" + info.Class);
                    if (visual != null) actor.Player.Play(visual.Idle, true);
                    _players[id] = actor;
                }
                actor.Root.transform.position = ViewToWorld(PlayerViewX - i * PlayerViewStep, GroundViewY);
                UpdateBars(actor, info.Combat.Health, info.Stats.MaxHealth, info.Combat.Shield);
                actor.Sprite.color = info.Combat.Health <= 0 ? new Color(1, 1, 1, 0.35f) : Color.white;
            }
            RemoveMissing(_players, seen);

            // 몬스터
            var seenM = new HashSet<int>();
            foreach (var m in area.Monsters)
            {
                seenM.Add(m.Id);
                if (!_monsters.TryGetValue(m.Id, out var actor))
                {
                    actor = MakeActor("M_" + m.Name, Vector3.zero, true);
                    var visual = MonsterVisualSet.LoadBySource(m.VisualSource) ?? MonsterVisualSet.LoadBySource("slime_blue");
                    actor.MonsterAnim = actor.Root.AddComponent<MonsterAnimator>();
                    if (visual != null) actor.MonsterAnim.SetVisual(visual);
                    _monsters[m.Id] = actor;
                }

                actor.Root.transform.position = ViewToWorld(MonsterViewX + m.Slot * MonsterViewStep, GroundViewY);
                int hp = m.State.Health;
                if (actor.LastHealth != int.MinValue && hp < actor.LastHealth && actor.MonsterAnim != null)
                {
                    actor.MonsterAnim.PlayAction(MonsterAction.Hit);
                }
                if (hp <= 0 && !actor.Dead)
                {
                    actor.Dead = true;
                    actor.MonsterAnim?.PlayAction(MonsterAction.Death);
                }
                actor.LastHealth = hp;
                UpdateBars(actor, hp, m.Stats.MaxHealth, m.State.Shield);
                if (actor.HealthFill != null) actor.HealthFill.parent.gameObject.SetActive(!actor.Dead);
            }
            RemoveMissing(_monsters, seenM);
        }

        Actor MakeActor(string name, Vector3 pos, bool monster)
        {
            var root = new GameObject(name);
            root.transform.SetParent(transform, false);
            root.transform.localPosition = pos;
            var sr = root.AddComponent<SpriteRenderer>();
            sr.sortingOrder = 10;
            var player = root.AddComponent<FlipbookPlayer>();

            // 체력 / 보호막 막대 (머리 위)
            var bar = new GameObject("Bar");
            bar.transform.SetParent(root.transform, false);
            bar.transform.localPosition = new Vector3(0, monster ? 0.9f : 0.75f, 0);
            MakeQuad(bar.transform, "Back", new Color(0.1f, 0.07f, 0.09f), 0.8f, 0.1f, 20);
            var fill = MakeQuad(bar.transform, "Fill", new Color(1f, 0.1f, 0.3f), 0.8f, 0.1f, 21);
            var shield = MakeQuad(bar.transform, "Shield", new Color(0.55f, 0.75f, 1f), 1f, 0.05f, 22);
            shield.localPosition = new Vector3(-0.4f, 0.09f, 0);

            return new Actor { Root = root, Sprite = sr, Player = player, HealthFill = fill, ShieldFill = shield };
        }

        static Transform MakeQuad(Transform parent, string name, Color color, float width, float height, int order)
        {
            if (_white == null)
            {
                var tex = Texture2D.whiteTexture;
                _white = Sprite.Create(tex, new Rect(0, 0, tex.width, tex.height), new Vector2(0f, 0.5f), tex.width);
            }
            var go = new GameObject(name);
            go.transform.SetParent(parent, false);
            go.transform.localPosition = new Vector3(-0.4f, 0, 0);
            go.transform.localScale = new Vector3(width, height, 1);
            var sr = go.AddComponent<SpriteRenderer>();
            sr.sprite = _white;
            sr.color = color;
            sr.sortingOrder = order;
            return go.transform;
        }

        static void UpdateBars(Actor a, int health, int maxHealth, int shield)
        {
            float ratio = maxHealth > 0 ? Mathf.Clamp01((float)health / maxHealth) : 0f;
            a.HealthFill.localScale = new Vector3(0.8f * ratio, a.HealthFill.localScale.y, 1);
            float sratio = maxHealth > 0 ? Mathf.Clamp01((float)shield / maxHealth) : 0f;
            a.ShieldFill.localScale = new Vector3(0.8f * sratio, a.ShieldFill.localScale.y, 1);
            a.ShieldFill.gameObject.SetActive(shield > 0);
        }

        static void RemoveMissing<TKey>(Dictionary<TKey, Actor> map, HashSet<TKey> keep)
        {
            var remove = new List<TKey>();
            foreach (var kv in map)
            {
                if (!keep.Contains(kv.Key)) remove.Add(kv.Key);
            }
            foreach (var k in remove)
            {
                if (map[k].Root != null) Destroy(map[k].Root);
                map.Remove(k);
            }
        }

        void ClearAll()
        {
            foreach (var a in _players.Values) if (a.Root != null) Destroy(a.Root);
            foreach (var a in _monsters.Values) if (a.Root != null) Destroy(a.Root);
            _players.Clear();
            _monsters.Clear();
        }
    }
}
