import { Contract, requestUrl } from './contract';
import type { ReceivedResponse, RequestCase } from './contract';

export class E2eClient {
  constructor(
    private readonly base: URL,
    private readonly contract: Contract,
    private readonly token?: string,
    private readonly transport: typeof fetch = fetch
  ) {}

  async exchange(test: RequestCase, step: string): Promise<ReceivedResponse> {
    this.contract.assertRequest(test);

    return this.send(test, step);
  }

  // Negative-input scenarios must prove the request is invalid before sending it.
  async requestInvalid(test: RequestCase, step: string): Promise<void> {
    let invalid = false;

    try {
      this.contract.assertRequest(test);
    } catch {
      invalid = true;
    }

    if (!invalid) {
      throw new Error(`${step}: expected a schema-invalid request`);
    }

    const received = await this.send(test, step);

    if (received.status !== 400) {
      throw new Error(`${step}: expected status 400, received ${received.status}`);
    }
  }

  private async send(test: RequestCase, step: string): Promise<ReceivedResponse> {
    const label = `${step}: ${test.method.toUpperCase()} ${test.path}${test.query ? ` ${JSON.stringify(test.query)}` : ''}`;

    let received: ReceivedResponse;

    try {
      const response = await this.transport(requestUrl(this.base, test), {
        method: test.method.toUpperCase(),
        headers: {
          Accept: 'application/json',
          ...(this.token && this.contract.requiresBearer(test)
            ? { Authorization: `Bearer ${this.token}` }
            : {}),
          ...(test.body === undefined ? {} : { 'Content-Type': 'application/json' })
        },
        body: test.body === undefined ? undefined : JSON.stringify(test.body),
        redirect: 'manual',
        signal: AbortSignal.timeout(10000)
      });

      received = {
        status: response.status,
        contentType: response.headers.get('content-type'),
        body: await response.text()
      };
    } catch {
      // Transport errors may contain request headers or bodies; report only the operation.
      throw new Error(`${label}: HTTP exchange failed`);
    }

    try {
      this.contract.assertResponse(test, received);
    } catch (cause) {
      throw new Error(`${label}: contract validation failed (status ${received.status})`, {
        cause
      });
    }

    return received;
  }

  async request(test: RequestCase, expected: number | number[], step: string): Promise<unknown> {
    const received = await this.exchange(test, step);
    const statuses = Array.isArray(expected) ? expected : [expected];

    if (!statuses.includes(received.status)) {
      throw new Error(
        `${step}: ${test.method.toUpperCase()} ${test.path} ${JSON.stringify(test.query ?? {})}: expected status ${statuses.join('/')}, received ${received.status}`
      );
    }

    if (this.contract.expectsJson(test, received.status)) {
      return JSON.parse(received.body);
    }

    return undefined;
  }
}

export function requiredValue(value: unknown, type: 'string', label: string): string;
export function requiredValue(value: unknown, type: 'number', label: string): number;
export function requiredValue(value: unknown, type: 'string' | 'number', label: string) {
  if (
    typeof value !== type ||
    (type === 'string' && value === '') ||
    (type === 'number' && !Number.isInteger(value))
  ) {
    throw new Error(
      `${label}: required ${type === 'string' ? 'nonempty string' : 'integer'} missing or invalid`
    );
  }

  return value;
}
