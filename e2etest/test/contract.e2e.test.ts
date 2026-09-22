import { beforeAll, describe, it } from 'vitest';
import { createCases } from '../src/cases';
import { baseUrl, Contract, loadContract } from '../src/contract';
import { E2eClient } from '../src/e2e-client';

const cases = createCases();
let contract: Contract;
let client: E2eClient;

beforeAll(async () => {
  const base = baseUrl(process.env.E2E_BASE_URL);

  contract = new Contract(await loadContract());
  client = new E2eClient(base, contract);
  contract.assertCoverage(cases);

  for (const test of cases) {
    contract.assertRequest(test);
  }
});

describe('OpenAPI contract', { concurrent: false }, () => {
  for (const test of cases) {
    const label = `${test.method.toUpperCase()} ${test.path}${test.query ? ` ${JSON.stringify(test.query)}` : ''}`;

    it(label, async () => {
      await client.exchange(test, label);
    });
  }
});
