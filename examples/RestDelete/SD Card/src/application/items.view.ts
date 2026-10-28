export interface ItemsView {
  requestedId(): number;
  showDeleted(id: number): void;
}
