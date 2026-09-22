import { afterEach, describe, expect, it, vi } from 'vitest';
import { contestState } from '../../src/services/contest';
import { Api } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { SessionService } from '../../src/services/session-service';
import { ChallengesPage } from '../../src/pages/challenges/challenges-page';
import { DetailPage } from '../../src/pages/detail/detail-page';

afterEach(() => {
  vi.unstubAllEnvs();
  vi.useRealTimers();
});

describe('contest start', () => {
  it('contestState handles unset, invalid, offset and boundary timestamps', () => {
    const now = Date.parse('2026-10-01T01:00:00Z');

    for (const [value, started] of [
      ['', true],
      ['invalid', false],
      ['2026-10-01T10:00:00', false],
      ['2026-10-01T10:00:01+09:00', false],
      ['2026-10-01T10:00:00+09:00', true],
      ['2026-10-01T00:59:59Z', true]
    ] as const) {
      expect(contestState(value, now).started, value).toBe(started);
    }
  });

  it('refresh skips requests before start and opens the list when start arrives', async () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-10-01T00:59:59Z'));
    vi.stubEnv('VITE_CONTEST_START_AT', '2026-10-01T10:00:00+09:00');

    const session = new SessionService();
    const api = new Api(new HttpClient(session));
    const challenges = vi.spyOn(api, 'challenges').mockResolvedValue([{ id: 1 }]);
    const users = vi.spyOn(api, 'users').mockResolvedValue([]);
    const page = new ChallengesPage(api, session);
    const detail = new DetailPage(api, session);

    await page.loading();
    page.attached();

    expect(challenges).not.toHaveBeenCalled();
    expect(users).not.toHaveBeenCalled();
    expect(page.contest.started).toBe(false);
    expect(detail.canLoad()).toBe('challenges');

    await vi.advanceTimersByTimeAsync(1000);

    expect(page.contest.started).toBe(true);
    expect(page.items).toEqual([{ id: 1 }]);
    expect(challenges).toHaveBeenCalledOnce();
    expect(detail.canLoad()).toBe(true);

    page.unbinding();

    expect(vi.getTimerCount()).toBe(0);
  });
});
