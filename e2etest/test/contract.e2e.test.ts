import { beforeAll, describe, it } from 'vitest';
import { createCases } from '../src/cases';
import { baseUrl, Contract, loadContract, requestUrl } from '../src/contract';

const cases = createCases();
let contract: Contract;
let base: URL;

beforeAll(async () => {
  base = baseUrl(process.env.E2E_BASE_URL);
  contract = new Contract(await loadContract());
  contract.assertCoverage(cases);
  for (const test of cases) contract.assertRequest(test);
});

describe('OpenAPI contract', { concurrent: false }, () => {
  for (const test of cases) {
    const label = `${test.method.toUpperCase()} ${test.path}${test.query ? ` ${JSON.stringify(test.query)}` : ''}`;
    it(label, async () => {
      const url = requestUrl(base, test);
      let response: Response;
      let body: string;
      try {
        response = await fetch(url, {
          method: test.method.toUpperCase(),
          headers: {
            Accept: 'application/json',
            ...(test.body === undefined ? {} : { 'Content-Type': 'application/json' })
          },
          body: test.body === undefined ? undefined : JSON.stringify(test.body),
          redirect: 'manual',
          signal: AbortSignal.timeout(10000)
        });
        body = await response.text();
      } catch (cause) {
        throw new Error(`${label}: HTTP exchange failed for ${url}`, { cause });
      }
      contract.assertResponse(test, {
        status: response.status,
        contentType: response.headers.get('content-type'),
        body
      });
    });
  }
});
