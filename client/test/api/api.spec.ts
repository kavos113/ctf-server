import { describe, it, expect, vi } from 'vitest';
import { Api, objectResponse } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { SessionService } from '../../src/services/session-service';

describe('API', () => {
  it('maps all operations to the OpenAPI contract', async () => {
    const session = new SessionService();

    session.start('test-token');

    const fetcher = vi.fn<typeof fetch>();
    const api = new Api(new HttpClient(session, fetcher));
    const credentials = { username: 'a', password: 'p' };
    const challenge = { name: 'n', description: 'd', genre: 'web' as const, flag: 'f' };
    const cases: [() => Promise<unknown>, string, string, unknown, unknown, number][] = [
      [() => api.login(credentials), 'POST', '/login', credentials, { token: 't' }, 200],
      [() => api.logout(), 'POST', '/logout', undefined, undefined, 200],
      [() => api.signup(credentials), 'POST', '/signup', credentials, { id: 'a' }, 201],
      [() => api.challenges(), 'GET', '/challenges', undefined, [], 200],
      [() => api.saveChallenge(challenge), 'POST', '/challenges', challenge, challenge, 201],
      [() => api.saveChallenge(challenge, 2), 'PUT', '/challenges?id=2', challenge, challenge, 200],
      [() => api.deleteChallenge(2), 'DELETE', '/challenges?id=2', undefined, undefined, 200],
      [() => api.myChallenges(), 'GET', '/challenges/me', undefined, [], 200],
      [() => api.answers(), 'GET', '/answers', undefined, [], 200],
      [() => api.answers(2), 'GET', '/answers?challenge_id=2', undefined, [], 200],
      [
        () => api.answer(2, 'f'),
        'POST',
        '/answers',
        { challenge_id: 2, answer: 'f' },
        { correct: false },
        200
      ],
      [() => api.myAnswers(2), 'GET', '/answers/me?challenge_id=2', undefined, [], 200],
      [() => api.users(), 'GET', '/users', undefined, [], 200]
    ];

    for (const [call, method, path, body, result, status] of cases) {
      fetcher.mockResolvedValueOnce(
        new Response(result === undefined ? null : JSON.stringify(result), { status })
      );
      await call();

      expect(fetcher).toHaveBeenLastCalledWith(`/api${path}`, {
        method,
        body: body === undefined ? undefined : JSON.stringify(body),
        headers: {
          Accept: 'application/json',
          ...([
            'POST /login',
            'POST /signup',
            'GET /challenges',
            'GET /answers',
            'GET /users'
          ].includes(`${method} ${path.split('?')[0]}`)
            ? {}
            : { Authorization: 'Bearer test-token' }),
          ...(body === undefined ? {} : { 'Content-Type': 'application/json' })
        }
      });
    }
  });

  it('request handles failure and empty responses without depending on JSON errors', async () => {
    for (const [status, protectedRequest, loggedIn] of [
      [401, true, false],
      [401, false, true],
      [403, true, true],
      [404, true, true],
      [500, false, true]
    ] as const) {
      const session = new SessionService();

      session.start('t');

      const http = new HttpClient(session, async () => new Response('plain text', { status }));

      await expect(
        http.request('GET', '/challenges/me', { protected: protectedRequest })
      ).rejects.toMatchObject({ status });
      expect(session.authenticated).toBe(loggedIn);
    }

    const session = new SessionService();

    await expect(
      new HttpClient(session, async () => new Response('bad')).request('GET', '/challenges')
    ).rejects.toThrow('応答を読み取れません');
    await expect(
      new HttpClient(session, async () => {
        throw new Error();
      }).request('POST', '/login')
    ).rejects.toThrow('完了している可能性');
    await expect(
      new HttpClient(session, async () => new Response('')).request('POST', '/login', {
        empty: true
      })
    ).resolves.toBeUndefined();
  });

  it.each([
    { name: 'public answers', operation: 'public' },
    { name: 'own answers', operation: 'own' },
    { name: 'submitted answer', operation: 'submit' }
  ])('$name preserves username and excludes user_id', async ({ operation }) => {
    const answer = { challenge_id: 1, username: 'Alice', user_id: 'internal-id', correct: true };
    const payload = operation === 'submit' ? answer : [answer];
    const api = new Api(
      new HttpClient(new SessionService(), async () => new Response(JSON.stringify(payload)))
    );
    const result =
      operation === 'submit'
        ? await api.answer(1, 'flag')
        : (operation === 'own' ? await api.myAnswers() : await api.answers())[0];

    expect(result.username).toBe('Alice');
    expect(result).not.toHaveProperty('user_id');
  });

  it('objectResponse preserves optional fields and excludes unlisted data', () => {
    for (const [input, expected] of [
      [{}, {}],
      [{ id: 1, flag: 'secret' }, { id: 1 }]
    ]) {
      expect(objectResponse(input, { id: 'number' })).toEqual(expected);
    }

    for (const input of [null, [], 'text', { id: '1' }]) {
      expect(() => objectResponse(input, { id: 'number' })).toThrow();
    }
  });

  it('login and answer reject missing decision fields', async () => {
    const api = new Api(new HttpClient(new SessionService(), async () => new Response('{}')));

    await expect(api.login({ username: 'Alice', password: 'password' })).rejects.toThrow('token');
    await expect(api.answer(1, 'f')).rejects.toThrow('正誤');
  });
});
