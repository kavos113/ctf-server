import type { Challenge } from '../api/api';
import { PageState } from './page-state';

export class MyChallengesPage extends PageState {
  items: Challenge[] = [];

  canLoad() {
    return this.allowed ? true : 'login';
  }

  loading() {
    return this.refresh();
  }

  clearPrivate() {
    this.items = [];
  }

  refresh() {
    if (!this.allowed) return;

    return this.read(
      () => this.api.myChallenges(),
      (items) => {
        this.items = items;
      }
    );
  }
}
