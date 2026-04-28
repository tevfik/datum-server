export class DatumError extends Error {
  constructor(public statusCode: number, public body: string) {
    super(`DatumError(${statusCode}): ${body}`);
    this.name = "DatumError";
  }
}
