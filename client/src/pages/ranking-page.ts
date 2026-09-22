import type { User } from '../api/api';
import { PageState } from './page-state';
export function sortUsers(users: User[]) {
  return [...users].sort((a, b) => {
    if (a.score === undefined)
      return b.score === undefined ? (a.id ?? '').localeCompare(b.id ?? '') : 1;
    if (b.score === undefined) return -1;
    return b.score - a.score || (a.id ?? '').localeCompare(b.id ?? '');
  });
}
export class RankingPage extends PageState {
  users: User[] = [];
  loading() {
    return this.refresh();
  }
  refresh() {
    return this.read(
      () => this.api.users(),
      (users) => {
        this.users = sortUsers(users);
      }
    );
  }
}
