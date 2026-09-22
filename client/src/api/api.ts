import type { components, paths } from './generated/schema';
import { ApiError, HttpClient } from './http-client';
import type { Genre } from './genre';

export type Challenge = components['schemas']['Challenge'];
export type PublicChallenge = components['schemas']['ChallengeWithoutFlag'];
export type ChallengeInput = Omit<components['schemas']['CreateChallengeRequest'], 'genre'> & {
  genre?: Genre;
};
export type Answer = components['schemas']['Answer'];
export type CorrectAnswer = components['schemas']['CorrectAnswer'];
export type User = components['schemas']['User'];
export type Credentials = paths['/login']['post']['requestBody']['content']['application/json'];

type SchemaFields = Record<string, 'string' | 'number' | 'boolean'>;

const challengeFields: SchemaFields = {
  id: 'number',
  name: 'string',
  description: 'string',
  genre: 'string',
  creator_id: 'string'
};

const answerFields: SchemaFields = {
  challenge_id: 'number',
  username: 'string',
  answered_at: 'string'
};

// Properties remain optional, as in OpenAPI; present values must have the documented type.
export function objectResponse<T>(value: unknown, fields: SchemaFields): T {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new ApiError('サーバーの応答形式が不正です。');
  }

  const result: Record<string, unknown> = {};

  for (const [key, type] of Object.entries(fields)) {
    const field = (value as Record<string, unknown>)[key];

    if (field !== undefined) {
      if (typeof field !== type) {
        throw new ApiError('サーバーの応答形式が不正です。');
      }

      result[key] = field;
    }
  }

  return result as T;
}

function listResponse<T>(value: unknown, fields: SchemaFields): T[] {
  if (!Array.isArray(value)) {
    throw new ApiError('サーバーの応答形式が不正です。');
  }

  return value.map((item) => objectResponse<T>(item, fields));
}

export class Api {
  static inject = [HttpClient];

  constructor(private http: HttpClient) {}

  async login(body: Credentials) {
    const data = objectResponse<{ token?: string }>(
      await this.http.request('POST', '/login', { body }),
      { token: 'string' }
    );

    if (!data.token) {
      throw new ApiError('ログイン応答にtokenがありません。');
    }

    return data.token;
  }

  logout() {
    return this.http.request('POST', '/logout', { empty: true, protected: true });
  }

  async signup(body: Credentials) {
    return objectResponse<{ id?: string; username?: string }>(
      await this.http.request('POST', '/signup', { body }),
      { id: 'string', username: 'string' }
    );
  }

  async challenges() {
    return listResponse<PublicChallenge>(
      await this.http.request('GET', '/challenges'),
      challengeFields
    );
  }

  async myChallenges() {
    return listResponse<Challenge>(
      await this.http.request('GET', '/challenges/me', { protected: true }),
      { ...challengeFields, flag: 'string' }
    );
  }

  async saveChallenge(body: ChallengeInput, id?: number) {
    return objectResponse<Challenge>(
      await this.http.request(id === undefined ? 'POST' : 'PUT', '/challenges', {
        body,
        query: id === undefined ? undefined : { id },
        protected: true
      }),
      { ...challengeFields, flag: 'string' }
    );
  }

  deleteChallenge(id: number) {
    return this.http.request('DELETE', '/challenges', {
      query: { id },
      empty: true,
      protected: true
    });
  }

  async answers(challengeId?: number) {
    return listResponse<CorrectAnswer>(
      await this.http.request('GET', '/answers', { query: { challenge_id: challengeId } }),
      answerFields
    );
  }

  async myAnswers(challengeId?: number) {
    return listResponse<Answer>(
      await this.http.request('GET', '/answers/me', {
        query: { challenge_id: challengeId },
        protected: true
      }),
      { ...answerFields, answer: 'string', correct: 'boolean' }
    );
  }

  async answer(challengeId: number, answer: string) {
    const result = objectResponse<Answer>(
      await this.http.request('POST', '/answers', {
        body: { challenge_id: challengeId, answer },
        protected: true
      }),
      { ...answerFields, answer: 'string', correct: 'boolean' }
    );

    if (result.correct === undefined) {
      throw new ApiError('応答に正誤の情報がありません。履歴を確認してください。');
    }

    return result;
  }

  async users() {
    return listResponse<User>(await this.http.request('GET', '/users'), {
      id: 'string',
      username: 'string',
      score: 'number'
    });
  }
}
