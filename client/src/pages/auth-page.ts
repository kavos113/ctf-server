import type { RouteNode } from '@aurelia/router';
import { PageState } from './page-state';

export class AuthPage extends PageState {
  signup = false;
  username = '';
  password = '';

  loading(_params: unknown, next: RouteNode) {
    this.reset();
    this.password = '';
    this.signup = next.data.signup === true;
  }

  clearPrivate() {
    this.password = '';
  }

  async submit() {
    if (!this.username.trim() || !this.password) {
      this.error = 'ユーザー名とパスワードを入力してください。';
      return;
    }

    await this.write(async (current) => {
      const credentials = { username: this.username, password: this.password };

      if (this.signup) {
        await this.api.signup(credentials);

        if (!current()) return;

        this.session.notice = '登録しました。ログインしてください。';
        window.location.hash = '/login';
      } else {
        const token = await this.api.login(credentials);

        if (!current()) return;

        this.session.start(token);
        window.location.hash = '/challenges';
      }

      this.password = '';
    });
  }
}
