import { marked } from 'marked';
import DOMPurify from 'dompurify';

export function renderMarkdown(source?: string): string {
  const html = marked.parse(source || '説明はありません。', {
    async: false,
    gfm: true,
    breaks: true
  });

  return DOMPurify.sanitize(html, {
    ALLOWED_TAGS: [
      'p',
      'br',
      'hr',
      'h1',
      'h2',
      'h3',
      'h4',
      'h5',
      'h6',
      'strong',
      'em',
      'del',
      'blockquote',
      'ul',
      'ol',
      'li',
      'pre',
      'code',
      'a',
      'img',
      'table',
      'thead',
      'tbody',
      'tr',
      'th',
      'td'
    ],
    ALLOWED_ATTR: ['href', 'src', 'alt', 'title', 'start', 'align'],
    ALLOW_DATA_ATTR: false,
    ALLOW_ARIA_ATTR: false
  });
}
