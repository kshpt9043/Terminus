// 테마 끝 층 행선지 투표 상태 기계 (언리얼 StartFloorVote ~ ApplyFloorChoice). 시간은 직접 넘김
using System;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;
using Terminus.Map;

namespace Terminus.Tests
{
    public class FloorVoteTests
    {
        MapSettings _settings;
        FloorVote _vote;
        List<FloorVoteState> _changes;
        List<FloorChoice> _applied;

        [SetUp]
        public void SetUp()
        {
            _settings = new MapSettings();
            _vote = new FloorVote(_settings, new Random(1));
            _changes = new List<FloorVoteState>();
            _applied = new List<FloorChoice>();
            _vote.Changed += s => _changes.Add(s);
            _vote.Applied += c => _applied.Add(c);
        }

        [Test]
        public void Start_Multi_OptionsAndTimer()
        {
            string notice = _vote.Start(2, 6, false, 3, 100.0);

            var s = _vote.State;
            Assert.That(s.Phase, Is.EqualTo(FloorVotePhase.Voting));
            Assert.That(s.VoteId, Is.EqualTo(1));
            Assert.That(s.Floor, Is.EqualTo(2));
            Assert.That(s.Options, Is.EqualTo(new[] { FloorChoice.NextFloor, FloorChoice.Escape, FloorChoice.Betray }));
            Assert.That(s.Counts, Is.EqualTo(new[] { 0, 0, 0 }));
            Assert.That(s.TotalVoters, Is.EqualTo(3));
            Assert.That(s.PhaseEndTime, Is.EqualTo(115.0));
            Assert.That(notice, Is.EqualTo("2층 보스를 쓰러뜨렸습니다. 15초 동안 다음 행선지를 투표합니다."));
            Assert.That(_vote.IsActive);
            Assert.That(_changes, Has.Count.EqualTo(1));
        }

        [Test]
        public void Start_LastFloor_NoNextFloor()
        {
            _vote.Start(6, 6, false, 2, 0);
            Assert.That(_vote.State.Options, Is.EqualTo(new[] { FloorChoice.Escape, FloorChoice.Betray }));
            Assert.That(_vote.Cast(1, FloorChoice.NextFloor, 1), Is.False);
        }

        [Test]
        public void NormalVote_AllVoted_RevealThenResultThenApply()
        {
            _vote.Start(2, 6, false, 3, 0);

            Assert.That(_vote.Cast(10, FloorChoice.NextFloor, 1), Is.True);
            Assert.That(_vote.Cast(10, FloorChoice.Escape, 1), Is.False, "한 사람 한 번");
            Assert.That(_vote.Cast(11, FloorChoice.NextFloor, 2), Is.True);
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Voting));
            Assert.That(_vote.State.VotedCount, Is.EqualTo(2));
            Assert.That(_vote.State.RevealedVotes, Is.Empty, "공개 전엔 누가 골랐는지 비어 있음");

            Assert.That(_vote.Cast(12, FloorChoice.Escape, 3), Is.True);

            // 전원 골랐음 -> 공개 타이머
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Revealing));
            Assert.That(_vote.State.PhaseEndTime, Is.EqualTo(3 + _settings.RevealSeconds));
            Assert.That(_vote.State.Counts, Is.EqualTo(new[] { 2, 1, 0 }));
            Assert.That(_vote.State.RevealedVotes, Is.Empty);
            Assert.That(_vote.Cast(13, FloorChoice.Escape, 3), Is.False, "공개 중엔 못 넣음");

            _vote.Tick(7.9);
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Revealing), "아직 시간 안 됨");

            _vote.Tick(8.0);
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Done));
            Assert.That(_vote.State.Result, Is.EqualTo(FloorChoice.NextFloor), "다수결");
            Assert.That(_vote.State.PhaseEndTime, Is.EqualTo(8.0 + _settings.ResultSeconds));
            Assert.That(_vote.State.RevealedVotes.Select(v => (v.PlayerId, v.Choice)),
                Is.EqualTo(new[] { (10UL, FloorChoice.NextFloor), (11UL, FloorChoice.NextFloor), (12UL, FloorChoice.Escape) }));
            Assert.That(_applied, Is.Empty);

            _vote.Tick(10.9);
            Assert.That(_applied, Is.Empty);
            _vote.Tick(11.0);
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.None), "투표 화면 닫힘");
            Assert.That(_vote.IsActive, Is.False);
            Assert.That(_applied, Is.EqualTo(new[] { FloorChoice.NextFloor }));
            Assert.That(_vote.NextTimerTime, Is.Null);

            // 단계마다 알림 (시작, 표 2, 공개, 결과, 닫힘)
            Assert.That(_changes.Select(c => c.Phase), Is.EqualTo(new[]
            {
                FloorVotePhase.Voting, FloorVotePhase.Voting, FloorVotePhase.Voting,
                FloorVotePhase.Revealing, FloorVotePhase.Done, FloorVotePhase.None,
            }));
        }

        [Test]
        public void ChangedPassesSnapshot()
        {
            _vote.Start(2, 6, false, 3, 0);
            var first = _changes[0];
            _vote.Cast(1, FloorChoice.Escape, 1);
            Assert.That(first.VotedCount, Is.EqualTo(0), "넘긴 상태는 사본");
        }

        [Test]
        public void Timeout_NoVotes_RandomAmongNonBetray()
        {
            var results = new HashSet<FloorChoice>();
            for (int seed = 0; seed < 40; ++seed)
            {
                var vote = new FloorVote(_settings, new Random(seed));
                FloorChoice applied = FloorChoice.None;
                vote.Applied += c => applied = c;

                vote.Start(4, 6, false, 3, 0);
                vote.Tick(14.99);
                Assert.That(vote.State.Phase, Is.EqualTo(FloorVotePhase.Voting));
                vote.Tick(15);
                Assert.That(vote.State.Phase, Is.EqualTo(FloorVotePhase.Revealing), "시간이 다 되면 공개");
                vote.Tick(20);
                Assert.That(vote.State.Phase, Is.EqualTo(FloorVotePhase.Done));
                Assert.That(vote.State.RevealedVotes, Is.Empty);
                vote.Tick(23);

                Assert.That(new[] { FloorChoice.NextFloor, FloorChoice.Escape }, Does.Contain(applied), "아무도 안 골랐으면 배신 빼고 랜덤");
                results.Add(applied);
            }
            Assert.That(results, Is.EquivalentTo(new[] { FloorChoice.NextFloor, FloorChoice.Escape }));
        }

        [Test]
        public void Timeout_PartialVotes_Majority()
        {
            _vote.Start(2, 6, false, 4, 0);
            _vote.Cast(1, FloorChoice.Escape, 1);
            _vote.Tick(15);
            _vote.Tick(20);
            Assert.That(_vote.State.Result, Is.EqualTo(FloorChoice.Escape));
            Assert.That(_vote.State.VotedCount, Is.EqualTo(1));
        }

        [Test]
        public void Tie_RandomAmongTied()
        {
            var results = new HashSet<FloorChoice>();
            for (int seed = 0; seed < 40; ++seed)
            {
                var vote = new FloorVote(_settings, new Random(seed));
                vote.Start(2, 6, false, 2, 0);
                vote.Cast(1, FloorChoice.NextFloor, 0);
                vote.Cast(2, FloorChoice.Escape, 0);
                vote.Tick(5);
                results.Add(vote.State.Result);
            }
            Assert.That(results, Is.EquivalentTo(new[] { FloorChoice.NextFloor, FloorChoice.Escape }));
        }

        [Test]
        public void Betray_FirstComeOnly_EndsVotingImmediately()
        {
            _vote.Start(2, 6, false, 4, 0);
            Assert.That(_vote.Cast(1, FloorChoice.NextFloor, 1), Is.True);
            Assert.That(_vote.Cast(2, FloorChoice.Betray, 2), Is.True);

            // 배신이 나오면 바로 끝 (그 1명 vs 나머지)
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Revealing));
            Assert.That(_vote.State.PhaseEndTime, Is.EqualTo(2 + _settings.RevealSeconds));
            Assert.That(_vote.Cast(3, FloorChoice.Betray, 2), Is.False, "배신은 선착순 1명");
            Assert.That(_vote.Cast(4, FloorChoice.Escape, 2), Is.False, "끝난 뒤엔 못 넣음");
            Assert.That(_vote.BetrayerId, Is.EqualTo(2UL));

            _vote.Tick(7);
            Assert.That(_vote.State.Result, Is.EqualTo(FloorChoice.Betray), "배신이 있으면 배신 (다수결보다 먼저)");
            _vote.Tick(10);
            Assert.That(_applied, Is.EqualTo(new[] { FloorChoice.Betray }));
        }

        [Test]
        public void SinglePlayer_NoBetray_SkipsReveal_ShortResult()
        {
            string notice = _vote.Start(2, 6, true, 1, 50);
            Assert.That(_vote.State.Options, Is.EqualTo(new[] { FloorChoice.NextFloor, FloorChoice.Escape }));
            Assert.That(notice, Is.EqualTo("2층 보스를 쓰러뜨렸습니다. 다음 행선지를 고르세요."));
            Assert.That(_vote.Cast(1, FloorChoice.Betray, 51), Is.False, "배신은 멀티만");

            Assert.That(_vote.Cast(1, FloorChoice.Escape, 52), Is.True);
            // 혼자면 공개할 게 없음 -> 바로 결과
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Done));
            Assert.That(_vote.State.Result, Is.EqualTo(FloorChoice.Escape));
            Assert.That(_changes.Any(c => c.Phase == FloorVotePhase.Revealing), Is.False);

            // 결과를 보여주는 시간은 0.5초 (화면의 PhaseEndTime 은 ResultSeconds 기준 - 언리얼 그대로)
            _vote.Tick(52.4);
            Assert.That(_applied, Is.Empty);
            _vote.Tick(52.5);
            Assert.That(_applied, Is.EqualTo(new[] { FloorChoice.Escape }));
        }

        [Test]
        public void SinglePlayer_Timeout_ResolvesWithoutReveal()
        {
            _vote.Start(1, 6, true, 1, 0);
            _vote.Tick(15);
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Done));
            Assert.That(new[] { FloorChoice.NextFloor, FloorChoice.Escape }, Does.Contain(_vote.State.Result));
        }

        [Test]
        public void TotalVotersAtLeastOne()
        {
            _vote.Start(2, 6, false, 0, 0);
            Assert.That(_vote.State.TotalVoters, Is.EqualTo(1));
        }

        [Test]
        public void NewVote_IncrementsIdAndResets()
        {
            _vote.Start(2, 6, false, 2, 0);
            _vote.Cast(1, FloorChoice.Escape, 0);
            _vote.Start(4, 6, false, 2, 100);
            Assert.That(_vote.State.VoteId, Is.EqualTo(2));
            Assert.That(_vote.State.VotedCount, Is.EqualTo(0));
            Assert.That(_vote.Votes, Is.Empty);
            Assert.That(_vote.Cast(1, FloorChoice.Escape, 101), Is.True, "새 투표에선 다시 넣을 수 있음");
        }

        [Test]
        public void Tick_OneStagePerCall()
        {
            _vote.Start(2, 6, false, 2, 0);
            _vote.Tick(1000);   // 늦게 불러도 한 단계씩
            Assert.That(_vote.State.Phase, Is.EqualTo(FloorVotePhase.Revealing));
            Assert.That(_vote.State.PhaseEndTime, Is.EqualTo(1005));
        }

        [Test]
        public void Cast_WhenNoVote_Rejected()
        {
            Assert.That(_vote.Cast(1, FloorChoice.Escape, 0), Is.False);
            Assert.That(_vote.IsActive, Is.False);
        }
    }
}
