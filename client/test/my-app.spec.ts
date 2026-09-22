import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
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
  const base = document.createElement('base');

  beforeEach(() => {
    base.href = '/';
    document.head.append(base);
  });

  afterEach(() => {
    base.remove();
  });

  it('renders every route with an in-memory API and enforces private navigation', async () => {
    window.history.replaceState(null, '', '/#/');

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

    expect(fixture.appHost.textContent).toContain('EachOther');
    expect(fixture.appHost.textContent).toContain('注意事項');
    expect(new URL(fixture.appHost.querySelector<HTMLAnchorElement>('.brand')!.href).hash).toBe(
      '#/'
    );

    const router = fixture.container.get(IRouter);

    await router.load('challenges');

    const card = fixture.appHost.querySelector('.challenge-card')!;

    expect(card.classList.contains('is-link')).toBe(true);
    expect(card.querySelectorAll('a')).toHaveLength(1);
    expect(new URL(card.querySelector('a')!.href).hash).toBe('#/challenges/1');
    expect(card.querySelector('.creator')?.textContent).toContain('alice');

    card.querySelector('a')!.click();
    await vi.waitFor(() => expect(fixture.appHost.textContent).toContain('フラグを提出'));

    for (const [path, expected] of [
      ['challenges', 'はじめてのフラグ'],
      ['challenges/1', 'フラグを提出'],
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

    expect(fixture.appHost.querySelector('.solved')).toBeNull();

    input.value = 'flag{welcome}';
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.closest('form')!.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    await vi.waitFor(() =>
      expect(fixture.appHost.querySelector('.result')?.textContent).toContain('正解')
    );

    expect(store.answers.filter((item) => item.user_id === 'alice')).toHaveLength(1);
    expect(fixture.appHost.querySelector('.solved')?.textContent).toContain('正解済み');

    await router.load('me/answers');

    expect(fixture.appHost.textContent).toContain('flag{welcome}');

    await router.load('answers');

    expect(fixture.appHost.textContent).not.toContain('flag{welcome}');

    await router.load('challenges/1');

    expect(fixture.appHost.querySelector('.solved')?.textContent).toContain('正解済み');

    await router.load('challenges/2');

    expect(fixture.appHost.querySelector('.solved')).toBeNull();

    session.clear();
    await router.load('me/answers');

    expect(fixture.appHost.querySelector('#password')).not.toBeNull();
  });
});
