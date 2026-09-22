import { describe, it, expect, vi } from 'vitest';
import { Registration } from 'aurelia';
import { RouterConfiguration, IRouter } from '@aurelia/router';
import { createFixture } from '@aurelia/testing';
import { MyApp } from '../src/my-app';
import template from '../src/my-app.html';
import { Api } from '../src/api/api';
import { HttpClient } from '../src/api/http-client';
import { SessionService } from '../src/services/session-service';
import { createStore } from '../mock/store.mjs';
import { handleRequest } from '../mock/handlers.mjs';

describe('application', () => {
  it('renders every route with an in-memory API and enforces private navigation', async () => {
    window.history.replaceState(null, '', '/#/challenges');

    const session = new SessionService();
    const store = createStore();
    const fetcher: typeof fetch = async (input, init) => {
      const url = new URL(String(input), 'http://localhost');
      const result = handleRequest(store, {
        method: init?.method ?? 'GET',
        path: url.pathname.replace(/^\/api/, ''),
        query: Object.fromEntries(url.searchParams),
        token: (init?.headers as Record<string, string>)?.Authorization?.replace('Bearer ', ''),
        body: init?.body ? JSON.parse(String(init.body)) : undefined
      });

      return new Response(result.body === undefined ? null : JSON.stringify(result.body), {
        status: result.status
      });
    };

    const api = new Api(new HttpClient(session, fetcher));
    const fixture = createFixture(template, MyApp, [
      MyApp,
      RouterConfiguration.customize({ useUrlFragmentHash: true }),
      Registration.instance(Api, api),
      Registration.instance(SessionService, session)
    ]);

    await fixture.started;

    const router = fixture.container.get(IRouter);

    for (const [path, expected] of [
      ['challenges', 'はじめてのフラグ'],
      ['challenges/1', 'フラグを提出'],
      ['answers', '正答履歴'],
      ['ranking', '200'],
      ['signup', '登録する'],
      ['login', 'ログイン'],
      ['me/challenges', 'ログイン'],
      ['missing-route', 'ページが見つかりません']
    ]) {
      await router.load(path);

      expect(fixture.appHost.textContent, path).toContain(expected);
    }

    session.start(await api.login({ username: 'alice', password: 'demo-password' }));

    for (const [path, expected] of [
      ['me/challenges', 'はじめてのフラグ'],
      ['me/challenges/new', '問題を作成'],
      ['me/challenges/1/edit', '削除する'],
      ['me/answers', '自分の解答'],
      ['challenges/1', '解答する']
    ]) {
      await router.load(path);

      expect(fixture.appHost.textContent, path).toContain(expected);
    }

    const input = fixture.appHost.querySelector<HTMLInputElement>('#answer')!;

    input.value = 'flag{welcome}';
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.closest('form')!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    await vi.waitFor(() => expect(fixture.appHost.textContent).toContain('正解です！'));

    expect(store.answers.filter((item) => item.user_id === 'alice')).toHaveLength(1);

    await router.load('me/answers');

    expect(fixture.appHost.textContent).toContain('flag{welcome}');

    await router.load('answers');

    expect(fixture.appHost.textContent).not.toContain('flag{welcome}');

    session.clear();
    await router.load('me/answers');

    expect(fixture.appHost.querySelector('#password')).not.toBeNull();
  });
});
