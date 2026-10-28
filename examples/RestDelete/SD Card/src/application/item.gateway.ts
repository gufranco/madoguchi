export interface ItemGateway {
  remove(id: number): Promise<void>;
}
