import type { PublicChallenge, User } from '../../api/api';
import { genres } from '../../api/genre';
import { contestState } from '../../services/contest';
import { PageState } from '../shared/page-state';
import './challenges-page.css';

export class ChallengesPage extends PageState {
  items: PublicChallenge[] = [];
  users: User[] = [];
  search = '';
  genre = '';
  genres = genres;
  contest = contestState();
  private startTimer?: ReturnType<typeof setInterval>;

  attached() {
    super.attached();

    if (!this.contest.started) {
      this.startTimer = setInterval(() => {
        if (contestState().started) {
          clearInterval(this.startTimer);
          void this.refresh();
        }
      }, 1000);
    }
  }

  unbinding() {
    clearInterval(this.startTimer);
    super.unbinding();
  }

  loading() {
    return this.refresh();
  }

  refresh() {
    this.contest = contestState();

    if (!this.contest.started) {
      this.reset();
      this.items = [];
      this.users = [];

      return;
    }

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

  get filtered() {
    return this.items.filter(
      (item) =>
        (!this.genre || item.genre === this.genre) &&
        (item.name ?? '').toLowerCase().includes(this.search.toLowerCase())
    );
  }
}
