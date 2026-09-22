import { SessionService } from '../services/session-service';
import type { paths } from './generated/schema';

type MethodFor<Path extends keyof paths> = {
  [Method in keyof paths[Path]]: NonNullable<paths[Path][Method]> extends { responses: unknown }
    ? Uppercase<Method & string>
    : never;
}[keyof paths[Path]];

export class ApiError extends Error {
  constructor(
    message: string,
    public status = 0
  ) {
    super(message);
  }
}

export function errorMessage(error: unknown): string {
  return error instanceof ApiError ? error.message : '処理に失敗しました。もう一度お試しください。';
}

export class HttpClient {
  static inject = [SessionService];

  constructor(
    public session: SessionService,
    private fetcher: typeof fetch = (input, init) => globalThis.fetch(input, init),
    private baseUrl = '/api'
  ) {}

  async request<Path extends keyof paths>(
    method: MethodFor<Path>,
    path: Path,
    options: {
      body?: unknown;
      query?: Record<string, number | undefined>;
      empty?: boolean;
      protected?: boolean;
    } = {}
  ): Promise<unknown> {
    const version = this.session.version;
    const headers: Record<string, string> = { Accept: 'application/json' };

    if (options.body !== undefined) headers['Content-Type'] = 'application/json';

    if (this.session.token) headers.Authorization = `Bearer ${this.session.token}`;

    const query = new URLSearchParams();

    Object.entries(options.query ?? {}).forEach(([key, value]) => {
      if (value !== undefined) query.set(key, String(value));
    });

    let response: Response;

    try {
      response = await this.fetcher(`${this.baseUrl}${path}${query.size ? `?${query}` : ''}`, {
        method,
        headers,
        body: options.body === undefined ? undefined : JSON.stringify(options.body)
      });
    } catch {
      throw new ApiError(
        method === 'GET'
          ? 'サーバーに接続できません。再試行してください。'
          : '通信が切れました。処理が完了している可能性があります。履歴や一覧を確認してください。'
      );
    }

    if (options.protected && version !== this.session.version) {
      throw new ApiError('ログイン状態が変わったため結果を破棄しました。');
    }

    if (!response.ok) {
      if (response.status === 401 && options.protected) {
        this.session.clear('ログインの有効期限が切れました。再度ログインしてください。');
      }

      const messages: Record<number, string> = {
        400: '入力内容を確認してください。',
        401: '認証に失敗しました。ログイン情報を確認してください。',
        403: 'この操作を行う権限がありません。',
        404: '対象の問題が見つかりません。',
        409: '同じユーザー名が登録されています。'
      };

      throw new ApiError(
        messages[response.status] ?? 'サーバーでエラーが発生しました。再試行してください。',
        response.status
      );
    }

    if (options.empty) return undefined;

    let result: unknown;

    try {
      result = await response.json();
    } catch {
      throw new ApiError('サーバーの応答を読み取れませんでした。');
    }

    if (options.protected && version !== this.session.version) {
      throw new ApiError('ログイン状態が変わったため結果を破棄しました。');
    }

    return result;
  }
}
