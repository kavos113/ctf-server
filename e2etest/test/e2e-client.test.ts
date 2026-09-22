import { beforeAll, describe, expect, it, vi } from 'vitest';
import { baseUrl, Contract, loadContract } from '../src/contract';
import type { RequestCase } from '../src/contract';
import { E2eClient, requiredValue } from '../src/e2e-client';

let contract: Contract;

beforeAll(async () => {
  contract = new Contract(await loadContract());
});

describe('E2eClient without HTTP', () => {
  it('exchange validates requests and responses, sends credentials, and reports transport failures', async () => {
    const cases: {
      label: string;
      request: RequestCase;
      token?: string;
      status?: number;
      contentType?: string;
      body?: string;
      failure?: 'send' | 'read';
      error?: string;
      calls?: number;
    }[] = [
      {
        label: 'authenticated JSON',
        token: 'test-token',
        request: { method: 'post', path: '/challenges', body: { description: '"説明"\\\n🔐' } },
        status: 201,
        contentType: 'application/json; charset=utf-8',
        body: '{"id":7}'
      },
      { label: 'public GET', request: { method: 'get', path: '/users' }, body: '[]' },
      {
        label: 'query',
        request: { method: 'get', path: '/answers', query: { challenge_id: 7 } },
        body: '[]'
      },
      {
        label: 'invalid request',
        request: { method: 'put', path: '/challenges', body: {} },
        calls: 0,
        error: 'query.id: required'
      },
      {
        label: 'invalid JSON',
        request: { method: 'get', path: '/users' },
        body: '{',
        error: 'contract validation failed'
      },
      {
        label: 'invalid response schema',
        request: { method: 'get', path: '/users' },
        body: '{}',
        error: 'contract validation failed'
      },
      {
        label: 'undocumented status',
        request: { method: 'get', path: '/users' },
        status: 401,
        error: 'contract validation failed'
      },
      {
        label: 'send failure',
        request: { method: 'get', path: '/users' },
        failure: 'send',
        error: 'HTTP exchange failed'
      },
      {
        label: 'body read failure',
        request: { method: 'get', path: '/users' },
        failure: 'read',
        error: 'HTTP exchange failed'
      }
    ];

    for (const test of cases) {
      const response = new Response(test.body ?? '', {
        status: test.status ?? 200,
        headers: { 'Content-Type': test.contentType ?? 'application/json' }
      });

      if (test.failure === 'read') {
        vi.spyOn(response, 'text').mockRejectedValue(new Error('private transport data'));
      }

      const transport = vi.fn<typeof fetch>().mockImplementation(async () => {
        if (test.failure === 'send') {
          throw new Error('private transport data');
        }

        return response;
      });
      const client = new E2eClient(
        baseUrl('http://example.invalid/api'),
        contract,
        test.token,
        transport
      );
      const result = client.exchange(test.request, test.label);

      if (test.error) {
        await expect(result, test.label).rejects.toThrow(test.error);
      } else {
        await expect(result, test.label).resolves.toEqual({
          status: test.status ?? 200,
          contentType: test.contentType ?? 'application/json',
          body: test.body
        });
      }

      expect(transport, test.label).toHaveBeenCalledTimes(test.calls ?? 1);

      if (transport.mock.calls.length) {
        const [url, init] = transport.mock.calls[0];
        const headers = new Headers(init?.headers);

        expect(String(url), test.label).toBe(
          `http://example.invalid/api${test.request.path}${test.request.query ? '?challenge_id=7' : ''}`
        );
        expect(headers.get('Authorization'), test.label).toBe(
          test.token ? `Bearer ${test.token}` : null
        );
        expect(headers.get('Accept'), test.label).toBe('application/json');
        expect(headers.get('Content-Type'), test.label).toBe(
          test.request.body ? 'application/json' : null
        );
        expect(init?.method, test.label).toBe(test.request.method.toUpperCase());
        expect(init?.body, test.label).toBe(
          test.request.body === undefined ? undefined : JSON.stringify(test.request.body)
        );
        expect(init?.redirect, test.label).toBe('manual');
        expect(init?.signal, test.label).toBeInstanceOf(AbortSignal);
      }
    }
  });

  it('request enforces scenario status and only decodes schema-defined JSON bodies', async () => {
    const cases = [
      {
        label: 'JSON',
        status: 200,
        expected: 200,
        body: '[]',
        path: '/users',
        method: 'get',
        value: []
      },
      {
        label: 'documented error is not success',
        status: 404,
        expected: 200,
        body: '',
        path: '/answers',
        method: 'get',
        error: 'expected status 200, received 404'
      },
      {
        label: 'expected error',
        status: 404,
        expected: 404,
        body: 'arbitrary',
        path: '/answers',
        method: 'get',
        value: undefined
      },
      {
        label: 'cleanup already deleted',
        status: 404,
        expected: [200, 404],
        body: '',
        path: '/challenges',
        method: 'delete',
        value: undefined
      },
      {
        label: 'undefined response body',
        status: 200,
        expected: 200,
        body: 'not JSON',
        path: '/logout',
        method: 'post',
        value: undefined
      }
    ];

    for (const test of cases) {
      const transport = vi.fn<typeof fetch>().mockResolvedValue(
        new Response(test.body, {
          status: test.status,
          headers: { 'Content-Type': 'application/json' }
        })
      );
      const client = new E2eClient(
        baseUrl('http://example.invalid'),
        contract,
        undefined,
        transport
      );
      const result = client.request(
        {
          method: test.method,
          path: test.path,
          ...(test.method === 'delete' ? { query: { id: 7 } } : {})
        },
        test.expected,
        test.label
      );

      if (test.error) {
        await expect(result, test.label).rejects.toThrow(test.error);
      } else {
        await expect(result, test.label).resolves.toEqual(test.value);
      }
    }
  });

  it('requiredValue rejects missing or malformed scenario identifiers without exposing values', () => {
    for (const value of ['id', 'token']) {
      expect(requiredValue(value, 'string', 'field')).toBe(value);
    }

    for (const value of [0, 1, -1]) {
      expect(requiredValue(value, 'number', 'field')).toBe(value);
    }

    for (const value of [undefined, null, '', 1, {}, []]) {
      expect(() => requiredValue(value, 'string', 'field')).toThrow(
        'field: required nonempty string'
      );
    }

    for (const value of [undefined, null, 'secret', NaN, Infinity, 1.5, {}, []]) {
      expect(() => requiredValue(value, 'number', 'field')).toThrow('field: required integer');
    }
  });
});
