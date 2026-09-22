import { describe, it, expect, vi } from 'vitest';
import { Api } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { SessionService } from '../../src/services/session-service';
import { PageState, formatDate, parseId } from '../../src/pages/page-state';
import { sortUsers } from '../../src/pages/ranking-page';
import { EditorPage } from '../../src/pages/editor-page';
import { DetailPage } from '../../src/pages/detail-page';

describe('page state', () => {
  it('read ignores stale responses and clears private data on session loss', async () => {
    const session = new SessionService();
    const page = new PageState(new Api(new HttpClient(session)), session);
    page.binding();
    let resolve!: (value: string) => void;
    let value = '';
    const first = page.read(
      () =>
        new Promise<string>((done) => {
          resolve = done;
        }),
      (result) => {
        value = result;
      }
    );
    await page.read(
      async () => 'new',
      (result) => {
        value = result;
      }
    );
    resolve('old');
    await first;
    expect(value).toBe('new');
    const pending = page.read(
      () =>
        new Promise<string>((done) => {
          resolve = done;
        }),
      (result) => {
        value = result;
      }
    );
    session.clear();
    resolve('private');
    await pending;
    expect(value).toBe('new');
    page.unbinding();
  });
  it('save retains input on failure and blocks duplicate submissions', async () => {
    const session = new SessionService();
    session.start('t');
    const api = new Api(new HttpClient(session));
    let reject!: (reason: Error) => void;
    const save = vi.spyOn(api, 'saveChallenge').mockImplementation(
      () =>
        new Promise((_resolve, fail) => {
          reject = fail;
        })
    );
    const page = new EditorPage(api, session);
    page.ready = true;
    page.form = { name: 'n', description: 'd', genre: 'g', flag: 'f' };
    const first = page.save();
    await page.save();
    expect(save).toHaveBeenCalledTimes(1);
    reject(new Error('failure'));
    await first;
    expect(page.form.flag).toBe('f');
    expect(page.busy).toBe(false);
  });
  it('submit allows repeated answers and displays correct false as a result', async () => {
    const session = new SessionService();
    session.start('t');
    const api = new Api(new HttpClient(session));
    const answer = vi.spyOn(api, 'answer').mockResolvedValue({ correct: false });
    vi.spyOn(api, 'answers').mockResolvedValue([]);
    vi.spyOn(api, 'myAnswers').mockResolvedValue([]);
    const page = new DetailPage(api, session);
    page.id = 1;
    page.challenge = { id: 1 };
    for (const input of ['wrong', 'again']) {
      page.answer = input;
      await page.submit();
      expect(page.message).toContain('不正解');
    }
    expect(answer).toHaveBeenCalledTimes(2);
  });
  it('sortUsers uses scores, stable ties and missing values', () => {
    const input = [
      { id: 'z' },
      { id: 'b', score: 100 },
      { id: 'a', score: 100 },
      { id: 'c', score: 0 }
    ];
    expect(sortUsers(input).map((item) => item.id)).toEqual(['a', 'b', 'c', 'z']);
    expect(input[0].id).toBe('z');
  });
  it('formatDate handles absent and invalid timestamps', () => {
    for (const input of [undefined, '', 'invalid']) expect(formatDate(input)).toBe('日時不明');
    expect(formatDate('2026-09-22T00:00:00Z')).not.toBe('日時不明');
  });
  it('parseId accepts integer route values', () => {
    for (const [input, expected] of [
      ['1', 1],
      ['0', 0],
      ['-1', -1],
      ['abc', undefined],
      ['', undefined],
      [undefined, undefined]
    ] as const)
      expect(parseId(input)).toBe(expected);
  });
});
