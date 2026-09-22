import type { Credentials } from '../api/api';

export function credentialsError({ username, password }: Credentials): string {
  if (username.length < 3 || username.length > 32 || /[^A-Za-z0-9_-]/.test(username)) {
    return 'ユーザー名は半角英数字・ハイフン・アンダースコアで3〜32文字にしてください。';
  }

  const bytes = new TextEncoder().encode(password).length;

  if (password.includes('\0') || bytes < 8 || bytes > 128) {
    return 'パスワードはUTF-8で8〜128バイトにしてください。NUL文字は使用できません。';
  }

  return '';
}
