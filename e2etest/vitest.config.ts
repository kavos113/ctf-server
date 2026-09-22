import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',
    fileParallelism: false,
    testTimeout: 15000,
    hookTimeout: 15000,
    retry: 0
  }
});
