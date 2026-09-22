import { describe, expect, it } from 'vitest';
import { renderMarkdown } from '../../src/services/markdown';

describe('Markdown', () => {
  it('renderMarkdown renders supported syntax and removes executable content', () => {
    const cases: [string | undefined, string, string][] = [
      [undefined, 'p', '説明はありません。'],
      ['', 'p', '説明はありません。'],
      ['# 日本語の見出し', 'h1', '日本語の見出し'],
      ['**強調**', 'strong', '強調'],
      ['*斜体*', 'em', '斜体'],
      ['~~削除~~', 'del', '削除'],
      ['- 項目', 'ul li', '項目'],
      ['1. 項目', 'ol li', '項目'],
      ['> 引用', 'blockquote', '引用'],
      ['`<script>`', 'code', '<script>'],
      ['```html\n<script>alert(1)</script>\n```', 'pre code', '<script>alert(1)</script>'],
      ['[リンク](https://example.com)', 'a[href="https://example.com"]', 'リンク'],
      ['![説明](https://example.com/image.png)', 'img[alt="説明"]', ''],
      ['| 列 |\n| --- |\n| 値 |', 'table td', '値'],
      ['1行目\n2行目', 'br', '']
    ];

    for (const [source, selector, text] of cases) {
      const container = document.createElement('div');

      container.innerHTML = renderMarkdown(source);

      expect(container.querySelector(selector), source).not.toBeNull();
      expect(container.querySelector(selector)?.textContent, source).toContain(text);
    }

    for (const source of [
      '<script>alert(1)</script>',
      '<img src="x" onerror="alert(1)">',
      '[click](javascript:alert%281%29)',
      '<a href="java&#x73;cript:alert(1)" onclick="alert(1)">click</a>',
      '<iframe src="https://example.com"></iframe><svg onload="alert(1)"></svg>',
      '<p style="color:red" click.trigger="attack()" innerhtml.bind="payload">text</p>'
    ]) {
      const container = document.createElement('div');

      container.innerHTML = renderMarkdown(source);

      expect(container.querySelector('script, iframe, svg')).toBeNull();

      for (const element of container.querySelectorAll('*')) {
        for (const attribute of element.attributes) {
          expect(attribute.name).not.toMatch(/^on|\.|^style$/);
          expect(attribute.value).not.toMatch(/^javascript:/i);
        }
      }
    }
  });
});
