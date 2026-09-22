import { beforeAll, describe, expect, it } from 'vitest';
import { createCases } from '../src/cases';
import { baseUrl, Contract, loadContract, requestUrl, type RequestCase } from '../src/contract';

let contract: Contract;

beforeAll(async () => {
  contract = new Contract(await loadContract());
});

describe('Contract', () => {
  it('assertCoverage detects missing and unknown operations', () => {
    const cases = createCases();

    for (const [input, error] of [
      [cases, undefined],
      [cases.filter((test) => test.path !== '/users'), /missing \[get \/users\]/],
      [[...cases, { method: 'get', path: '/unknown' }], /extra \[get \/unknown\]/]
    ] as const) {
      const check = () => contract.assertCoverage([...input]);

      if (error) {
        expect(check).toThrow(error);
      } else {
        expect(check).not.toThrow();
      }
    }
  });

  it('assertRequest checks body and query schemas without adding required properties', () => {
    const cases: [RequestCase, RegExp | undefined][] = [
      ...createCases().map((test) => [test, undefined] as [RequestCase, undefined]),
      [{ method: 'post', path: '/signup', body: {} }, undefined],
      [{ method: 'post', path: '/signup', body: { extra: true } }, undefined],
      [{ method: 'post', path: '/signup' }, /body: required/],
      [{ method: 'post', path: '/signup', body: { username: 123 } }, /username.*string/],
      [{ method: 'post', path: '/signup', body: null }, /object/],
      [{ method: 'delete', path: '/challenges' }, /query.id: required/],
      [{ method: 'delete', path: '/challenges', query: { id: '1' } }, /query.id.*integer/],
      [{ method: 'delete', path: '/challenges', query: { id: 1.5 } }, /query.id.*integer/],
      [{ method: 'get', path: '/unknown' }, /Unknown operation/]
    ];

    for (const [input, error] of cases) {
      const check = () => contract.assertRequest(input);

      if (error) {
        expect(check).toThrow(error);
      } else {
        expect(check).not.toThrow();
      }
    }
  });

  it('assertResponse enforces only the documented response contract', () => {
    const cases: {
      request: RequestCase;
      status: number;
      body: string;
      contentType?: string | null;
      error?: RegExp;
    }[] = [
      { request: { method: 'post', path: '/login' }, status: 200, body: '{}' },
      { request: { method: 'post', path: '/login' }, status: 200, body: '{"extra":true}' },
      {
        request: { method: 'post', path: '/login' },
        status: 200,
        body: '{"token":"t"}',
        contentType: 'Application/JSON; charset=utf-8'
      },
      {
        request: { method: 'post', path: '/login' },
        status: 200,
        body: '{"token":3}',
        error: /POST \/login response 200 body.*token.*string/
      },
      {
        request: { method: 'post', path: '/login' },
        status: 401,
        body: '{}',
        error: /response 401.*undefined status/
      },
      {
        request: { method: 'post', path: '/login' },
        status: 200,
        body: '{}',
        contentType: null,
        error: /Content-Type.*missing/
      },
      {
        request: { method: 'post', path: '/login' },
        status: 200,
        body: '{}',
        contentType: 'text/plain',
        error: /Content-Type.*text\/plain/
      },
      {
        request: { method: 'post', path: '/login' },
        status: 200,
        body: 'bad',
        error: /invalid JSON/
      },
      { request: { method: 'post', path: '/login' }, status: 200, body: '', error: /invalid JSON/ },
      { request: { method: 'post', path: '/login' }, status: 200, body: 'null', error: /object/ },
      { request: { method: 'get', path: '/users' }, status: 200, body: '[]' },
      { request: { method: 'get', path: '/users' }, status: 200, body: '[{}]' },
      { request: { method: 'get', path: '/users' }, status: 200, body: '{}', error: /array/ },
      {
        request: { method: 'get', path: '/users' },
        status: 200,
        body: '[{"score":1.5}]',
        error: /score.*integer/
      },
      {
        request: { method: 'get', path: '/challenges' },
        status: 200,
        body: '[{"flag":"allowed extra field"}]'
      },
      {
        request: { method: 'get', path: '/challenges' },
        status: 200,
        body: '[{"id":"1"}]',
        error: /id.*integer/
      },
      {
        request: { method: 'get', path: '/answers/me' },
        status: 200,
        body: '[{"answered_at":"2026-09-22T12:30:00Z","correct":false}]'
      },
      {
        request: { method: 'get', path: '/answers/me' },
        status: 200,
        body: '[{"answered_at":"2026-02-30T12:30:00Z"}]',
        error: /answered_at.*date-time/
      },
      {
        request: { method: 'get', path: '/answers/me' },
        status: 200,
        body: '[{"correct":"false"}]',
        error: /correct.*boolean/
      },
      {
        request: { method: 'delete', path: '/challenges' },
        status: 403,
        body: 'arbitrary',
        contentType: 'text/plain'
      },
      { request: { method: 'get', path: '/answers' }, status: 404, body: '', contentType: null },
      {
        request: { method: 'post', path: '/logout' },
        status: 200,
        body: 'arbitrary',
        contentType: null
      }
    ];

    for (const { request, status, body, contentType = 'application/json', error } of cases) {
      const check = () => contract.assertResponse(request, { status, body, contentType });

      if (error) {
        expect(check).toThrow(error);
      } else {
        expect(check).not.toThrow();
      }
    }
  });
});

it('baseUrl requires an explicit HTTP target and preserves a base path', () => {
  for (const [input, expected] of [
    ['http://localhost:8080', 'http://localhost:8080/'],
    ['http://localhost:8080/api', 'http://localhost:8080/api/'],
    ['https://example.test/api/', 'https://example.test/api/']
  ]) {
    expect(baseUrl(input).href).toBe(expected);
  }

  for (const input of [
    undefined,
    '',
    'bad',
    'file:///tmp',
    'http://example.test/?a=b',
    'http://example.test/#fragment',
    'http://user:pass@example.test'
  ]) {
    expect(() => baseUrl(input)).toThrow();
  }
});

it('requestUrl preserves the base path and serializes query values', () => {
  for (const [base, request, expected] of [
    ['http://localhost:8080', { method: 'get', path: '/users' }, 'http://localhost:8080/users'],
    [
      'http://localhost:8080/api/',
      { method: 'get', path: '/answers', query: { challenge_id: -1 } },
      'http://localhost:8080/api/answers?challenge_id=-1'
    ],
    [
      'http://localhost:8080/api',
      { method: 'get', path: '/answers', query: { challenge_id: undefined } },
      'http://localhost:8080/api/answers'
    ]
  ] as const) {
    expect(requestUrl(baseUrl(base), request).href).toBe(expected);
  }
});
