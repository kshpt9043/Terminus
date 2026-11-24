// [클라] 지금 보는 구역의 무대: 왼쪽 플레이어, 오른쪽 몬스터 (언리얼 ADungeonArea 의 슬롯 위치 + ATerminusBattler / ATerminusMonster 액터)
//  - 자리: 언리얼 DungeonArea 기본 슬롯 (플레이어 -520 / -390 / -260 / -130, 몬스터 170 / 340 / 510, 카메라 화면 폭 ±650) 을 화면 비율로
//  - 배신 전투: 배신자는 맞은편 몬스터 0번 자리에서 왼쪽을 봄 (언리얼 BeginBetrayal 의 Place(Betrayer, GetMonsterSlotLocation(0)))
//  - 스냅샷 체력이 줄면 피격 (몬스터 Hit 플립북 + 붉은 번쩍임), 0 이 되면 사망 (몬스터는 피격 다음에 사망 플립북, 플레이어는 흐리게)
//  - MonsterActingMsg 로 공격 / 방어 / 스페셜 플립북 (언리얼 PlayIntentAnim)
//  - SkillUsedMsg 로 맞는 대상 가운데에 스킬 이펙트 플립북 (Resources/Visuals/Effects)
//  - 스냅샷 상태이상이 새로 걸리거나 늘면 발밑에 Status_<종류> 플립북 + 그 색으로 잠깐 물들임
// 머리 위 행동 예고 / 발밑 체력바 / 상태이상 / 대상 고르기 칸은 언리얼처럼 전투 HUD(UCombatHUDWidget 의 TagLayer)가 UI 로 그림.
// 이 클래스는 그 자리를 알려 줌 (TryGetMonsterBounds / TryGetPlayerBounds / PickAt = 화면 좌표로 스프라이트 맞추기)
using System.Collections.Generic;
using Terminus.Art;
using Terminus.Data;
using Terminus.Game.Net;
using UnityEngine;

namespace Terminus.Game.Client
{
    public sealed class AreaView : MonoBehaviour
    {
        // 언리얼 슬롯 (월드 X, 카메라 화면 폭 1300) -> 화면 비율
        const float UnrealViewWidth = 1300f;
        static readonly float[] PlayerSlots = { -520f, -390f, -260f, -130f };
        static readonly float[] MonsterSlots = { 170f, 340f, 510f };

        // 발이 닿는 높이 (화면 아래에서 비율). 아래쪽은 전투 HUD 의 스킬 판 / 체력바가 씀
        public const float GroundViewY = 0.37f;

        // 직업 / 몬스터 그림 배율 (64px = 0.75 유닛). 배경 확인 미리보기(BackdropPreview)도 같은 값
        public const float ActorScale = 0.75f;

        // 피격 번쩍임
        const float FlashSeconds = 0.22f;
        static readonly Color FlashColor = new Color(1f, 0.38f, 0.34f, 1f);
        static readonly Color DeadPlayerColor = new Color(0.55f, 0.55f, 0.6f, 0.45f);

        static float PlayerSlotX(int i) => 0.5f + PlayerSlots[Mathf.Clamp(i, 0, PlayerSlots.Length - 1)] / UnrealViewWidth;
        static float MonsterSlotX(int s) => 0.5f + MonsterSlots[Mathf.Clamp(s, 0, MonsterSlots.Length - 1)] / UnrealViewWidth;

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
            public GameObject Root;       // 발 위치
            public Transform Body;        // 스프라이트 (발이 Root 에 닿게 올려 둠)
            public SpriteRenderer Sprite;
            public FlipbookPlayer Player;
            public MonsterAnimator MonsterAnim;
            public int LastHealth = int.MinValue;
            public bool Dead;
            public float FlashLeft;
            public Color FlashTint = FlashColor;
            public List<StatusInstance> LastStatuses;
        }

        readonly Dictionary<ulong, Actor> _players = new Dictionary<ulong, Actor>();
        readonly Dictionary<int, Actor> _monsters = new Dictionary<int, Actor>();
        int _shownArea = -1;

        /// <summary>지금 무대 (던전 씬에 하나)</summary>
        public static AreaView Instance { get; private set; }

        /// <summary>관전: 이 구역을 대신 보여줌 (-1 = 내 구역). 관전 화면(언리얼 USpectateWidget / ViewDungeonArea)이 정함</summary>
        public static int SpectateArea = -1;

        /// <summary>지금 무대에 보이는 구역 (관전 중이면 그 구역, 아니면 내 구역). 없으면 null</summary>
        public static AreaInfo ShownArea => SpectateArea >= 0 ? ClientState.Areas.Find(SpectateArea) : ClientState.MyArea;

        /// <summary>무대를 비추는 카메라 (전투 HUD 가 월드 -> 화면 변환에 씀)</summary>
        public static Camera ViewCamera => Camera.main;

        public static AreaView Create()
        {
            var go = new GameObject("AreaView");
            return go.AddComponent<AreaView>();
        }

        // ------------------------------------------------------------ 자리 조회 (전투 HUD)

        /// <summary>몬스터 스프라이트의 월드 영역 (MonsterInfo.Id). 무대에 없으면 false</summary>
        public static bool TryGetMonsterBounds(int monsterId, out Bounds bounds)
        {
            bounds = default;
            var view = Instance;
            return view != null && view._monsters.TryGetValue(monsterId, out var a) && GetBounds(a, out bounds);
        }

        /// <summary>플레이어 스프라이트의 월드 영역 (PlayerInfo.Id). 무대에 없으면 false</summary>
        public static bool TryGetPlayerBounds(ulong playerId, out Bounds bounds)
        {
            bounds = default;
            var view = Instance;
            return view != null && view._players.TryGetValue(playerId, out var a) && GetBounds(a, out bounds);
        }

        /// <summary>
        /// 화면 좌표(픽셀, 왼쪽 아래가 0,0 = Camera.ScreenToWorldPoint 기준)에 있는 살아 있는 배틀러.
        /// 몬스터면 isMonster + monsterId(MonsterInfo.Id), 플레이어면 playerId. 겹치면 스프라이트 가운데에 더 가까운 것
        /// </summary>
        public static bool PickAt(Vector2 screenPos, out int monsterId, out ulong playerId, out bool isMonster)
        {
            monsterId = -1;
            playerId = 0;
            isMonster = false;
            var view = Instance;
            var cam = ViewCamera;
            if (view == null || cam == null) return false;

            var world = cam.ScreenToWorldPoint(new Vector3(screenPos.x, screenPos.y, Mathf.Abs(cam.transform.position.z)));
            float best = float.MaxValue;
            bool found = false;
            foreach (var kv in view._monsters)
            {
                if (kv.Value.Dead || !GetBounds(kv.Value, out var b) || !Contains2D(b, world)) continue;
                float d = ((Vector2)(b.center - world)).sqrMagnitude;
                if (d >= best) continue;
                best = d;
                found = true;
                isMonster = true;
                monsterId = kv.Key;
            }
            foreach (var kv in view._players)
            {
                if (kv.Value.Dead || !GetBounds(kv.Value, out var b) || !Contains2D(b, world)) continue;
                float d = ((Vector2)(b.center - world)).sqrMagnitude;
                if (d >= best) continue;
                best = d;
                found = true;
                isMonster = false;
                monsterId = -1;
                playerId = kv.Key;
            }
            return found;
        }

        static bool Contains2D(Bounds b, Vector3 p) => p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y;

        static bool GetBounds(Actor a, out Bounds bounds)
        {
            bounds = default;
            if (a == null || a.Root == null) return false;
            if (a.Sprite != null && a.Sprite.sprite != null)
            {
                bounds = a.Sprite.bounds;
                return true;
            }
            // 그림이 아직 없으면 발 위 1 유닛 칸
            var feet = a.Root.transform.position;
            bounds = new Bounds(feet + new Vector3(0f, 0.5f, 0f), new Vector3(0.8f, 1f, 0.1f));
            return true;
        }

        // ------------------------------------------------------------ 수명

        static bool _hooked;

        void OnEnable()
        {
            Instance = this;
            if (!_hooked)
            {
                // 알림 처리기는 한 번만 붙이고 지금 무대로 넘김 (씬마다 무대가 새로 생겨도 처리기가 쌓이지 않게)
                _hooked = true;
                NetBus.On<MonsterActingMsg>(m => { if (Instance != null) Instance.OnMonsterActing(m); });
                NetBus.On<SkillUsedMsg>(m => { if (Instance != null) Instance.OnSkillUsed(m); });
            }
            ClientState.AreasChanged += Refresh;
            ClientState.PartyChanged += Refresh;
            Refresh();
        }

        void OnDisable()
        {
            ClientState.AreasChanged -= Refresh;
            ClientState.PartyChanged -= Refresh;
            if (Instance == this) Instance = null;
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics()
        {
            _hooked = false;
            Instance = null;
            SpectateArea = -1;
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

        // ------------------------------------------------------------ 스킬 이펙트

        const int EffectSortingOrder = 20;   // 캐릭터(10) 위
        const float EffectScale = 0.8f;      // 캐릭터 배율 대비 (플레이 확인 후 0.8)
        static readonly Dictionary<string, SpriteFlipbook> _effects = new Dictionary<string, SpriteFlipbook>();

        // 스킬 -> 이펙트 이름 (Resources/Visuals/Effects/<이름>). 없으면 null
        // shortcut: 파이터 공격만 연결됨, 다른 묶음은 이펙트를 만들 때 여기에 추가
        static string EffectNameFor(SkillRow skill)
        {
            if (skill == null) return null;
            if (skill.OwnerClass == SkillOwner.Fighter && skill.SkillType == SkillType.Attack) return "FistKi";
            return null;
        }

        static SpriteFlipbook LoadEffect(string name)
        {
            if (!_effects.TryGetValue(name, out var fb))
                _effects[name] = fb = Resources.Load<SpriteFlipbook>("Visuals/Effects/" + name);
            return fb;
        }

        void OnSkillUsed(SkillUsedMsg msg)
        {
            if (this == null || msg.AreaIndex != _shownArea) return;
            var name = !string.IsNullOrEmpty(msg.Effect) ? msg.Effect : EffectNameFor(GameData.FindSkillRow(msg.SkillRow));
            var fb = name != null ? LoadEffect(name) : null;
            if (fb == null) return;
            foreach (var id in msg.MonsterIds)
                if (TryGetMonsterBounds(id, out var b)) SpawnEffect(fb, b.center, ActorScale * EffectScale, FacesLeft(b.center));
            foreach (var id in msg.PlayerIds)
                if (TryGetPlayerBounds(id, out var b)) SpawnEffect(fb, b.center, ActorScale * EffectScale, FacesLeft(b.center));
        }

        // 상태이상 이펙트는 pixelfx 기본 인형(38px) 기준으로 그려짐 -> 우리 그림(64px x ActorScale)에 맞춤
        const float StatusEffectScale = ActorScale * 64f / 38f * 0.8f;

        // 물들일 색 (이펙트 색과 맞춤). 없는 종류는 이펙트만
        static readonly Dictionary<StatusEffect, Color> StatusTints = new Dictionary<StatusEffect, Color>
        {
            { StatusEffect.Poison, new Color(0.45f, 0.9f, 0.35f) },
            { StatusEffect.Ice, new Color(0.5f, 0.8f, 1f) },
            { StatusEffect.Flooding, new Color(0.35f, 0.6f, 1f) },
            { StatusEffect.Lava, new Color(1f, 0.55f, 0.2f) },
            { StatusEffect.InternalInjury, new Color(0.9f, 0.25f, 0.3f) },
            { StatusEffect.Fear, new Color(0.65f, 0.55f, 0.85f) },
            { StatusEffect.Contrarywind, new Color(0.7f, 0.85f, 0.95f) },
            { StatusEffect.Dark, new Color(0.6f, 0.35f, 0.85f) },
            { StatusEffect.Acid, new Color(0.8f, 0.95f, 0.2f) },
            { StatusEffect.Fusion, new Color(0.8f, 0.65f, 1f) },
            { StatusEffect.Mark, new Color(1f, 0.35f, 0.3f) },
            { StatusEffect.Brave, new Color(1f, 0.7f, 0.3f) },
            { StatusEffect.Metal, new Color(0.75f, 0.8f, 0.88f) },
            { StatusEffect.Solid, new Color(1f, 0.88f, 0.45f) },
        };

        static void Flash(Actor a, Color tint)
        {
            a.FlashLeft = FlashSeconds;
            a.FlashTint = tint;
        }

        // 처음 본 스냅샷은 비교만 (들어오자마자 전부 터지지 않게)
        void CheckStatuses(Actor a, List<StatusInstance> now)
        {
            var prev = a.LastStatuses;
            a.LastStatuses = now.ConvertAll(s => s.Clone());
            if (prev == null || a.Root == null) return;
            foreach (var s in now)
            {
                if (s.Type == StatusEffect.None || s.Type == StatusEffect.SelfDamage) continue;
                var old = prev.Find(x => x.Type == s.Type);
                if (old != null && s.Value <= old.Value && s.Duration <= old.Duration) continue;
                var fb = LoadEffect("Status_" + s.Type);
                if (fb != null) SpawnEffect(fb, a.Root.transform.position, StatusEffectScale);
                if (StatusTints.TryGetValue(s.Type, out var tint)) Flash(a, tint);
            }
        }

        // 타격 이펙트는 오른쪽으로 치는 그림 -> 화면 왼쪽(보통 플레이어) 대상은 뒤집어 맞은편에서 날아오게
        static bool FacesLeft(Vector3 target)
        {
            var cam = Camera.main;
            return cam != null && target.x < cam.transform.position.x;
        }

        void SpawnEffect(SpriteFlipbook fb, Vector3 at, float scale, bool flipX = false)
        {
            var go = new GameObject("FX_" + fb.name);
            go.transform.SetParent(transform, false);
            go.transform.position = at;
            go.transform.localScale = Vector3.one * scale;
            var sr = go.AddComponent<SpriteRenderer>();
            sr.sortingOrder = EffectSortingOrder;
            sr.flipX = flipX;
            go.AddComponent<FlipbookPlayer>().Play(fb, false, () => Destroy(go));
        }

        void Update()
        {
            // 관전 구역을 바꾼 경우를 스냅샷(0.1초)까지 안 기다리게 매 프레임 구역 번호만 봄
            var area = ShownArea;
            int index = area != null && area.bInUse ? area.AreaIndex : -1;
            if (index != _shownArea) Refresh();

            float dt = Time.deltaTime;
            foreach (var a in _players.Values) Tick(a, dt, false);
            foreach (var a in _monsters.Values) Tick(a, dt, true);
        }

        static void Tick(Actor a, float dt, bool monster)
        {
            if (a.Root == null) return;

            // 스프라이트 피벗이 가운데 -> 발이 Root 에 닿게 (그림 크기가 몬스터마다 달라서 매번)
            if (a.Sprite != null && a.Sprite.sprite != null)
            {
                float lift = -a.Sprite.sprite.bounds.min.y * ActorScale;
                if (Mathf.Abs(a.Body.localPosition.y - lift) > 0.0001f) a.Body.localPosition = new Vector3(0f, lift, 0f);
            }

            if (a.FlashLeft > 0f) a.FlashLeft = Mathf.Max(0f, a.FlashLeft - dt);
            ApplyTint(a, monster);
        }

        static void ApplyTint(Actor a, bool monster)
        {
            if (a.Sprite == null) return;
            var baseColor = !monster && a.Dead ? DeadPlayerColor : Color.white;
            float t = a.FlashLeft / FlashSeconds;
            var c = Color.Lerp(baseColor, a.FlashTint, t * t);
            c.a = baseColor.a;
            a.Sprite.color = c;
        }

        // ------------------------------------------------------------ 스냅샷 -> 무대

        void Refresh()
        {
            if (this == null) return;
            var area = ShownArea;
            int areaIndex = area != null && area.bInUse ? area.AreaIndex : -1;
            if (areaIndex != _shownArea)
            {
                ClearAll();
                _shownArea = areaIndex;
            }
            if (_shownArea < 0) return;

            RefreshPlayers(area);
            RefreshMonsters(area);
        }

        void RefreshPlayers(AreaInfo area)
        {
            var seen = new HashSet<ulong>();
            int slot = 0;
            for (int i = 0; i < area.Occupants.Count; ++i)
            {
                var id = area.Occupants[i];
                var info = ClientState.Party.Find(id);
                if (info == null) continue;
                seen.Add(id);

                if (!_players.TryGetValue(id, out var actor))
                {
                    actor = MakeActor("P_" + info.Name);
                    var visual = Resources.Load<MonsterVisualSet>("Visuals/Characters/" + info.Class);
                    if (visual != null && visual.Idle != null) actor.Player.Play(visual.Idle, true);
                    _players[id] = actor;
                }

                // 배신자는 맞은편 몬스터 0번 자리에서 왼쪽을 봄. 나머지는 왼쪽부터 차례로 (팀 먼저, 배신자 마지막 = 전투 대상 순서)
                bool betrayer = area.bBetrayal && id == area.BetrayerId;
                float x = betrayer ? MonsterSlotX(0) : PlayerSlotX(slot++);
                actor.Root.transform.position = ViewToWorld(x, GroundViewY);
                actor.Sprite.flipX = betrayer;   // 직업 그림은 오른쪽을 봄

                int hp = info.Combat.Health;
                if (actor.LastHealth != int.MinValue && hp < actor.LastHealth) Flash(actor, FlashColor);
                actor.LastHealth = hp;
                CheckStatuses(actor, info.Combat.Statuses);
                actor.Dead = info.bHasBattler && hp <= 0;
                ApplyTint(actor, false);
            }
            RemoveMissing(_players, seen);
        }

        void RefreshMonsters(AreaInfo area)
        {
            var seen = new HashSet<int>();
            foreach (var m in area.Monsters)
            {
                seen.Add(m.Id);
                int hp = m.State.Health;
                if (!_monsters.TryGetValue(m.Id, out var actor))
                {
                    actor = MakeActor("M_" + m.Name);
                    var visual = MonsterVisualSet.LoadBySource(m.VisualSource) ?? MonsterVisualSet.LoadBySource("slime_blue");
                    actor.MonsterAnim = actor.Body.gameObject.AddComponent<MonsterAnimator>();
                    if (visual != null) actor.MonsterAnim.SetVisual(visual);
                    _monsters[m.Id] = actor;

                    // 이미 죽은 채로 처음 보면 (관전 / 늦게 들어옴) 그림 없이
                    if (hp <= 0)
                    {
                        actor.Dead = true;
                        actor.Sprite.enabled = false;
                    }
                }

                actor.Root.transform.position = ViewToWorld(MonsterSlotX(m.Slot), GroundViewY);

                if (actor.LastHealth != int.MinValue && hp < actor.LastHealth)
                {
                    Flash(actor, FlashColor);
                    actor.MonsterAnim?.PlayAction(MonsterAction.Hit);
                }
                if (hp <= 0 && !actor.Dead)
                {
                    // 죽는 한 대를 맞으면 피격 다음에 사망 (MonsterAnimator 가 줄을 세움)
                    actor.Dead = true;
                    actor.MonsterAnim?.PlayAction(MonsterAction.Death);
                }
                else if (hp > 0 && actor.Dead)
                {
                    // 부활 (고대의 슬라임 코어 등): 다시 그림
                    actor.Dead = false;
                    actor.Sprite.enabled = true;
                    var visual = actor.MonsterAnim != null ? actor.MonsterAnim.Visual : null;
                    if (visual != null) actor.MonsterAnim.SetVisual(visual);
                }
                actor.LastHealth = hp;
                if (!actor.Dead) CheckStatuses(actor, m.State.Statuses);
            }
            RemoveMissing(_monsters, seen);
        }

        Actor MakeActor(string name)
        {
            var root = new GameObject(name);
            root.transform.SetParent(transform, false);

            var body = new GameObject("Body");
            body.transform.SetParent(root.transform, false);
            body.transform.localScale = new Vector3(ActorScale, ActorScale, 1f);
            var sr = body.AddComponent<SpriteRenderer>();
            sr.sortingOrder = 10;
            var player = body.AddComponent<FlipbookPlayer>();

            return new Actor { Root = root, Body = body.transform, Sprite = sr, Player = player };
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
