import { randomUUID } from 'node:crypto';
import type { paths } from './generated/schema';

export function createCredentials() {
  return {
    username: `e2e-${randomUUID().replaceAll('-', '').slice(0, 28)}`,
    password: `password-${randomUUID()}`
  };
}

type Method = 'get' | 'post' | 'put' | 'delete' | 'patch' | 'head' | 'options' | 'trace';
type Body<T> = T extends { requestBody: { content: { 'application/json': infer B } } }
  ? { body: B }
  : { body?: never };
type Query<T> = T extends { parameters: { query: infer Q } }
  ? { query: Q }
  : T extends { parameters: { query?: infer Q } }
    ? { query?: Q }
    : { query?: never };

export type ContractCase = {
  [P in keyof paths]: {
    [M in Method]: paths[P][M] extends undefined
      ? never
      : { path: P; method: M } & Body<paths[P][M]> & Query<paths[P][M]>;
  }[Method];
}[keyof paths];

export function createCases(): ContractCase[] {
  const suffix = randomUUID();
  const credentials = createCredentials();
  const challenge = {
    name: `e2e-${suffix}`,
    description: `OpenAPI contract test ${suffix}`,
    flag: `flag-${suffix}`,
    genre: 'web' as const
  };

  return [
    { method: 'post', path: '/signup', body: credentials },
    { method: 'post', path: '/login', body: credentials },
    { method: 'post', path: '/logout' },
    { method: 'get', path: '/challenges' },
    { method: 'post', path: '/challenges', body: challenge },
    { method: 'put', path: '/challenges', query: { id: 2147483647 }, body: challenge },
    { method: 'delete', path: '/challenges', query: { id: 2147483647 } },
    { method: 'get', path: '/challenges/me' },
    { method: 'get', path: '/answers' },
    { method: 'get', path: '/answers', query: { challenge_id: 2147483647 } },
    {
      method: 'post',
      path: '/answers',
      body: { challenge_id: 2147483647, answer: `answer-${suffix}` }
    },
    { method: 'get', path: '/answers/me' },
    { method: 'get', path: '/answers/me', query: { challenge_id: 2147483647 } },
    { method: 'get', path: '/users' }
  ];
}
