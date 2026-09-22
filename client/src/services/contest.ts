/// <reference types="vite/client" />

export function contestState(value = import.meta.env.VITE_CONTEST_START_AT, now = Date.now()) {
  if (!value?.trim()) {
    return { started: true, notice: '' };
  }

  const start = Date.parse(value);

  if (
    !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/.test(value) ||
    !Number.isFinite(start)
  ) {
    return { started: false, notice: '開始日時の設定を確認中です。' };
  }

  return {
    started: now >= start,
    notice: `コンテスト開始前です。開始日時: ${new Date(start).toLocaleString('ja-JP', { timeZoneName: 'short' })}`
  };
}
