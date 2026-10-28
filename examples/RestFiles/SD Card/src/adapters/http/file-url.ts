export function fileUrl(name: string): string {
  return `/api/files/${encodeURIComponent(name)}`;
}
