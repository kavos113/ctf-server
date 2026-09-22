import { describe, it, expect } from 'vitest';
import { credentialsError } from '../../src/services/credentials';

describe('credentials', () => {
  it('credentialsError follows username and UTF-8 password constraints without trimming', () => {
    for (const [username, password, valid] of [
      ['abc', '12345678', true],
      ['A_-'.repeat(10) + 'ab', 'a'.repeat(128), true],
      ['abc', 'あ'.repeat(42) + 'ab', true],
      ['abc', 'あ'.repeat(43), false],
      ['abc', '1234567', false],
      ['abc', 'a'.repeat(129), false],
      ['abc', '12345678\0', false],
      ['abc', '        ', true],
      ['ab', '12345678', false],
      ['a'.repeat(33), '12345678', false],
      [' abc', '12345678', false],
      ['日本語', '12345678', false],
      ['abc\n', '12345678', false]
    ] as const) {
      expect(credentialsError({ username, password }) === '').toBe(valid);
    }
  });
});
