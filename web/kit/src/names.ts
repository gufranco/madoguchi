const CARD_NAME = /^[^ ."*/:<>?\\|%][^"*/:<>?\\|%]{0,63}$/;
const PRINTABLE = /^[\x20-\x7e]+$/;
const CARD_PATH_SIZE = 128;
const GZIP_SUFFIX = ".gz";

export function isCardName(name: string): boolean {
  return PRINTABLE.test(name) && CARD_NAME.test(name) && !name.endsWith(".") && !name.endsWith(" ");
}

export function isCardPath(path: string): boolean {
  const fits = `/${path}${GZIP_SUFFIX}`.length < CARD_PATH_SIZE;
  return fits && path.split("/").every(isCardName);
}
