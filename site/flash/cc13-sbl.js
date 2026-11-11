// TI CC13xx/CC26xx (CC1354P10) flashing through the chip's ROM serial bootloader, over the probe's backchannel UART (Web Serial in the page).
// WHY NOT THE DEBUGGER: the CC1354P10's debug port is 2-pin cJTAG behind an ICEPick router. The XDS110's CMSIS-DAP interface (what mspm0-dap.js and
// cc-saci.js use) gets no answer to SWD (ack 7 at 1 MHz, with and without the dormant wake, right after a power-on) and reads IDCODE 0 in JTAG mode
// (LS45013A, 2026-10-22). Only TI's own DSLite/CCStudio speak cJTAG on it.
// WHAT THIS IS: the framing and commands of the ROM bootloader as TI documents them for the CC13x2/CC26x2 family and the CC13x4 TRM (autobaud 0x55 0x55,
// packets [size, checksum, command, data...], ACK 0xCC, NACK 0x33). NOT YET RUN ON A BOARD: the bootloader only starts when the board is reset with
// its bootloader pin low (BL_CONFIG in the CCFG; on the LaunchPad the button held while RESET is released), which needs a hand. The unit test
// (tools/webflash/cc13-sbl.test.mjs) runs it against a fake bootloader.
// Never: a write at or above 0x50000000 (the CCFG), CMD_SET_CCFG, a bank erase.
export const CMD = { PING: 0x20, DOWNLOAD: 0x21, GET_STATUS: 0x23, SEND_DATA: 0x24, RESET: 0x25, SECTOR_ERASE: 0x26, CRC32: 0x27, GET_CHIP_ID: 0x28, MEMORY_READ: 0x2A };
export const ACK = 0xCC, NACK = 0x33;
export const STATUS = { 0x40: "success", 0x41: "unknown command", 0x42: "invalid command", 0x43: "invalid address", 0x44: "flash fail" };
export const MAIN_FLASH_END = 0x100000;      // 1 MB; the CCFG is a separate area and is never written here
export const ERASE_STEP = 2048;              // the smallest sector; all erases run before any write, so a larger real sector is just erased more than once
const be32 = (v) => [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff];
const sum = (b) => b.reduce((x, y) => (x + y) & 0xff, 0);
export function packet(cmd, data = []) { const p = [cmd, ...data]; return Uint8Array.from([p.length + 2, sum(p), ...p]); }

// io = { write(Uint8Array), read(n, ms) -> Uint8Array (throws on timeout) }
export class Sbl {
  constructor(io, log = () => {}) { this.io = io; this.log = log; }
  async ack(ms = 1000) { const r = await this.io.read(1, ms); if (r[0] === ACK) return; throw new Error(r[0] === NACK ? "the bootloader refused the command (NACK)" : `unexpected byte 0x${r[0].toString(16)}`); }
  async send(cmd, data = [], ms = 1000) { await this.io.write(packet(cmd, data)); await this.ack(ms); }
  async reply(n, ms = 1000) {
    const h = await this.io.read(2, ms), body = await this.io.read(h[0] - 2, ms);
    if (body.length !== n || sum([...body]) !== h[1]) throw new Error("the bootloader's answer is damaged");
    await this.io.write(Uint8Array.of(ACK));
    return body;
  }
  // Autobaud: two 0x55 bytes, then the bootloader answers ACK. Fails (times out) when the chip is not in its bootloader.
  async connect() { await this.io.write(Uint8Array.of(0x55, 0x55)); await this.ack(1500); }
  async ping() { await this.send(CMD.PING); }
  async chipId() { await this.send(CMD.GET_CHIP_ID); const b = await this.reply(4); return ((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) >>> 0; }
  async status() { await this.send(CMD.GET_STATUS); return (await this.reply(1))[0]; }
  async must(what) { const s = await this.status(); if (s !== 0x40) throw new Error(`${what}: ${STATUS[s] || "status 0x" + s.toString(16)}`); }
  async read32(addr, words) {
    const out = new Uint8Array(words * 4);
    for (let o = 0; o < words; o += 63) {
      const n = Math.min(63, words - o);
      await this.send(CMD.MEMORY_READ, [...be32(addr + o * 4), 1, n]);
      out.set(await this.reply(n * 4), o * 4);
    }
    return out;
  }
  async crc32(addr, size) { await this.send(CMD.CRC32, [...be32(addr), ...be32(size), ...be32(0)]); const b = await this.reply(4); return ((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) >>> 0; }
  // segments: [{addr, data: Uint8Array}] inside main flash. Erases every touched sector first, then downloads each segment and checks the chip's CRC32.
  async write(segments, progress = () => {}, crcOf) {
    for (const s of segments) if (s.addr + s.data.length > MAIN_FLASH_END) throw new Error(`refusing: 0x${s.addr.toString(16)} is outside main flash (the CCFG is never written here)`);
    const sectors = new Set();
    for (const s of segments) for (let a = s.addr - (s.addr % ERASE_STEP); a < s.addr + s.data.length; a += ERASE_STEP) sectors.add(a);
    let k = 0;
    for (const a of [...sectors].sort((x, y) => x - y)) { await this.send(CMD.SECTOR_ERASE, be32(a), 4000); await this.must(`erase 0x${a.toString(16)}`); progress(0.3 * ++k / sectors.size); }
    let done = 0; const total = segments.reduce((n, s) => n + s.data.length, 0);
    for (const s of segments) {
      await this.send(CMD.DOWNLOAD, [...be32(s.addr), ...be32(s.data.length)]); await this.must("download start");
      for (let o = 0; o < s.data.length; o += 252) {
        const chunk = s.data.subarray(o, o + 252);
        let ok = false;
        for (let t = 1; t <= 3 && !ok; t++) {      // 3 tries per block, 150 ms apart
          try { await this.send(CMD.SEND_DATA, [...chunk]); await this.must(`write 0x${(s.addr + o).toString(16)}`); ok = true; }
          catch (e) { if (t === 3) throw e; await new Promise((r) => setTimeout(r, 150)); }
        }
        done += chunk.length; progress(0.3 + 0.7 * done / total);
      }
      const want = (crcOf || (() => null))(s.data);
      if (want !== null && (await this.crc32(s.addr, s.data.length)) !== want) throw new Error(`the chip's CRC32 of 0x${s.addr.toString(16)} does not match what was sent`);
    }
  }
  async reset() { await this.io.write(packet(CMD.RESET)); await this.ack(1000).catch(() => {}); }
}
