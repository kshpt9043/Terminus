// 테마 끝 층 선택 (다음 층 / 탈출 / 배신) 투표 상태 기계 (언리얼 AMapManager::StartFloorVote, CastFloorVote,
// EndFloorVoting, ResolveFloorVote, ApplyFloorChoice). 언리얼 타이머(FTimerHandle) 대신 서버 시각을 넘겨받는 Tick(now)
// 플레이어는 ulong 번호 (NGO ClientId 등). 복제 / 화면 갱신은 Changed 이벤트를 받은 Game 쪽이
using System;
using System.Collections.Generic;

namespace Terminus.Map
{
    public sealed class FloorVote
    {
        readonly MapSettings _settings;
        readonly Random _rng;

        // 투표 진행 (서버). 넣은 순서대로 (언리얼 TMap<TWeakObjectPtr<APlayerState>, EFloorChoice> FloorVotes)
        readonly List<FloorVoteEntry> _votes = new List<FloorVoteEntry>();

        enum PendingTimer { None, EndVoting, Resolve, Apply }
        PendingTimer _timer = PendingTimer.None;
        double _timerAt = 0;

        /// <summary>모두에게 복제되는 투표 상태 (서버 원본). 밖으로 넘길 땐 Clone</summary>
        public FloorVoteState State { get; private set; } = new FloorVoteState();

        public bool IsActive => State.Phase != FloorVotePhase.None;

        /// <summary>투표 상태가 바뀜 (언리얼 NotifyVoteChanged -> OnRep_VoteState). 넘기는 값은 사본</summary>
        public event Action<FloorVoteState> Changed;

        /// <summary>결과를 보여 준 뒤 적용할 때 (언리얼 ApplyFloorChoice). 이때 Phase 는 이미 None (투표 화면 닫힘)</summary>
        public event Action<FloorChoice> Applied;

        /// <summary>다음 타이머가 울리는 서버 시각. 없으면 null (Game 쪽이 Tick 을 언제 부를지 정할 때)</summary>
        public double? NextTimerTime => _timer == PendingTimer.None ? (double?)null : _timerAt;

        public FloorVote(MapSettings settings, Random rng)
        {
            _settings = settings ?? new MapSettings();
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        /// <summary>서버만 아는 표 (누가 무엇을). 공개 전에는 복제하면 안 됨</summary>
        public IReadOnlyList<FloorVoteEntry> Votes => _votes;

        /// <summary>배신을 고른 사람 (선착순 1명). 없으면 null</summary>
        public ulong? BetrayerId
        {
            get
            {
                ulong? betrayer = null;
                foreach (var v in _votes)
                {
                    if (v.Choice == FloorChoice.Betray) betrayer = v.PlayerId;
                }
                return betrayer;
            }
        }

        /// <summary>
        /// 투표 시작. 돌려주는 글은 전원에게 보낼 시스템 채팅
        /// totalVoters = 지금 접속한 인원 (언리얼 GameState PlayerArray 수, 최소 1)
        /// </summary>
        public string Start(int floor, int lastFloor, bool bSinglePlayerRun, int totalVoters, double now)
        {
            _votes.Clear();
            int nextId = State.VoteId + 1;
            State = new FloorVoteState
            {
                VoteId = nextId,
                Phase = FloorVotePhase.Voting,
                Floor = floor,
            };

            // 마지막 층(콘텐츠 끝)이면 다음 층 없음. 배신은 멀티만
            if (floor < lastFloor) State.Options.Add(FloorChoice.NextFloor);
            State.Options.Add(FloorChoice.Escape);
            if (!bSinglePlayerRun) State.Options.Add(FloorChoice.Betray);

            for (int i = 0; i < State.Options.Count; ++i) State.Counts.Add(0);
            State.TotalVoters = Math.Max(1, totalVoters);
            State.PhaseEndTime = now + _settings.VoteSeconds;

            SetTimer(PendingTimer.EndVoting, now + _settings.VoteSeconds);

            string notice = bSinglePlayerRun
                ? $"{floor}층 보스를 쓰러뜨렸습니다. 다음 행선지를 고르세요."
                : $"{floor}층 보스를 쓰러뜨렸습니다. {(int)Math.Round(_settings.VoteSeconds, MidpointRounding.AwayFromZero)}초 동안 다음 행선지를 투표합니다.";

            NotifyVoteChanged();
            return notice;
        }

        /// <summary>[서버] 표 넣기 (PC 의 Server_CastFloorVote). 한 사람 한 번, 배신은 선착순 1명. 거절하면 false</summary>
        public bool Cast(ulong voter, FloorChoice choice, double now)
        {
            if (State.Phase != FloorVotePhase.Voting) return false;
            if (_votes.Exists(v => v.PlayerId == voter)) return false;   // 한 번만

            int index = State.Options.IndexOf(choice);
            if (index < 0) return false;

            // 배신은 선착순 1명
            if (choice == FloorChoice.Betray && State.Counts[index] > 0) return false;

            _votes.Add(new FloorVoteEntry { PlayerId = voter, Choice = choice });
            ++State.Counts[index];
            ++State.VotedCount;

            // 배신이 나오면 바로 끝 (그 1명 vs 나머지), 전원 골랐으면 공개로
            if (choice == FloorChoice.Betray || State.VotedCount >= State.TotalVoters)
            {
                EndFloorVoting(now);
            }
            else
            {
                NotifyVoteChanged();
            }
            return true;
        }

        /// <summary>
        /// 서버 시각을 넘겨 타이머를 돌림 (언리얼 GetWorldTimerManager 자리). 매 프레임 불러도 됨
        /// 한 번에 한 단계만 넘어감 (다음 단계 타이머는 지금 시각부터 다시 잼. 언리얼 타이머와 같음)
        /// </summary>
        public void Tick(double now)
        {
            if (_timer == PendingTimer.None || now < _timerAt) return;

            var fired = _timer;
            _timer = PendingTimer.None;

            switch (fired)
            {
                case PendingTimer.EndVoting: EndFloorVoting(now); break;
                case PendingTimer.Resolve: ResolveFloorVote(now); break;
                case PendingTimer.Apply: ApplyFloorChoice(); break;
            }
        }

        void EndFloorVoting(double now)
        {
            if (State.Phase != FloorVotePhase.Voting) return;
            ClearTimer();

            // 혼자면 공개할 게 없음
            if (State.TotalVoters <= 1)
            {
                ResolveFloorVote(now);
                return;
            }

            State.Phase = FloorVotePhase.Revealing;
            State.PhaseEndTime = now + _settings.RevealSeconds;
            SetTimer(PendingTimer.Resolve, now + _settings.RevealSeconds);
            NotifyVoteChanged();
        }

        void ResolveFloorVote(double now)
        {
            ClearTimer();

            // 누가 무엇을 골랐는지 공개
            State.RevealedVotes.Clear();
            bool bBetrayed = false;
            foreach (var v in _votes)
            {
                State.RevealedVotes.Add(v.Clone());
                bBetrayed |= v.Choice == FloorChoice.Betray;
            }

            // 결과: 배신이 있으면 배신. 아니면 다수결, 동점이면 그중 랜덤 (아무도 안 골랐으면 배신 빼고 랜덤)
            FloorChoice result = FloorChoice.None;
            if (bBetrayed)
            {
                result = FloorChoice.Betray;
            }
            else
            {
                int best = -1;
                var tied = new List<FloorChoice>();
                for (int i = 0; i < State.Options.Count; ++i)
                {
                    if (State.Options[i] == FloorChoice.Betray) continue;

                    int count = i < State.Counts.Count ? State.Counts[i] : 0;
                    if (count > best)
                    {
                        best = count;
                        tied.Clear();
                    }
                    if (count == best)
                    {
                        tied.Add(State.Options[i]);
                    }
                }
                if (tied.Count > 0)
                {
                    result = tied[MapGenerator.RandRange(_rng, 0, tied.Count - 1)];
                }
            }

            State.Result = result;
            State.Phase = FloorVotePhase.Done;
            State.PhaseEndTime = now + _settings.ResultSeconds;
            NotifyVoteChanged();

            // 결과를 잠깐 보여주고 적용 (혼자면 바로)
            double delay = State.TotalVoters <= 1 ? _settings.SingleResultSeconds : _settings.ResultSeconds;
            SetTimer(PendingTimer.Apply, now + delay);
        }

        void ApplyFloorChoice()
        {
            FloorChoice result = State.Result;

            // 투표 화면 닫기
            State.Phase = FloorVotePhase.None;
            NotifyVoteChanged();

            Applied?.Invoke(result);
        }

        void NotifyVoteChanged()
        {
            Changed?.Invoke(State.Clone());
        }

        void SetTimer(PendingTimer timer, double at)
        {
            _timer = timer;
            _timerAt = at;
        }

        void ClearTimer()
        {
            _timer = PendingTimer.None;
        }
    }
}
