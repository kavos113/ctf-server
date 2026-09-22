import SwaggerParser from '@apidevtools/swagger-parser';
import Ajv, { type AnySchema } from 'ajv';
import addFormats from 'ajv-formats';
import type { OpenAPIV3 } from 'openapi-types';
import { fileURLToPath } from 'node:url';

export interface RequestCase {
  method: string;
  path: string;
  query?: Record<string, unknown>;
  body?: unknown;
}

export interface ReceivedResponse {
  status: number;
  contentType: string | null;
  body: string;
}

const methods = new Set(['get', 'post', 'put', 'delete', 'patch', 'head', 'options', 'trace']);

export async function loadContract(): Promise<OpenAPIV3.Document> {
  const filename = fileURLToPath(new URL('../../docs/openapi.yaml', import.meta.url));
  return (await SwaggerParser.validate(filename, {
    resolve: { external: false },
    dereference: { circular: false }
  })) as OpenAPIV3.Document;
}

export class Contract {
  private readonly ajv = new Ajv({ allErrors: true, strict: false });

  constructor(private readonly document: OpenAPIV3.Document) {
    addFormats(this.ajv);
  }

  assertCoverage(cases: RequestCase[]): void {
    const expected = Object.entries(this.document.paths).flatMap(([path, item]) =>
      Object.keys(item ?? {})
        .filter((method) => methods.has(method))
        .map((method) => `${method} ${path}`)
    );
    const actual = new Set(cases.map((test) => `${test.method} ${test.path}`));
    const missing = expected.filter((operation) => !actual.has(operation));
    const extra = [...actual].filter((operation) => !expected.includes(operation));
    if (missing.length || extra.length) {
      throw new Error(
        `Operation coverage: missing [${missing.join(', ')}], extra [${extra.join(', ')}]`
      );
    }
  }

  private operation(test: RequestCase): OpenAPIV3.OperationObject {
    const item = this.document.paths[test.path];
    const operation = item?.[test.method as OpenAPIV3.HttpMethods];
    if (!operation) throw new Error(`Unknown operation: ${test.method.toUpperCase()} ${test.path}`);
    return operation;
  }

  private validate(
    schema: OpenAPIV3.SchemaObject | undefined,
    value: unknown,
    context: string
  ): void {
    if (!schema) return;
    const validate = this.ajv.compile(schema as AnySchema);
    if (!validate(value)) {
      throw new Error(
        `${context}: ${this.ajv.errorsText(validate.errors, { dataVar: '$', separator: '; ' })}`
      );
    }
  }

  assertRequest(test: RequestCase): void {
    const operation = this.operation(test);
    const label = `${test.method.toUpperCase()} ${test.path} request`;
    const parameters = [
      ...(this.document.paths[test.path]?.parameters ?? []),
      ...(operation.parameters ?? [])
    ] as OpenAPIV3.ParameterObject[];
    for (const parameter of parameters) {
      if (parameter.in !== 'query') continue;
      const value = test.query?.[parameter.name];
      if (value === undefined) {
        if (parameter.required) throw new Error(`${label} query.${parameter.name}: required`);
      } else {
        this.validate(
          parameter.schema as OpenAPIV3.SchemaObject,
          value,
          `${label} query.${parameter.name}`
        );
      }
    }
    const body = operation.requestBody as OpenAPIV3.RequestBodyObject | undefined;
    if (test.body === undefined) {
      if (body?.required) throw new Error(`${label} body: required`);
    } else {
      const media = body?.content['application/json'];
      if (!media) throw new Error(`${label}: application/json body is not defined`);
      this.validate(media.schema as OpenAPIV3.SchemaObject, test.body, `${label} body`);
    }
  }

  assertResponse(test: RequestCase, received: ReceivedResponse): void {
    const operation = this.operation(test);
    const label = `${test.method.toUpperCase()} ${test.path} response ${received.status}`;
    const response = (operation.responses[String(received.status)] ??
      operation.responses[`${Math.floor(received.status / 100)}XX`] ??
      operation.responses.default) as OpenAPIV3.ResponseObject | undefined;
    if (!response)
      throw new Error(
        `${label}: undefined status (defined: ${Object.keys(operation.responses).join(', ')})`
      );
    if (!response.content || Object.keys(response.content).length === 0) return;
    const contentType = received.contentType?.split(';', 1)[0].trim().toLowerCase();
    const media = contentType ? response.content[contentType] : undefined;
    if (!media)
      throw new Error(`${label}: unexpected Content-Type ${received.contentType ?? '(missing)'}`);
    if (contentType !== 'application/json')
      throw new Error(`${label}: unsupported media type ${contentType}`);
    let value: unknown;
    try {
      value = JSON.parse(received.body);
    } catch {
      throw new Error(`${label} body: invalid JSON`);
    }
    this.validate(media.schema as OpenAPIV3.SchemaObject, value, `${label} body`);
  }
}

export function baseUrl(value: string | undefined): URL {
  if (!value) throw new Error('E2E_BASE_URL is required; use a disposable test server');
  const url = new URL(value);
  if (
    !['http:', 'https:'].includes(url.protocol) ||
    url.search ||
    url.hash ||
    url.username ||
    url.password
  ) {
    throw new Error('E2E_BASE_URL must be an HTTP(S) URL without credentials, query or fragment');
  }
  url.pathname = `${url.pathname.replace(/\/$/, '')}/`;
  return url;
}

export function requestUrl(base: URL, test: RequestCase): URL {
  const url = new URL(test.path.replace(/^\//, ''), base);
  for (const [name, value] of Object.entries(test.query ?? {})) {
    if (value !== undefined) url.searchParams.set(name, String(value));
  }
  return url;
}
