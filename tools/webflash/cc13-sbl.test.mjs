// cc13-sbl.js against a fake ROM bootloader: framing, autobaud, chip id, a write with erase and CRC check, the refusals.
import test from "node:test";
import assert from "node:assert/strict";
import { Sbl, packet, CMD, ACK, MAIN_FLASH_END } from "../../site/flash/cc13-sbl.js";
import { crc32 } from "../../site/flash/cc-saci.js";

function fake() {
  const flash = new Uint8Array(0x20000).fill(0xff), st = { erased: [], dl: null, buf: [], rx: [], log: [] };
  const out = [];
  const reply = (...b) => { const p = [...b]; out.push(p.length + 2, p.reduce((x, y) => x + y, 0) & 0xff, ...p); };
  const io = {
    async write(b) {
      if (b.length === 2 && b[0] === 0x55) { out.push(ACK); return; }
      if (b.length === 1 && b[0] === ACK) return;
      assert.equal(b[0], b.length); assert.equal(b[1], [...b.subarray(2)].reduce((x, y) => x + y, 0) & 0xff);
      const c = b[2], d = [...b.subarray(3)], a = ((d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3]) >>> 0;
      out.push(ACK);
      if (c === CMD.GET_CHIP_ID) reply(0x2B, 0xB6, 0xA0, 0x2F);
      else if (c === CMD.GET_STATUS) reply(0x40);
      else if (c === CMD.SECTOR_ERASE) { st.erased.push(a); flash.fill(0xff, a, a + 2048); }
      else if (c === CMD.DOWNLOAD) st.dl = { a, left: ((d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]) >>> 0 };
      else if (c === CMD.SEND_DATA) { flash.set(d, st.dl.a); st.dl.a += d.length; }
      else if (c === CMD.CRC32) { const n = ((d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7]) >>> 0, v = crc32(flash.subarray(a, a + n)); reply(v >>> 24, (v >>> 16) & 255, (v >>> 8) & 255, v & 255); }
    },
    async read(n) { if (out.length < n) throw new Error("timeout"); return Uint8Array.from(out.splice(0, n)); },
  };
  return { io, flash, st };
}

test("packet framing: size, checksum, command, data", () => {
  assert.deepEqual([...packet(0x20)], [3, 0x20, 0x20]);
  assert.deepEqual([...packet(0x26, [0, 0, 8, 0])], [7, 0x2E, 0x26, 0, 0, 8, 0]);
});
test("connect, chip id, erase before write, send in blocks, CRC32 check", async () => {
  const f = fake(), s = new Sbl(f.io);
  await s.connect(); await s.ping();
  assert.equal(await s.chipId(), 0x2BB6A02F);
  const data = Uint8Array.from({ length: 5000 }, (_, i) => (i * 7) & 0xff);
  await s.write([{ addr: 0x1000, data }], () => {}, crc32);
  assert.deepEqual([...f.flash.subarray(0x1000, 0x1000 + 5000)], [...data]);
  assert.deepEqual(f.st.erased, [0x1000, 0x1800, 0x2000]);
});
test("a write outside main flash (the CCFG) is refused before anything is sent", async () => {
  const f = fake(), s = new Sbl(f.io);
  await assert.rejects(s.write([{ addr: 0x50000000, data: new Uint8Array(4) }]), /CCFG/);
  await assert.rejects(s.write([{ addr: MAIN_FLASH_END - 2, data: new Uint8Array(4) }]), /outside main flash/);
  assert.equal(f.st.erased.length, 0);
});
