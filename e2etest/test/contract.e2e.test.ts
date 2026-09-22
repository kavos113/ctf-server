import { afterAll, beforeAll, describe, it } from 'vitest';
import { createCases, createCredentials } from '../src/cases';
import { baseUrl, Contract, loadContract } from '../src/contract';
import { E2eClient, requiredValue } from '../src/e2e-client';

const cases = createCases().sort(
  (a, b) => Number(a.path === '/logout') - Number(b.path === '/logout')
);
let contract: Contract;
let client: E2eClient;

beforeAll(async () => {
  const base = baseUrl(process.env.E2E_BASE_URL);

  contract = new Contract(await loadContract());
  contract.assertCoverage(cases);

  for (const test of cases) {
    contract.assertRequest(test);
  }

  const publicClient = new E2eClient(base, contract);
  const credentials = createCredentials();

  await publicClient.request(
    { method: 'post', path: '/signup', body: credentials },
    201,
    'setup signup'
  );
  const login = (await publicClient.request(
    { method: 'post', path: '/login', body: credentials },
    200,
    'setup login'
  )) as { token: string };

  client = new E2eClient(base, contract, requiredValue(login.token, 'string', 'setup token'));
}, 30000);

afterAll(async () => {
  if (client) {
    await client.request({ method: 'post', path: '/logout' }, [200, 401], 'cleanup session');
  }
}, 15000);

describe('OpenAPI contract', { concurrent: false }, () => {
  for (const test of cases) {
    const label = `${test.method.toUpperCase()} ${test.path}${test.query ? ` ${JSON.stringify(test.query)}` : ''}`;

    it(
      label,
      async () => {
        await client.exchange(test, label);
      },
      15000
    );
  }
});
