export interface CdnFile {
  readonly href: string;
  readonly integrity: string;
}

export const PICO_CSS: CdnFile = {
  href: "https://cdn.jsdelivr.net/npm/@picocss/pico@2.1.1/css/pico.classless.min.css",
  integrity: "sha384-NZhm4G1I7BpEGdjDKnzEfy3d78xvy7ECKUwwnKTYi036z42IyF056PbHfpQLIYgL",
};

export const SWAGGER_CSS: CdnFile = {
  href: "https://unpkg.com/swagger-ui-dist@5.33.2/swagger-ui.css",
  integrity: "sha384-Ov4/wv3j2bmct8cDc5X4ngJZohVPzEmc6uDPH8WeljUxO5vtoykvMEfbu9Vh6RaW",
};

export const SWAGGER_JS: CdnFile = {
  href: "https://unpkg.com/swagger-ui-dist@5.33.2/swagger-ui-bundle.js",
  integrity: "sha384-gNnUp++d7WqR9OKX4OMoUmV2w0JxEs1uu6NPXgcynGClIX5DOu6ol4Lp+lINyZcJ",
};

export const CDN_FILES: Readonly<Record<string, CdnFile>> = {
  PICO_CSS,
  SWAGGER_CSS,
  SWAGGER_JS,
};
