// 몬스터 동작 재생 (언리얼 ATerminusMonster 의 DT_MonsterVisual 플립북 직접 재생 부분)
// 전투 쪽은 PlayAction 만 부른다. 죽는 한 대를 맞으면 피격 다음에 사망 (언리얼 e35d293)
using UnityEngine;

namespace Terminus.Art
{
    [RequireComponent(typeof(FlipbookPlayer))]
    public sealed class MonsterAnimator : MonoBehaviour
    {
        [SerializeField] MonsterVisualSet _visual;

        FlipbookPlayer _player;
        SpriteRenderer _renderer;
        bool _dead;
        bool _deathQueued;

        public MonsterVisualSet Visual => _visual;

        void Awake()
        {
            _player = GetComponent<FlipbookPlayer>();
            _renderer = GetComponent<SpriteRenderer>();
            if (_visual != null) SetVisual(_visual);
        }

        public void SetVisual(MonsterVisualSet visual)
        {
            if (_player == null) Awake();
            _visual = visual;
            _dead = false;
            _deathQueued = false;
            if (_renderer != null)
            {
                _renderer.enabled = true;
                // 그림이 왼쪽을 보면 그대로, 오른쪽을 보면 뒤집어 왼쪽을 보게
                _renderer.flipX = visual != null && !visual.FacesLeft;
            }
            PlayIdle();
        }

        public void PlayIdle()
        {
            if (_dead || _visual == null) return;
            _player.Play(_visual.Idle, true);
        }

        public void PlayAction(MonsterAction action)
        {
            if (_dead || _visual == null) return;

            if (action == MonsterAction.Idle)
            {
                PlayIdle();
                return;
            }

            if (action == MonsterAction.Death)
            {
                // 피격 재생 중이면 그게 끝난 다음에 사망
                if (_player.IsPlaying && _player.Current == _visual.Hit)
                {
                    _deathQueued = true;
                    return;
                }
                PlayDeath();
                return;
            }

            var flipbook = _visual.Get(action);
            if (flipbook == null)
            {
                PlayIdle();
                return;
            }
            _player.Play(flipbook, false, OnOneShotFinished);
        }

        void OnOneShotFinished()
        {
            if (_deathQueued)
            {
                _deathQueued = false;
                PlayDeath();
                return;
            }
            PlayIdle();
        }

        void PlayDeath()
        {
            _dead = true;
            if (_visual.Death == null)
            {
                if (_renderer != null) _renderer.enabled = false;
                return;
            }
            _player.Play(_visual.Death, false, () =>
            {
                if (_renderer != null) _renderer.enabled = false;
            });
        }
    }
}
