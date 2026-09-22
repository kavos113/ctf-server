import type { Answer, CorrectAnswer, PublicChallenge } from '../api/api';
import { errorMessage } from '../api/http-client';
import { PageState, formatDate, parseId } from './page-state';
export class DetailPage extends PageState {
  id?: number;
  challenge?: PublicChallenge;
  answer = '';
  own: Answer[] = [];
  publicAnswers: CorrectAnswer[] = [];
  historyError = '';
  private historyGeneration = 0;
  formatDate = formatDate;
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
  }
  async refresh() {
    this.challenge = undefined;
    this.own = [];
    this.publicAnswers = [];
    this.historyError = '';
    if (this.id === undefined) {
      this.error = '問題IDが不正です。';
      return;
    }
    await this.read(
      () => this.api.challenges(),
      (items) => {
        this.challenge = items.find((item) => item.id === this.id);
        if (!this.challenge) this.error = '問題が見つかりません。';
      }
    );
    if (this.challenge) await this.refreshHistory();
  }
  async refreshHistory() {
    const current = this.checkpoint();
    const generation = ++this.historyGeneration;
    const id = this.id;
    const results = await Promise.allSettled([
      this.api.answers(id),
      this.allowed ? this.api.myAnswers(id) : Promise.resolve([])
    ]);
    if (!current() || generation !== this.historyGeneration || id !== this.id) return;
    this.historyError = '';
    if (results[0].status === 'fulfilled') this.publicAnswers = results[0].value;
    else this.historyError = errorMessage(results[0].reason);
    if (results[1].status === 'fulfilled') this.own = results[1].value;
    else this.historyError = errorMessage(results[1].reason);
  }
  async submit() {
    if (!this.allowed || this.id === undefined || !this.challenge) return;
    if (!this.answer) {
      this.error = '解答を入力してください。';
      return;
    }
    await this.write(async (current) => {
      const result = await this.api.answer(this.id!, this.answer);
      if (!current()) return;
      this.message = result.correct ? '正解です！' : '不正解です。もう一度挑戦できます。';
      this.answer = '';
      await this.refreshHistory();
    });
  }
}
