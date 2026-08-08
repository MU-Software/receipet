/*
 * Types for the Emscripten build of receipet.  Hand-written: this is plain C
 * exports driven by pointer arithmetic, not embind, so --emit-tsd has nothing
 * to describe.  What the arguments mean is in include/receipet.h; repeating it
 * here would be a second copy that nothing checks.  Sizes and offsets are in
 * "@mu-software/receipet/constants", generated from the header.
 */

/** A pointer into the wasm heap.  0 is NULL. */
export type Ptr = number;
export type ImagePtr = Ptr;
export type BrotherStatusPtr = Ptr;

/** A rcpt_buf: allocate BUF_SIZEOF bytes, read it at the BUF_*_OFFSET constants. */
export type BufPtr = Ptr;

/** 0 ok, -1 bad argument, -2 out of memory, -3 a bug in the library - OK and ERR_* in the constants module. */
export type RcptResult = 0 | -1 | -2 | -3;

export type ValueType = 'i8' | 'i16' | 'i32' | 'i64' | 'float' | 'double' | '*';

export interface ReceipetModule {
  /** Replaced when memory grows: re-read it after allocating, and copy bytes out rather than keeping a view. */
  readonly HEAPU8: Uint8Array;
  /** The same heap in 32-bit words: index by `ptr >>> 2`. */
  readonly HEAPU32: Uint32Array;

  _malloc(size: number): Ptr;
  _free(ptr: Ptr): void;

  getValue(ptr: Ptr, type: ValueType): number;
  setValue(ptr: Ptr, value: number, type: ValueType): void;

  ccall(name: string, returnType: string | null, argTypes: string[], args: unknown[]): unknown;
  cwrap(name: string, returnType: string | null, argTypes: string[]): (...args: unknown[]) => unknown;

  /** Writes the image pointer through `out`, a 4-byte slot. */
  _rcpt_image_new(out: Ptr, width: number, height: number): RcptResult;
  _rcpt_image_free(img: ImagePtr): void;
  _rcpt_image_clear(img: ImagePtr): void;
  _rcpt_image_set_pixel(img: ImagePtr, x: number, y: number, value: number): void;
  _rcpt_image_get_pixel(img: ImagePtr, x: number, y: number): number;
  _rcpt_buf_free(buf: BufPtr): void;

  /** Each fills `out`: allocate BUF_SIZEOF bytes, then hand that same pointer to _rcpt_buf_free. */
  _rcpt_enc_escpos(out: BufPtr, img: ImagePtr, chunk_rows: number, flags: number): RcptResult;
  _rcpt_enc_escp(out: BufPtr, img: ImagePtr, mode: number, flags: number): RcptResult;
  _rcpt_enc_tspl(out: BufPtr, img: ImagePtr, x: number, y: number, mode: number, chunk_rows: number, copies: number, flags: number): RcptResult;
  _rcpt_enc_cpcl(out: BufPtr, img: ImagePtr, x: number, y: number, dpi: number, copies: number, flags: number): RcptResult;
  _rcpt_enc_zpl(out: BufPtr, img: ImagePtr, x: number, y: number, copies: number, flags: number): RcptResult;
  _rcpt_enc_brother(out: BufPtr, img: ImagePtr, media_type: number, width_mm: number, length_mm: number, offset: number, flags: number): RcptResult;

  /** `out` is BROTHER_STATUS_SIZEOF bytes; read its fields at the BROTHER_STATUS_*_OFFSET constants. */
  _rcpt_parse_brother_status(out: BrotherStatusPtr, data: Ptr, len: number): RcptResult;

  /** `out` is BROTHER_STATUS_FIELDS uint32s, read off HEAPU32.  Prefer this to the struct form. */
  _rcpt_parse_brother_status_u32(out: Ptr, data: Ptr, len: number): RcptResult;
}

export interface ReceipetModuleOptions {
  print?(text: string): void;
  printErr?(text: string): void;
  onAbort?(what: unknown): void;
  locateFile?(path: string, prefix: string): string;
  wasmBinary?: ArrayBuffer | Uint8Array;
  [key: string]: unknown;
}

declare function createReceipet(options?: ReceipetModuleOptions): Promise<ReceipetModule>;

export default createReceipet;
