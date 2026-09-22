import type { PublicChallenge, User } from '../../api/api';
import { PageState } from '../shared/page-state';
import './challenges-page.css';

export class ChallengesPage extends PageState {
  items: PublicChallenge[] = [];
  users: User[] = [];
  search = '';
  genre = '';

  loading() {
    return this.refresh();
  }

  refresh() {
    return this.read(
      () => Promise.all([this.api.challenges(), this.api.users().catch(() => [])]),
      ([items, users]) => {
        this.items = items;
        this.users = users;
      }
    );
  }

  creatorName(id?: string) {
    if (!id) {
      return '作成者不明';
    }

    return this.users.find((user) => user.id === id)?.username || id;
  }

  get genres() {
    return [...new Set(this.items.map((item) => item.genre).filter(Boolean))];
  }

  get filtered() {
    return this.items.filter(
      (item) =>
        (!this.genre || item.genre === this.genre) &&
        (item.name ?? '').toLowerCase().includes(this.search.toLowerCase())
    );
  }
}
