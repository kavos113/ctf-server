import { Api } from './api/api';
import { errorMessage } from './api/http-client';
import { SessionService } from './services/session-service';
import { AuthPage } from './pages/auth/auth-page';
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
    { path: '', redirectTo: 'challenges' },
    { path: 'login', component: AuthPage, title: 'ログイン' },
    { path: 'signup', component: AuthPage, data: { signup: true }, title: 'ユーザー登録' },
    { path: 'challenges', component: ChallengesPage, title: '問題一覧' },
    { path: 'challenges/:id', component: DetailPage, title: '問題' },
    { path: 'answers', component: HistoryPage, title: '正答履歴' },
    { path: 'me/answers', component: HistoryPage, data: { mine: true }, title: '自分の解答' },
    { path: 'ranking', component: RankingPage, title: 'ランキング' },
    { path: 'me/challenges', component: MyChallengesPage, title: '自分の問題' },
    { path: 'me/challenges/new', component: EditorPage, title: '問題作成' },
    { path: 'me/challenges/:id/edit', component: EditorPage, title: '問題編集' },
    { path: 'not-found', component: NotFoundPage, title: 'ページが見つかりません' }
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

    let notice = 'ログアウトしました。';

    try {
      await this.api.logout();
    } catch (error) {
      notice = `端末のログイン情報を削除しました。サーバー側の失効は確認できませんでした。${errorMessage(error)}`;
    } finally {
      this.session.clear(notice);
      this.busy = false;
      window.location.hash = '/login';
    }
  }
}
