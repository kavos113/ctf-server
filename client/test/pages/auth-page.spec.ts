import { describe, it, expect, vi } from 'vitest';
import { Api } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { SessionService } from '../../src/services/session-service';
import { AuthPage } from '../../src/pages/auth/auth-page';

describe('authentication page', () => {
  it('submit validates credentials and handles login, registration and failure', async () => {
    for (const [signup, username, status] of [
      [false, 'alice', 200],
      [true, 'alice', 201],
      [false, 'ab', 400],
      [false, 'alice', 401],
      [true, 'alice', 409],
      [false, 'alice', 503]
    ] as const) {
      const session = new SessionService();
      const fetcher = vi.fn<typeof fetch>().mockResolvedValue(
        new Response(JSON.stringify(signup ? { id: 'id', username } : { token: 'jwt' }), {
          status
        })
      );
      const page = new AuthPage(new Api(new HttpClient(session, fetcher)), session);

      page.binding();
      page.signup = signup;
      page.username = username;
      page.password = 'password';
      await page.submit();

      expect(fetcher).toHaveBeenCalledTimes(username === 'ab' ? 0 : 1);
      expect(session.authenticated).toBe(status === 200);
      expect(page.error !== '').toBe(status >= 400);
      expect(page.busy).toBe(false);

      if (status < 400) {
        expect(page.password).toBe('');
        expect(window.location.hash).toBe(signup ? '#/login' : '#/challenges');
      }

      page.unbinding();
    }
  });
});
