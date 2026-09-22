import { Api } from './api/api';
import { ApiError, errorMessage } from './api/http-client';
import { SessionService } from './services/session-service';
import { AuthPage } from './pages/auth/auth-page';
import { HomePage } from './pages/home/home-page';
import { ChallengesPage } from './pages/challenges/challenges-page';
import { DetailPage } from './pages/detail/detail-page';
import { HistoryPage } from './pages/history/history-page';
import { RankingPage } from './pages/ranking/ranking-page';
import { MyChallengesPage } from './pages/my-challenges/my-challenges-page';
import { EditorPage } from './pages/editor/editor-page';
import { NotFoundPage } from './pages/not-found/not-found-page';
import './my-app.css';

export class MyApp {
  static inject = [Api, SessionService];

  static routes = [
    { path: '', component: HomePage, title: 'トップ | EachOther 2026' },
    { path: 'login', component: AuthPage, title: 'ログイン | EachOther 2026' },
    { path: 'signup', component: AuthPage, data: { signup: true }, title: 'ユーザー登録 | EachOther 2026' },
    { path: 'challenges', component: ChallengesPage, title: '問題一覧 | EachOther 2026' },
    { path: 'challenges/:id', component: DetailPage, title: '問題 | EachOther 2026' },
    { path: 'me/answers', component: HistoryPage, data: { mine: true }, title: '自分の解答 | EachOther 2026' },
    { path: 'ranking', component: RankingPage, title: 'ランキング | EachOther 2026' },
    { path: 'me/challenges', component: MyChallengesPage, title: '自分の問題 | EachOther 2026' },
    { path: 'me/challenges/new', component: EditorPage, title: '問題作成 | EachOther 2026' },
    { path: 'me/challenges/:id/edit', component: EditorPage, title: '問題編集 | EachOther 2026' },
    { path: 'not-found', component: NotFoundPage, title: 'ページが見つかりません | EachOther 2026' }
  ];

  busy = false;
  private unsubscribe?: () => void;

  constructor(
    public api: Api,
    public session: SessionService
  ) {}

  binding() {
    this.unsubscribe = this.session.subscribe(() => {
      if (this.session.notice && !this.session.authenticated) {
        window.location.hash = '/login';
      }
    });
  }

  unbinding() {
    this.unsubscribe?.();
  }

  focusMain() {
    document.querySelector<HTMLElement>('main')?.focus();

    return false;
  }

  async logout() {
    if (this.busy) {
      return;
    }

    this.busy = true;
    const version = this.session.version;

    let notice = 'ログアウトしました。';

    try {
      await this.api.logout();
    } catch (error) {
      if (!(error instanceof ApiError && error.status === 401)) {
        notice = `端末のログイン情報を削除しました。サーバー側の失効は確認できませんでした。${errorMessage(error)}`;
      }
    } finally {
      if (version === this.session.version) {
        this.session.clear(notice);
      }

      this.busy = false;

      if (!this.session.authenticated) {
        window.location.hash = '/login';
      }
    }
  }
}
