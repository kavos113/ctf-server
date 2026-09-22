import { createServer } from 'node:http';
import { createStore } from './store.mjs';
import { handleRequest } from './handlers.mjs';

const scenario = process.env.MOCK_SCENARIO || 'normal';
const store = createStore(scenario);
const delay = Math.max(0, Number(process.env.MOCK_DELAY_MS) || 0);
const port = Number(process.env.MOCK_PORT) || 8081;
createServer(async (request, response) => {
  try {
    const url = new URL(request.url, 'http://localhost');
    let text = '';
    for await (const chunk of request) {
      text += chunk;
      if (text.length > 1_000_000) {
        response.writeHead(413).end();
        return;
      }
    }
    let body;
    try {
      body = text ? JSON.parse(text) : undefined;
    } catch {
      response.writeHead(400).end();
      return;
    }
    const result = handleRequest(
      store,
      {
        method: request.method,
        path: url.pathname,
        query: Object.fromEntries(url.searchParams),
        token: request.headers.authorization?.startsWith('Bearer ')
          ? request.headers.authorization.slice(7)
          : undefined,
        body
      },
      scenario
    );
    if (delay) await new Promise((resolve) => setTimeout(resolve, delay));
    response.writeHead(result.status, {
      'Content-Type': result.raw ? 'text/plain' : 'application/json',
      'Cache-Control': 'no-store'
    });
    response.end(result.raw ?? (result.body === undefined ? '' : JSON.stringify(result.body)));
  } catch {
    response.writeHead(500).end();
  }
}).listen(port, '127.0.0.1', () => {
  console.log(`Development mock: http://127.0.0.1:${port} (${scenario}). Restart to reset data.`);
});
