# CC1407P wire packer: the same octets on a 16-bit-char chip

A field-by-field packer gives the CC1407P host driver the same SPI bytes on the F2800137 as on a Mac: 17 of 17 cases equal, 315 octets, same digest. TI's host driver puts C structs and bit-fields on the SPI wire. That is right where a byte is 8 bits and enums are 1 byte, and wrong on the C28x, where a char is 16 bits ([the shim run](../c28x/README.md): 9 units against 10 bytes). `trx_wire.c` writes and reads each field at an explicit octet offset, so the stream is the same on the F2800137 and on a Mac. Run 2026-10-25 on a LAUNCHXL-F2800137 (C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, RAM load, no radio). Terms: [docs/vocab.md](../../../docs/vocab.md).

| File | What it is |
|---|---|
| `trx_wire.h`, `trx_wire.c` | The packer and unpacker. One octet is one `trx_octet` array element (a byte on Arm, a 16-bit word with the top 8 bits zero on the C28x). Inputs are `uint16_t`/`uint32_t`, never `uint8_t` or bit-fields. 677 words (1.3 kB) of code on the C28x |
| `scenario.h` | The field values both sides use (the TI TX sample's sequence) |
| `ref_gen.c` | Builds each message from TI's own structs on the Mac (`-fshort-enums`, as the Arm builds use) and writes `vectors.h` |
| `test_wire.c` | One test source for both targets: repacks every vector and counts mismatches |
| `build_host.sh`, `build_c28x.sh`, `c2krun_wire.js`, `wire_main_c28x.c` | Mac build and run; C28x build; the debugger runner that reads the result back |
| `vectors.h` | The committed reference vectors |

## What is covered

Transaction header, 4-byte padding and big-endian CRC-16; CONFIG_LIST, NV_ERASE, PING, DEVICE_CONFIG_STORE, CONFIG_STORE (two fragments of the front-end table), CONFIG_PERSIST, CMD_STORE (TX parameters, every branch and schedule field), CMD_SUBMIT, CMD_STOP, STREAM_STORE with the Sun FSK payload header; and, from the radio, the transaction header, CRC check, request header, LAST_STATUS, CMD_STATUS and stream descriptor. Not covered: the RX, test, carrier-sense and extended command parameter layouts (only the TX layout is written), and the driver's own chunking and buffer logic, which still runs on `sizeof` and `uint8_t` and needs a port to call the packer.

## Result

| Check | Mac | F2800137 |
|---|---|---|
| Cases equal to the reference | 17 of 17 | 17 of 17 |
| Octets packed | 315 | 315 |
| FNV-1a of the stream | DC1DF5B1 | DC1DF5B1 |
| Stream bytes, compared octet by octet | written to a file | read from RAM over the debugger: identical |
| First transaction | `00 04 11 00 02 32 00 00 E9 71` (TI's unmodified driver) | same 10 octets |
| Test with one offset moved by hand | fails 1 case | not run |

Reference: TI's structs on the Mac plus the first transaction of TI's unmodified driver against a fake radio. It is not a capture of the full driver flow, because the driver blocks in `TRX_Host_open` until a radio answers. No radio was attached; nothing was sent over the air.

## Traps found on the way

1. `LRF_*_byteCount` is `sizeof` of a `uint32_t` array, so on the C28x it is words (2 per 32-bit value), half the octet count. Use 4 per word with `trx_w_words_to_octets`.
2. `TRX_PayloadHeader_SunFSK` is a bit-field struct cast over a byte array in `app_tx.c`; use `trx_w_payload_hdr_sunfsk`.
3. The wire layout assumes 1-byte enums. A Mac build without `-fshort-enums` gets a 42-byte DeviceConfigData instead of 12.
4. A breakpoint on the empty `wire_halt` function did not stop the run in CCStudio scripting; run free, wait, halt instead (`c2krun_wire.js`).

## Run it

```bash
portability/cc1407p-host/wire/build_host.sh <outdir>                # regenerates vectors.h, runs the Mac test
portability/cc1407p-host/wire/build_c28x.sh <outdir> <outdir>       # RAM image
/Applications/ti/ccs2101/ccs/scripting/run.sh \
  portability/cc1407p-host/wire/c2krun_wire.js <outdir>/wire_c28x.out c2000-launchpad/tools/F2800137_CL371081.ccxml <result.txt>
```
