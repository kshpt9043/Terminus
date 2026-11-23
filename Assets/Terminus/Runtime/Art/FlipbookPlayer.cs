// SpriteRenderer 에 플립북을 재생 (언리얼 UPaperFlipbookComponent + TerminusMonster 의 동작 재생 규칙)
//  - 대기: 반복
//  - 공격 / 방어 / 스페셜 / 피격: 한 번 재생 후 대기로
//  - 사망: 마지막 프레임에서 멈췄다가 그림을 끔 (언리얼 HideAfterDeath)
using System;
using UnityEngine;

namespace Terminus.Art
{
    [RequireComponent(typeof(SpriteRenderer))]
    public sealed class FlipbookPlayer : MonoBehaviour
    {
        [SerializeField] SpriteFlipbook _flipbook;
        [SerializeField] bool _loop = true;
        [SerializeField] float _playRate = 1f;

        SpriteRenderer _renderer;
        float _time;
        bool _playing;
        Action _onFinished;

        public SpriteFlipbook Current => _flipbook;
        public bool IsPlaying => _playing;

        void Awake()
        {
            _renderer = GetComponent<SpriteRenderer>();
            if (_flipbook != null) Play(_flipbook, _loop);
        }

        /// <summary>처음부터 재생. loop 가 false 면 끝에서 onFinished 를 한 번 부르고 마지막 프레임에 멈춤</summary>
        public void Play(SpriteFlipbook flipbook, bool loop, Action onFinished = null)
        {
            if (_renderer == null) _renderer = GetComponent<SpriteRenderer>();
            _flipbook = flipbook;
            _loop = loop;
            _onFinished = onFinished;
            _time = 0f;
            _playing = flipbook != null && flipbook.FrameCount > 0;
            ApplyFrame();
        }

        public void Stop()
        {
            _playing = false;
            _onFinished = null;
        }

        void Update()
        {
            if (!_playing || _flipbook == null) return;

            _time += Time.deltaTime * _playRate;
            float duration = _flipbook.Duration;
            if (duration <= 0f) return;

            if (_time >= duration)
            {
                if (_loop)
                {
                    _time %= duration;
                }
                else
                {
                    _time = duration;
                    _playing = false;
                    ApplyFrame();
                    var done = _onFinished;
                    _onFinished = null;
                    done?.Invoke();
                    return;
                }
            }
            ApplyFrame();
        }

        void ApplyFrame()
        {
            if (_renderer == null || _flipbook == null) return;
            int index = Mathf.FloorToInt(_time * _flipbook.FramesPerSecond);
            _renderer.sprite = _flipbook.GetFrame(index);
        }
    }
}
