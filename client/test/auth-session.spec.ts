import { describe, it, expect } from 'vitest';
import { Api } from '../src/api/api';
import { HttpClient } from '../src/api/http-client';
import { MyApp } from '../src/my-app';
import { SessionService } from '../src/services/session-service';

describe('authentication lifecycle', () => {
  it('logout handles success, invalid JWT, server failure and concurrent login', async () => {
    for (const outcome of [200, 401, 500, 'network', 'new-login'] as const) {
      const session = new SessionService();

      session.start('old-jwt');

      const api = new Api(
        new HttpClient(session, async () => {
          if (outcome === 'network') {
            throw new Error('offline');
          }

          if (outcome === 'new-login') {
            session.start('new-jwt');
          }

          return new Response(null, { status: typeof outcome === 'number' ? outcome : 200 });
        })
      );
      const app = new MyApp(api, session);

      await app.logout();

      expect(app.busy).toBe(false);
      expect(session.authenticated).toBe(outcome === 'new-login');

      if (outcome === 'new-login') {
        expect(session.token).toBe('new-jwt');
        expect(session.notice).toBe('');
      } else {
        expect(session.notice).toContain(
          outcome === 200 || outcome === 401 ? 'ログアウトしました' : '失効は確認できません'
        );
      }
    }
  });

  it('request ignores old private responses without clearing a newer login', async () => {
    for (const status of [200, 401]) {
      const session = new SessionService();

      session.start('old-jwt');

      const http = new HttpClient(session, async () => {
        session.start('new-jwt');

        return new Response('[]', { status });
      });

      await expect(http.request('GET', '/challenges/me', { protected: true })).rejects.toThrow(
        '結果を破棄'
      );
      expect(session.token).toBe('new-jwt');
    }
  });
});
