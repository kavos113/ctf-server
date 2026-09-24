import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vitest/config';
import aurelia from '@aurelia/vite-plugin';

export default defineConfig({
  plugins: [aurelia({ useDev: true })],
  test: {
    env: { VITE_CONTEST_START_AT: '' },
    environment: 'jsdom',
    watch: false,
    root: fileURLToPath(new URL('./', import.meta.url)),
    setupFiles: ['./test/setup.ts']
  }
});
