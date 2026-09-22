import { describe, it, expect, vi } from 'vitest';
import { Api } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { ChallengesPage } from '../../src/pages/challenges/challenges-page';
import { SessionService } from '../../src/services/session-service';

describe('challenges page', () => {
  it('creatorName resolves usernames and falls back to the creator ID', () => {
    const session = new SessionService();
    const page = new ChallengesPage(new Api(new HttpClient(session)), session);

    page.users = [{ id: 'alice', username: 'Alice' }, { id: 'bob' }, { username: 'Missing ID' }];

    for (const [id, expected] of [
      ['alice', 'Alice'],
      ['bob', 'bob'],
      ['unknown', 'unknown'],
      [undefined, '作成者不明']
    ]) {
      expect(page.creatorName(id)).toBe(expected);
    }
  });

  it('refresh keeps challenges available when the user list fails', async () => {
    for (const usersAvailable of [true, false]) {
      const session = new SessionService();
      const api = new Api(new HttpClient(session));
      const page = new ChallengesPage(api, session);

      vi.spyOn(api, 'challenges').mockResolvedValue([{ id: 1, creator_id: 'alice' }]);
      vi.spyOn(api, 'users').mockImplementation(async () => {
        if (!usersAvailable) {
          throw new Error('User list unavailable');
        }

        return [{ id: 'alice', username: 'Alice' }];
      });

      await page.refresh();

      expect(page.items).toEqual([{ id: 1, creator_id: 'alice' }]);
      expect(page.creatorName('alice')).toBe(usersAvailable ? 'Alice' : 'alice');
      expect(page.error).toBe('');
    }
  });
});
