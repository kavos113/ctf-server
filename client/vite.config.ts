import { defineConfig, loadEnv } from 'vite';
import aurelia from '@aurelia/vite-plugin';

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '');
  return {
    server: {
      open: !process.env.CI,
      port: 9000,
      proxy: {
        '/api': {
          target: env.API_PROXY_TARGET || 'http://127.0.0.1:8081',
          rewrite: (path) => path.replace(/^\/api/, '')
        }
      }
    },
    esbuild: { target: 'es2022' },
    plugins: [aurelia({ useDev: mode !== 'production' })]
  };
});
