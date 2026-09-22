import type { ChallengeInput } from '../api/api';
import { PageState, parseId } from './page-state';
export class EditorPage extends PageState {
  id?: number;
  editing = false;
  ready = false;
  confirmDelete = false;
  form: ChallengeInput = {};
  canLoad() {
    return this.allowed ? true : 'login';
  }
  loading(params: Record<string, unknown>) {
    this.reset();
    this.clearPrivate();
    this.editing = params.id !== undefined;
    this.id = parseId(params.id);
    return this.refresh();
  }
  clearPrivate() {
    this.form = {};
    this.ready = false;
    this.confirmDelete = false;
  }
  async refresh() {
    this.ready = false;
    if (!this.allowed) return;
    if (!this.editing) {
      this.form = {};
      this.ready = true;
      return;
    }
    if (this.id === undefined) {
      this.error = '問題IDが不正です。';
      return;
    }
    await this.read(
      () => this.api.myChallenges(),
      (items) => {
        const item = items.find((item) => item.id === this.id);
        if (!item) {
          this.error = '編集できる問題が見つかりません。';
          return;
        }
        this.form = {
          name: item.name,
          description: item.description,
          genre: item.genre,
          flag: item.flag
        };
        this.ready = true;
      }
    );
  }
  async save() {
    if (!this.allowed || !this.ready) return;
    if (
      !this.form.name?.trim() ||
      !this.form.description?.trim() ||
      !this.form.genre?.trim() ||
      !this.form.flag
    ) {
      this.error = 'すべての項目を入力してください。';
      return;
    }
    await this.write(async (current) => {
      await this.api.saveChallenge({ ...this.form }, this.id);
      if (!current()) return;
      this.form = {};
      window.location.hash = '/me/challenges';
    });
  }
  async remove() {
    if (!this.allowed || !this.confirmDelete || this.id === undefined) return;
    await this.write(async (current) => {
      await this.api.deleteChallenge(this.id!);
      if (!current()) return;
      window.location.hash = '/me/challenges';
    });
  }
}
