import { Api } from '../api/api';
import { errorMessage } from '../api/http-client';
import { SessionService } from '../services/session-service';

export class PageState {
  static inject = [Api, SessionService];
  busy = false;
  pending = false;
  error = '';
  message = '';
  private generation = 0;
  private lifetime = 0;
  private unsubscribe?: () => void;

  constructor(
    public api: Api,
    public session: SessionService
  ) {}

  binding() {
    this.unsubscribe = this.session.subscribe(() => {
      this.reset();
      this.clearPrivate();

      if (!this.session.authenticated) this.error = 'ログインしてください。';
    });
  }

  unbinding() {
    this.reset();
    this.unsubscribe?.();
    this.clearPrivate();
  }

  clearPrivate() {}

  attached() {
    document.querySelector<HTMLElement>('main')?.focus();
  }

  reset() {
    this.generation++;
    this.lifetime++;
    this.pending = false;
    this.busy = false;
    this.error = '';
    this.message = '';
  }

  checkpoint() {
    const lifetime = this.lifetime;
    const version = this.session.version;

    return () => lifetime === this.lifetime && version === this.session.version;
  }

  get allowed() {
    return this.session.authenticated;
  }

  async read<T>(load: () => Promise<T>, apply: (result: T) => void) {
    const generation = ++this.generation;

    this.pending = true;
    this.error = '';

    try {
      const result = await load();

      if (generation === this.generation) apply(result);
    } catch (error) {
      if (generation === this.generation) this.error = errorMessage(error);
    } finally {
      if (generation === this.generation) this.pending = false;
    }
  }

  async write(action: (current: () => boolean) => Promise<void>) {
    if (this.busy) return;

    const current = this.checkpoint();

    this.busy = true;
    this.error = '';
    this.message = '';

    try {
      await action(current);
    } catch (error) {
      if (current()) this.error = errorMessage(error);
    } finally {
      if (current()) this.busy = false;
    }
  }
}

export function formatDate(value?: string) {
  if (!value) return '日時不明';

  const date = new Date(value);

  return Number.isNaN(date.getTime()) ? '日時不明' : date.toLocaleString('ja-JP');
}

export function parseId(value: unknown): number | undefined {
  if (typeof value !== 'string' || !/^-?\d+$/.test(value)) return undefined;

  return Number(value);
}
