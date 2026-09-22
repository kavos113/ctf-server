import type { PublicChallenge } from '../../api/api';
import { PageState } from '../shared/page-state';
import './challenges-page.css';

export class ChallengesPage extends PageState {
  items: PublicChallenge[] = [];
  search = '';
  genre = '';

  loading() {
    return this.refresh();
  }

  refresh() {
    return this.read(
      () => this.api.challenges(),
      (items) => {
        this.items = items;
      }
    );
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
