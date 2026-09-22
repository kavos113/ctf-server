import type { Answer, CorrectAnswer, PublicChallenge, User } from '../../api/api';
import { errorMessage } from '../../api/http-client';
import { PageState, formatDate, parseId } from '../shared/page-state';
import './detail-page.css';

export class DetailPage extends PageState {
  id?: number;
  challenge?: PublicChallenge;
  users: User[] = [];
  answer = '';
  own: Answer[] = [];
  publicAnswers: CorrectAnswer[] = [];
  historyError = '';
  private submittedCorrectly = false;
  private historyGeneration = 0;
  formatDate = formatDate;

  get creatorName() {
    const id = this.challenge?.creator_id;

    return id ? this.users.find((user) => user.id === id)?.username || id : '作成者不明';
  }

  get solved() {
    return (
      this.allowed &&
      (this.submittedCorrectly ||
        this.own.some((item) => item.challenge_id === this.id && item.correct === true))
    );
  }

  loading(params: Record<string, unknown>) {
    this.reset();
    this.clearPrivate();
    this.id = parseId(params.id);

    return this.refresh();
  }

  clearPrivate() {
    this.historyGeneration++;
    this.answer = '';
    this.own = [];
    this.message = '';
    this.submittedCorrectly = false;
  }

  async refresh() {
    this.challenge = undefined;
    this.users = [];
    this.own = [];
    this.publicAnswers = [];
    this.historyError = '';

    if (this.id === undefined) {
      this.error = '問題IDが不正です。';

      return;
    }

    await this.read(
      () => Promise.all([this.api.challenges(), this.api.users().catch(() => [])]),
      ([items, users]) => {
        this.challenge = items.find((item) => item.id === this.id);
        this.users = users;

        if (!this.challenge) {
          this.error = '問題が見つかりません。';
        }
      }
    );

    if (this.challenge) {
      await this.refreshHistory();
    }
  }

  async refreshHistory() {
    const current = this.checkpoint();
    const generation = ++this.historyGeneration;
    const id = this.id;

    const results = await Promise.allSettled([
      this.api.answers(id),
      this.allowed ? this.api.myAnswers(id) : Promise.resolve([])
    ]);

    if (!current() || generation !== this.historyGeneration || id !== this.id) {
      return;
    }

    this.historyError = '';

    if (results[0].status === 'fulfilled') {
      this.publicAnswers = results[0].value;
    } else {
      this.historyError = errorMessage(results[0].reason);
    }

    if (results[1].status === 'fulfilled') {
      this.own = results[1].value;
    } else {
      this.historyError = errorMessage(results[1].reason);
    }
  }

  async submit() {
    if (!this.allowed || this.id === undefined || !this.challenge) {
      return;
    }

    if (!this.answer) {
      this.error = '解答を入力してください。';

      return;
    }

    await this.write(async (current) => {
      const result = await this.api.answer(this.id!, this.answer);

      if (!current()) {
        return;
      }

      this.message = result.correct ? '正解' : '不正解';
      this.submittedCorrectly ||= result.correct === true;
      this.answer = '';

      await this.refreshHistory();
    });
  }
}
