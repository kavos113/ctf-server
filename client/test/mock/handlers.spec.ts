import { describe, it, expect } from 'vitest';
import { createStore } from '../../mock/store.mjs';
import { handleRequest } from '../../mock/handlers.mjs';

describe('mock', () => {
  it('handleRequest supports the participant and creator lifecycle without HTTP', () => {
    const store = createStore();
    const call = (method: string, path: string, body?: unknown, token?: string, query = {}) =>
      handleRequest(store, { method, path, body, token, query });
    const token = call('POST', '/login', { username: 'alice', password: 'demo-password' }).body
      .token;

    expect(call('POST', '/signup', { username: 'new', password: 'password' }).status).toBe(201);
    expect(call('POST', '/login', { username: 'new', password: 'password' }).status).toBe(200);

    for (const [method, path] of [
      ['POST', '/logout'],
      ['GET', '/challenges/me'],
      ['GET', '/answers/me'],
      ['POST', '/answers'],
      ['POST', '/challenges'],
      ['PUT', '/challenges'],
      ['DELETE', '/challenges']
    ]) {
      expect(call(method, path).status).toBe(401);
    }

    const input = { name: 'test', genre: 'misc', description: 'test', flag: 'secret' };
    const created = call('POST', '/challenges', input, token);

    expect(created.status).toBe(201);

    const id = created.body.id;

    expect(call('PUT', '/challenges', { ...input, name: 'edited' }, token, { id }).body.name).toBe(
      'edited'
    );

    for (const answer of ['wrong', 'secret', 'secret']) {
      const result = call('POST', '/answers', { challenge_id: id, answer }, token);

      expect(result.status).toBe(200);
      expect(result.body.correct).toBe(answer === 'secret');
    }

    expect(call('GET', '/answers/me', undefined, token, { challenge_id: id }).body).toHaveLength(3);

    const publicAnswers = call('GET', '/answers', undefined, undefined, { challenge_id: id }).body;

    expect(publicAnswers).toHaveLength(2);
    expect(publicAnswers[0]).not.toHaveProperty('answer');
    expect(call('GET', '/challenges').body[0]).not.toHaveProperty('flag');
    expect(call('GET', '/users').body[0]).not.toHaveProperty('password');
    expect(call('DELETE', '/challenges', undefined, token, { id: 2 }).status).toBe(403);
    expect(call('DELETE', '/challenges', undefined, token, { id: 999 }).status).toBe(404);
    expect(call('DELETE', '/challenges', undefined, token, { id }).status).toBe(200);
    expect(call('GET', '/answers/me', undefined, token).body).toHaveLength(3);
    expect(call('GET', '/answers', undefined, undefined, { challenge_id: id }).status).toBe(404);
    expect(call('POST', '/logout', undefined, token).status).toBe(200);
    expect(call('GET', '/challenges/me', undefined, token).status).toBe(401);
    expect(createStore().challenges).toHaveLength(2);
  });
});
