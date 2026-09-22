import type { RouteNode } from '@aurelia/router';
import type { Answer, PublicChallenge, User } from '../api/api';
import { PageState, formatDate, parseId } from './page-state';

export class HistoryPage extends PageState {
  mine = false;
  items: Answer[] = [];
  challenges: PublicChallenge[] = [];
  users: User[] = [];
  filter = '';
  formatDate = formatDate;

  canLoad(_params: unknown, next: RouteNode) {
    return !next.data.mine || this.allowed ? true : 'login';
  }

  loading(_params: unknown, next: RouteNode) {
    this.reset();
    this.items = [];
    this.mine = next.data.mine === true;
    this.filter = '';

    return this.refresh();
  }

  clearPrivate() {
    if (this.mine) this.items = [];
  }

  async refresh() {
    if (this.mine && !this.allowed) return;

    const id = parseId(this.filter);

    await this.read(
      async () => {
        const [items, challenges, users] = await Promise.all([
          this.mine ? this.api.myAnswers(id) : this.api.answers(id),
          this.api.challenges().catch(() => []),
          this.api.users().catch(() => [])
        ]);

        return { items, challenges, users };
      },
      (result) => Object.assign(this, result)
    );
  }

  challengeName(id?: number) {
    return this.challenges.find((item) => item.id === id)?.name ?? `問題 ${id ?? '不明'}`;
  }

  userName(id?: string) {
    return this.users.find((item) => item.id === id)?.username ?? id ?? 'ユーザー不明';
  }
}
