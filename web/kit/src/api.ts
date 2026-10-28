export interface Problem {
  readonly title?: string;
  readonly detail?: string;
  readonly status?: number;
}

export interface ApiFailure {
  readonly status: number;
}

export function apiError(status: number, message: string): Error {
  const cause: ApiFailure = { status };
  return new Error(message, { cause });
}

export function failureStatus(error: unknown): number | undefined {
  const cause = error instanceof Error ? error.cause : undefined;
  const status =
    typeof cause === "object" && cause !== null ? (cause as Partial<ApiFailure>).status : undefined;
  return typeof status === "number" ? status : undefined;
}

export type Fetcher = (path: string, init: RequestInit) => Promise<Response>;
export type Check<T> = (value: unknown) => value is T;

const PROBLEM_TYPE = "application/problem+json";

function asProblem(value: unknown): Problem | undefined {
  if (typeof value !== "object" || value === null) {
    return undefined;
  }
  const fields = value as Record<string, unknown>;
  const pick = (key: string) => (typeof fields[key] === "string" ? { [key]: fields[key] } : {});
  const status = typeof fields["status"] === "number" ? { status: fields["status"] } : {};
  return { ...pick("title"), ...pick("detail"), ...status } as Problem;
}

export async function readProblem(response: Response): Promise<Problem | undefined> {
  const type = response.headers.get("Content-Type") ?? "";
  if (!type.startsWith(PROBLEM_TYPE)) {
    return undefined;
  }
  try {
    return asProblem(await response.json());
  } catch {
    return undefined;
  }
}

export function describeFailure(
  method: string,
  path: string,
  status: number,
  problem: Problem | undefined,
): string {
  const reason = problem?.detail ?? problem?.title;
  return `${method} ${path} answered ${status}${reason === undefined ? "" : `: ${reason}`}`;
}

type Init = Omit<RequestInit, "method">;

async function send(fetcher: Fetcher, method: string, path: string, init: Init): Promise<Response> {
  const response = await fetcher(path, { ...init, method });
  if (!response.ok) {
    const problem = await readProblem(response);
    throw apiError(response.status, describeFailure(method, path, response.status, problem));
  }
  return response;
}

export async function requestJson<T>(
  fetcher: Fetcher,
  method: string,
  path: string,
  check: Check<T>,
  init: Init = {},
): Promise<T | null> {
  const response = await send(fetcher, method, path, init);
  if (response.status === 204) {
    return null;
  }
  const body: unknown = await response.json();
  if (!check(body)) {
    throw apiError(response.status, `${method} ${path} answered a body of the wrong shape`);
  }
  return body;
}

export async function readJson<T>(
  fetcher: Fetcher,
  method: string,
  path: string,
  check: Check<T>,
  init: Init = {},
): Promise<T> {
  const body = await requestJson(fetcher, method, path, check, init);
  if (body === null) {
    throw apiError(204, `${method} ${path} answered no body`);
  }
  return body;
}

export async function sendCommand(
  fetcher: Fetcher,
  method: string,
  path: string,
  init: Init = {},
): Promise<void> {
  await send(fetcher, method, path, init);
}

export async function readText(
  fetcher: Fetcher,
  method: string,
  path: string,
  init: Init = {},
): Promise<string> {
  const response = await send(fetcher, method, path, init);
  return response.text();
}
