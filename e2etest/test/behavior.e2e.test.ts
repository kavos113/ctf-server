import { randomUUID } from 'node:crypto';
import { beforeAll, describe, expect, it } from 'vitest';
import { descriptionCases, sqlLikeCases } from '../src/behavior-cases';
import { createCases, createCredentials } from '../src/cases';
import { baseUrl, Contract, loadContract } from '../src/contract';
import { E2eClient, requiredValue } from '../src/e2e-client';
import type { components } from '../src/generated/schema';

type ChallengeInput = Required<components['schemas']['CreateChallengeRequest']>;
type Challenge = components['schemas']['Challenge'];
type Answer = components['schemas']['Answer'];
type User = {
  id: string;
  username: string;
  client: E2eClient;
  token: string;
  credentials: ReturnType<typeof createCredentials>;
};
type Problem = { id: number; owner: User; input: ChallengeInput };

let base: URL;
let contract: Contract;

beforeAll(async () => {
  base = baseUrl(process.env.E2E_BASE_URL);
  contract = new Contract(await loadContract());
});

class Scenario {
  readonly publicClient = new E2eClient(base, contract);
  private readonly problems: Problem[] = [];

  constructor(readonly label: string) {}

  async user(): Promise<User> {
    const credentials = createCredentials();
    const signup = (await this.publicClient.request(
      { method: 'post', path: '/signup', body: credentials },
      201,
      `${this.label} signup`
    )) as { id?: string; username?: string };
    const id = requiredValue(signup.id, 'string', `${this.label} signup.id`);

    expect(signup.username, `${this.label} signup.username`).toBe(credentials.username);

    const login = (await this.publicClient.request(
      { method: 'post', path: '/login', body: credentials },
      200,
      `${this.label} login`
    )) as { token?: string };
    const token = requiredValue(login.token, 'string', `${this.label} login.token`);

    return {
      id,
      username: credentials.username,
      client: new E2eClient(base, contract, token),
      token,
      credentials
    };
  }

  async create(owner: User, overrides: Partial<ChallengeInput> = {}): Promise<Problem> {
    const input = {
      name: `e2e-${randomUUID()}`,
      description: 'description',
      genre: 'web',
      flag: `flag-${randomUUID()}`,
      ...overrides
    };
    const response = (await owner.client.request(
      { method: 'post', path: '/challenges', body: input },
      201,
      `${this.label} create`
    )) as Challenge;
    const problem = {
      id: requiredValue(response.id, 'number', `${this.label} create.id`),
      owner,
      input
    };

    // Track the ID before assertions so a mismatched response still gets cleaned up.
    this.problems.push(problem);
    this.challenge(response, problem, true, 'create');

    return problem;
  }

  challenge(actual: Challenge, problem: Problem, privateView: boolean, step: string): void {
    const context = `${this.label} ${step} id=${problem.id}`;

    expect(actual.id, `${context} id`).toBe(problem.id);
    expect(actual.creator_id, `${context} creator_id`).toBe(problem.owner.id);

    for (const field of ['name', 'description', 'genre'] as const) {
      expect(actual[field], `${context} ${field}`).toBe(problem.input[field]);
    }

    if (privateView) {
      expect(actual.flag === problem.input.flag, `${context} flag equality`).toBe(true);
    } else {
      expect(Object.hasOwn(actual, 'flag'), `${context} flag must be absent`).toBe(false);
    }
  }

  async list(owner?: User): Promise<Challenge[]> {
    return (await (owner?.client ?? this.publicClient).request(
      { method: 'get', path: owner ? '/challenges/me' : '/challenges' },
      200,
      `${this.label} list ${owner ? 'own' : 'public'}`
    )) as Challenge[];
  }

  async visible(problem: Problem, exists = true): Promise<void> {
    for (const owner of [undefined, problem.owner]) {
      const rows = (await this.list(owner)).filter((row) => row.id === problem.id);
      const step = owner ? 'own GET' : 'public GET';

      expect(rows.length, `${this.label} ${step} id=${problem.id} count`).toBe(exists ? 1 : 0);

      if (exists) {
        this.challenge(rows[0], problem, Boolean(owner), step);
      }
    }
  }

  async update(problem: Problem, input: ChallengeInput): Promise<Problem> {
    const updated = { ...problem, input };
    const response = (await problem.owner.client.request(
      { method: 'put', path: '/challenges', query: { id: problem.id }, body: input },
      200,
      `${this.label} update`
    )) as Challenge;

    this.challenge(response, updated, true, 'update');

    return updated;
  }

  async remove(problem: Problem): Promise<void> {
    await problem.owner.client.request(
      { method: 'delete', path: '/challenges', query: { id: problem.id } },
      200,
      `${this.label} delete`
    );
  }

  async submit(user: User, problem: Problem, answer: string, correct: boolean): Promise<void> {
    const response = (await user.client.request(
      { method: 'post', path: '/answers', body: { challenge_id: problem.id, answer } },
      200,
      `${this.label} submit id=${problem.id}`
    )) as Answer;

    this.answer(response, user, problem, answer, correct);
  }

  answer(actual: Answer, user: User, problem: Problem, answer: string, correct: boolean): void {
    const context = `${this.label} answer id=${problem.id}`;

    expect(actual.challenge_id, `${context} challenge_id`).toBe(problem.id);
    expect(actual.username, `${context} username`).toBe(user.username);
    expect(actual.answer === answer, `${context} answer equality`).toBe(true);
    expect(actual.correct, `${context} correct`).toBe(correct);
    requiredValue(actual.answered_at, 'string', `${context} answered_at`);
  }

  async history(user?: User, id?: number): Promise<Answer[]> {
    const rows = (await (user?.client ?? this.publicClient).request(
      {
        method: 'get',
        path: user ? '/answers/me' : '/answers',
        ...(id === undefined ? {} : { query: { challenge_id: id } })
      },
      200,
      `${this.label} history ${user ? 'own' : 'public'}`
    )) as Answer[];

    for (const row of rows) {
      requiredValue(row.answered_at, 'string', `${this.label} history.answered_at`);

      if (id !== undefined) {
        expect(row.challenge_id, `${this.label} history filter id=${id}`).toBe(id);
      }

      if (user) {
        expect(row.username, `${this.label} history owner`).toBe(user.username);
      } else {
        expect(Object.hasOwn(row, 'answer'), `${this.label} public answer absent`).toBe(false);
        expect(Object.hasOwn(row, 'flag'), `${this.label} public flag absent`).toBe(false);
      }
    }

    return rows;
  }

  async recorded(user: User, problem: Problem, answer: string, correct: boolean): Promise<void> {
    for (const id of [undefined, problem.id]) {
      const own = (await this.history(user, id)).filter(
        (row) => row.challenge_id === problem.id && row.answer === answer
      );

      expect(own.length, `${this.label} own history id=${problem.id}`).toBeGreaterThan(0);

      for (const row of own) {
        this.answer(row, user, problem, answer, correct);
      }

      const published = (await this.history(undefined, id)).filter(
        (row) => row.challenge_id === problem.id && row.username === user.username
      );

      expect(published.length > 0, `${this.label} public history id=${problem.id}`).toBe(correct);
    }
  }

  async cleanup(): Promise<void> {
    const errors: unknown[] = [];

    for (const problem of this.problems) {
      try {
        await problem.owner.client.request(
          { method: 'delete', path: '/challenges', query: { id: problem.id } },
          [200, 404],
          `${this.label} cleanup id=${problem.id}`
        );
      } catch (error) {
        errors.push(error);
      }
    }

    if (errors.length) {
      throw new AggregateError(errors, `${this.label} cleanup failed`);
    }
  }
}

function scenario(label: string, run: (scenario: Scenario) => Promise<void>): void {
  it(
    label,
    async () => {
      const context = new Scenario(label);
      let failed = false;
      let cleanupError: unknown;

      try {
        await run(context);
      } catch (error) {
        failed = true;

        throw error;
      } finally {
        try {
          await context.cleanup();
        } catch (error) {
          if (!failed) {
            cleanupError = error;
          } else {
            console.error(error);
          }
        }
      }

      if (cleanupError) {
        throw cleanupError;
      }
    },
    360000
  ); // At most 30 sequential exchanges plus cleanup, each bounded to 10 seconds.
}

describe('API behavior', { concurrent: false }, () => {
  // Valid bodies and query IDs ensure authentication is tested before resource lookup.
  const protectedCases = createCases().filter(
    (test) =>
      [
        'post /logout',
        'post /challenges',
        'put /challenges',
        'delete /challenges',
        'get /challenges/me',
        'get /answers/me',
        'post /answers'
      ].includes(`${test.method} ${test.path}`) && !(test.path === '/answers/me' && test.query)
  );

  for (const test of protectedCases) {
    for (const mode of ['missing', 'malformed', 'tampered'] as const) {
      scenario(`B22 ${mode} JWT: ${test.method} ${test.path}`, async (s) => {
        let token: string | undefined;

        if (mode === 'malformed') {
          token = 'not-a-jwt';
        } else if (mode === 'tampered') {
          const user = await s.user();
          const parts = user.token.split('.');

          expect(parts.length, 'JWT segments').toBe(3);
          expect(parts[2].length > 0, 'JWT signature present').toBe(true);
          parts[2] = (parts[2][0] === 'A' ? 'B' : 'A') + parts[2].slice(1);
          token = parts.join('.');
        }

        await new E2eClient(base, contract, token).request(test, 401, s.label);
      });
    }
  }

  scenario('B23 logout revokes only the presented JWT session', async (s) => {
    const user = await s.user();
    const second = (await s.publicClient.request(
      { method: 'post', path: '/login', body: user.credentials },
      200,
      'second login'
    )) as { token: string };
    const token = requiredValue(second.token, 'string', 'second login.token');
    const other = new E2eClient(base, contract, token);

    expect(token !== user.token, 'separate session tokens').toBe(true);
    await user.client.request({ method: 'get', path: '/challenges/me' }, 200, 'first session');
    await other.request({ method: 'get', path: '/challenges/me' }, 200, 'second session');
    await user.client.request({ method: 'post', path: '/logout' }, 200, 'logout');

    for (const test of protectedCases) {
      await user.client.request(test, 401, 'revoked JWT');
    }

    await other.request({ method: 'get', path: '/challenges/me' }, 200, 'other session survives');
    await other.request({ method: 'post', path: '/logout' }, 200, 'second logout');
  });

  scenario('B24 invalid credentials and duplicate signup', async (s) => {
    const user = await s.user();

    await s.publicClient.request(
      { method: 'post', path: '/signup', body: user.credentials },
      409,
      'duplicate signup'
    );

    for (const credentials of [
      { ...user.credentials, password: `wrong-${randomUUID()}` },
      createCredentials()
    ]) {
      await s.publicClient.request(
        { method: 'post', path: '/login', body: credentials },
        401,
        'invalid credentials'
      );
    }

    await user.client.request(
      { method: 'get', path: '/challenges/me' },
      200,
      'existing session survives'
    );
  });

  for (const { label, value } of sqlLikeCases) {
    for (const field of ['name', 'description', 'genre', 'flag'] as const) {
      scenario(`B20 SQL-like ${field}: ${label}`, async (s) => {
        const owner = await s.user();
        const neighbor = await s.create(owner);
        const input = field === 'name' ? `${randomUUID()} ${value}` : value;
        const problem = await s.create(owner, { [field]: input });

        expect(problem.id, `${s.label} distinct IDs`).not.toBe(neighbor.id);
        await s.visible(problem);
        await s.visible(neighbor);

        const updated = await s.update(problem, {
          ...problem.input,
          [field]: `updated ${input}`
        });

        await s.visible(updated);
        await s.visible(neighbor);
      });
    }

    scenario(`B21 SQL-like answer: ${label}`, async (s) => {
      const owner = await s.user();
      const solver = await s.user();
      const neighbor = await s.create(owner);
      const problem = await s.create(owner, { flag: value });

      expect(problem.id, `${s.label} distinct IDs`).not.toBe(neighbor.id);

      // SQL-looking text must not make a different flag match.
      await s.submit(solver, neighbor, value, false);
      await s.recorded(solver, neighbor, value, false);
      await s.submit(solver, problem, value, true);
      await s.recorded(solver, problem, value, true);
      await s.visible(problem);
      await s.visible(neighbor);
    });
  }

  scenario('B01 signup, login and user listing', async (s) => {
    const user = await s.user();
    const rows = (await s.publicClient.request(
      { method: 'get', path: '/users' },
      200,
      'B01 users'
    )) as components['schemas']['User'][];
    const matches = rows.filter((row) => row.id === user.id);

    expect(matches).toHaveLength(1);
    expect(matches[0].username).toBe(user.username);
  });

  for (const { label, description } of descriptionCases) {
    scenario(`B02/B03/B05 create and read description: ${label}`, async (s) => {
      await s.visible(await s.create(await s.user(), { description }));
    });
  }

  scenario('B04 multiple independent records', async (s) => {
    const owner = await s.user();
    const problems: Problem[] = [];

    for (const description of ['before', '"quoted"\n説明', 'after']) {
      problems.push(await s.create(owner, { description }));
    }

    expect(new Set(problems.map((problem) => problem.id)).size).toBe(3);

    for (const problem of problems) {
      await s.visible(problem);
    }
  });

  scenario('B06/B07 update and preserve neighboring record', async (s) => {
    const owner = await s.user();
    const first = await s.create(owner);
    const other = await s.create(owner);
    const updated = await s.update(first, {
      name: `updated-${randomUUID()}`,
      description: 'new "description" \\\n説明',
      genre: 'crypto',
      flag: `updated-${randomUUID()}`
    });

    await s.visible(updated);
    await s.visible(other);
  });

  scenario('B08/B09 delete, missing resource and unchanged neighbor', async (s) => {
    const owner = await s.user();
    const deleted = await s.create(owner);
    const other = await s.create(owner);

    await s.remove(deleted);
    await s.visible(deleted, false);

    for (const method of ['delete', 'put'] as const) {
      await owner.client.request(
        {
          method,
          path: '/challenges',
          query: { id: deleted.id },
          ...(method === 'put' ? { body: deleted.input } : {})
        },
        404,
        `${s.label} missing ${method}`
      );
    }

    await owner.client.request(
      {
        method: 'post',
        path: '/answers',
        body: { challenge_id: deleted.id, answer: 'missing' }
      },
      404,
      `${s.label} missing answer`
    );
    await s.visible(deleted, false);
    await s.visible(other);
  });

  for (const correct of [false, true]) {
    scenario(`${correct ? 'B11' : 'B10'} submission and history correct=${correct}`, async (s) => {
      const problem = await s.create(await s.user());
      const solver = await s.user();
      const answer = correct ? problem.input.flag : `wrong-${randomUUID()}`;

      await s.submit(solver, problem, answer, correct);
      await s.recorded(solver, problem, answer, correct);
    });
  }

  scenario('B12 special characters in flag and answer', async (s) => {
    const problem = await s.create(await s.user(), { flag: 'flag{"引用"\\\n日本語🔐}' });
    const solver = await s.user();
    const wrong = `${problem.input.flag}!`;

    await s.visible(problem);
    await s.submit(solver, problem, wrong, false);
    await s.recorded(solver, problem, wrong, false);
    await s.submit(solver, problem, problem.input.flag, true);
    await s.recorded(solver, problem, problem.input.flag, true);
  });

  scenario('B13 histories filter by challenge', async (s) => {
    const owner = await s.user();
    const solver = await s.user();
    const problems = [await s.create(owner), await s.create(owner)];

    for (const problem of problems) {
      await s.submit(solver, problem, problem.input.flag, true);
    }

    for (const problem of problems) {
      await s.recorded(solver, problem, problem.input.flag, true);
    }
  });

  scenario('B14 self answers and repeated submissions', async (s) => {
    const owner = await s.user();
    const problem = await s.create(owner);

    await s.submit(owner, problem, problem.input.flag, true);
    await s.submit(owner, problem, problem.input.flag, true);
    await s.recorded(owner, problem, problem.input.flag, true);
  });

  scenario('B15 judging uses updated flag', async (s) => {
    const original = await s.create(await s.user());
    const solver = await s.user();
    const updated = await s.update(original, { ...original.input, flag: `new-${randomUUID()}` });

    await s.submit(solver, updated, original.input.flag, false);
    await s.recorded(solver, updated, original.input.flag, false);
    await s.submit(solver, updated, updated.input.flag, true);
    await s.recorded(solver, updated, updated.input.flag, true);
  });

  for (const method of ['put', 'delete'] as const) {
    scenario(`${method === 'put' ? 'B16' : 'B17'} reject other user's ${method}`, async (s) => {
      const problem = await s.create(await s.user());
      const other = await s.user();

      await other.client.request(
        {
          method,
          path: '/challenges',
          query: { id: problem.id },
          ...(method === 'put'
            ? {
                body: {
                  name: 'unauthorized',
                  description: 'changed',
                  genre: 'crypto',
                  flag: 'changed'
                }
              }
            : {})
        },
        403,
        s.label
      );
      await s.visible(problem);
    });
  }

  scenario('B18 own challenges isolated by user', async (s) => {
    const a = await s.create(await s.user());
    const b = await s.create(await s.user());

    for (const [own, other] of [
      [a, b],
      [b, a]
    ]) {
      await s.visible(own);

      const rows = await s.list(own.owner);

      expect(
        rows.some((row) => row.id === other.id),
        `${s.label} other problem absent`
      ).toBe(false);
      expect(
        rows.every((row) => row.creator_id === own.owner.id),
        `${s.label} owner`
      ).toBe(true);
    }
  });

  scenario('B19 own answers isolated by user', async (s) => {
    const a = await s.user();
    const b = await s.user();
    const problem = await s.create(a);
    const submissions = [
      { user: a, answer: 'wrong-A' },
      { user: b, answer: 'wrong-B' }
    ];

    for (const { user, answer } of submissions) {
      await s.submit(user, problem, answer, false);
    }

    for (const { user, answer } of submissions) {
      await s.recorded(user, problem, answer, false);
    }
  });
});
