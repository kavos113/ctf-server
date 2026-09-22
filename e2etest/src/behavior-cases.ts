import type { components } from './generated/schema';

type Description = Pick<components['schemas']['CreateChallengeRequest'], 'description'>;

export const descriptionCases = [
  { label: 'plain', description: 'plain description' },
  { label: 'quotes', description: '説明に "quoted text" を含む' },
  { label: 'backslashes', description: 'C:\\tmp\\challenge\\' },
  { label: 'line breaks and tab', description: '1行目\n2行目\r\n3行目\t末尾' },
  { label: 'literal escapes', description: 'literal \\n and \\t' },
  { label: 'unicode', description: '暗号の問題 🔐 café' },
  { label: 'apostrophes', description: "O'Reilly's challenge" },
  { label: 'JSON-like text', description: '説明: {"key":"value"}, [1,2] & <tag> + % ? #' },
  { label: 'whitespace', description: '  説明の前後に空白  ' },
  { label: 'empty', description: '' },
  { label: 'mixed', description: '"引用" と \\ と\n日本語 🔐 と O\'Reilly' }
] satisfies (Description & { label: string })[];
