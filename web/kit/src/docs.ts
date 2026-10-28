export interface SwaggerHost {
  readonly SwaggerUIBundle?: (options: typeof DOCS_OPTIONS) => unknown;
}

export const DOCS_OPTIONS = {
  url: "/api/openapi.json",
  dom_id: "#swagger-ui",
  validatorUrl: null,
  persistAuthorization: false,
  tryItOutEnabled: true,
} as const;

export function mountDocs(host: SwaggerHost): void {
  const bundle = host.SwaggerUIBundle;
  if (bundle === undefined) {
    throw new Error("Swagger UI did not load from the CDN");
  }
  bundle(DOCS_OPTIONS);
}
