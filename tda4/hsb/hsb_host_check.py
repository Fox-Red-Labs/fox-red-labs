#!/usr/bin/env python3
"""Fox Red Labs: a host-side check for a Holoscan Sensor Bridge device, written from NVIDIA's public
protocol documents and host source (holoscan-sensor-bridge, Apache-2.0). It does what the host library
does to bring up a data channel, then receives the RoCE v2 formatted UDP stream and checks it.

  1. Listen for the device's BOOTP enumeration broadcasts (UDP 12267); decode the NVDA vendor field.
  2. Configure the data plane over ECB (UDP 8192) with the same register writes as DataChannel.configure_roce.
  3. Receive on our UDP port: RDMA WRITE (0x2A) payload packets and the WRITE IMMEDIATE (0x2B) metadata
     packet that ends each frame; reassemble by RETH virtual address; check sequence numbers, byte counts,
     frame numbers and the frame CRC-32 in the metadata.

This is a verifier, not a Holoscan application: it proves the device side speaks the protocol. The real host
test is NVIDIA's LinuxReceiverOperator on a Holoscan machine.

usage: hsb_host_check.py --host-ip 192.0.2.1 --port 50000 --frame-bytes 2073600 [--frames 60] [--mtu 1500]
"""
import argparse, socket, struct, time, zlib, sys

BOOTP_REQ_PORT, ECB_PORT, DATA_SRC_PORT = 12267, 8192, 12288
WR_DWORD, RD_DWORD = 0x04, 0x14
FLAG_ACK = 0x01
HIF, VP = 0x02000300, 0x1000              # data_plane 0, sensor 0 (sif index 0)
DP_QP, DP_RKEY, DP_PAGE_LSB, DP_PAGE_MSB, DP_PAGE_INC, DP_MAX_BUFF, DP_BUFFER_LENGTH = 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18
DP_HOST_MAC_LOW, DP_HOST_MAC_HIGH, DP_HOST_IP, DP_HOST_UDP_PORT = 0x20, 0x24, 0x28, 0x2C
DP_PACKET_SIZE, DP_PACKET_UDP_PORT, DP_VP_MASK = 0x04, 0x08, 0x0C
PAGE = 128
OP_WRITE, OP_WRITE_IMM = 0x2A, 0x2B

def enumerate_device(timeout=10.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", BOOTP_REQ_PORT)); s.settimeout(timeout)
    pkt, (ip, port) = s.recvfrom(2048); s.close()
    op, htype, hlen, hops, xid, secs, flags, ciaddr, yiaddr, siaddr, giaddr = struct.unpack("!BBBBIHHIIII", pkt[:28])
    chaddr = pkt[28:44]; vend = pkt[236:300]
    mac = ":".join(f"{b:02x}" for b in chaddr[:6])
    tag, tlen = vend[0], vend[1]; vendor = vend[2:6].decode(errors="replace")
    eth_port, enum_ver = vend[6], vend[7]
    uuid = vend[10:26].hex(); serial = vend[30:37][::-1].hex(); ipver = struct.unpack("<H", vend[37:39])[0]
    dev_ip = socket.inet_ntoa(struct.pack("!I", ciaddr))
    return dict(from_ip=ip, from_port=port, op=op, mac=mac, device_ip=dev_ip, tag=hex(tag), tag_len=tlen, vendor=vendor,
                eth_port=eth_port, enum_version=enum_ver, uuid=uuid, serial=serial, hsb_ip_version=hex(ipver), xid=xid, secs=secs)

class Ecb:
    def __init__(self, dev_ip):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); self.s.settimeout(2.0)
        self.addr = (dev_ip, ECB_PORT); self.seq = 1; self.n = 0
    def _xfer(self, cmd, address, value=0):
        req = struct.pack("!BBHBBII", cmd, FLAG_ACK, self.seq & 0xFFFF, 0, 0, address, value)
        self.s.sendto(req, self.addr); rep, _ = self.s.recvfrom(1500); self.n += 1
        rcmd, rflags, rseq, status, _r, raddr, rdata = struct.unpack("!BBHBBII", rep[:14])
        if rcmd != (cmd | 0x80) or status != 0 or raddr != address:
            raise RuntimeError(f"ECB cmd {cmd:#x} addr {address:#x}: reply cmd {rcmd:#x} status {status:#x} addr {raddr:#x}")
        self.seq += 1; return rdata
    def write(self, address, value): self._xfer(WR_DWORD, address, value & 0xFFFFFFFF)
    def read(self, address): return self._xfer(RD_DWORD, address)

def mac_to_regs(mac):
    b = bytes.fromhex(mac.replace(":", ""))
    return struct.unpack("!I", b[2:6])[0], struct.unpack("!H", b[0:2])[0]   # low 4 bytes, high 2 bytes (host library order)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host-ip", required=True); ap.add_argument("--port", type=int, default=50000)
    ap.add_argument("--frame-bytes", type=int, required=True); ap.add_argument("--frames", type=int, default=60)
    ap.add_argument("--mtu", type=int, default=1500); ap.add_argument("--host-mac", default="02:00:00:xx:xx:xx")
    ap.add_argument("--source", default=None, help="raw frames file the device serves; received frames are checked against it")
    a = ap.parse_args()

    known = set()
    if a.source:
        raw = open(a.source, "rb").read()
        known = {zlib.crc32(raw[i:i + a.frame_bytes]) & 0xFFFFFFFF for i in range(0, len(raw) - a.frame_bytes + 1, a.frame_bytes)}
        print(f"source frames: {len(known)} distinct CRCs")
    t0 = time.time(); e = enumerate_device(); t_enum = time.time() - t0
    print(f"enumerated in {t_enum:.2f} s: {e}"); sys.stdout.flush()
    dev = e["device_ip"] if e["device_ip"] != "0.0.0.0" else e["from_ip"]

    # host receive socket first, so nothing is lost
    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 64 << 20)
    rx.bind((a.host_ip, a.port)); rx.settimeout(5.0)

    ecb = Ecb(dev)
    # same arithmetic as DataChannel::compute_payload_size / configure_common / configure_roce
    overhead = 74
    payload = ((a.mtu - overhead) // PAGE) * PAGE
    frame_aligned = (a.frame_bytes + PAGE - 1) & ~(PAGE - 1)
    page_size = frame_aligned + PAGE        # frame plus one page for the metadata slot
    pages = 2                                # two frame buffers
    frame_memory = 0x0000_1000_0000         # a fake virtual address: the Linux receiver ignores it
    qp, rkey = 0x1234, 0xABCD
    mac_low, mac_high = mac_to_regs(a.host_mac)
    host_ip_u32 = struct.unpack("!I", socket.inet_aton(a.host_ip))[0]
    vp_mask = 1
    version = ecb.read(0x80)                 # HSB_IP_VERSION register
    ecb.write(HIF + DP_VP_MASK, ecb.read(HIF + DP_VP_MASK) & ~vp_mask)
    ecb.write(HIF + DP_PACKET_SIZE, payload // PAGE)
    ecb.write(HIF + DP_PACKET_UDP_PORT, DATA_SRC_PORT)
    ecb.write(VP + DP_BUFFER_LENGTH, a.frame_bytes)
    ecb.write(VP + DP_HOST_MAC_LOW, mac_low); ecb.write(VP + DP_HOST_MAC_HIGH, mac_high)
    ecb.write(VP + DP_HOST_IP, host_ip_u32); ecb.write(VP + DP_HOST_UDP_PORT, a.port)
    ecb.write(VP + DP_QP, qp); ecb.write(VP + DP_RKEY, rkey)
    ecb.write(VP + DP_PAGE_LSB, frame_memory & 0xFFFFFFFF); ecb.write(VP + DP_PAGE_MSB, frame_memory >> 32)
    ecb.write(VP + DP_PAGE_INC, page_size // PAGE); ecb.write(VP + DP_MAX_BUFF, pages - 1)
    ecb.write(HIF + DP_VP_MASK, ecb.read(HIF + DP_VP_MASK) | vp_mask)
    print(f"configured over ECB ({ecb.n} transactions): version reg {version:#x}, payload {payload} B/packet, frame {a.frame_bytes} B, "
          f"page {page_size} B, {pages} pages, host {a.host_ip}:{a.port}"); sys.stdout.flush()

    frames = {}; done = 0; pkts = 0; bad_seq = 0; last_psn = None; t_first = None; bytes_rx = 0
    results = []
    while done < a.frames:
        try: pkt, src = rx.recvfrom(65535)
        except socket.timeout: print("timeout waiting for data"); break
        if t_first is None: t_first = time.time()
        pkts += 1; bytes_rx += len(pkt)
        if src[1] != DATA_SRC_PORT: print(f"unexpected source port {src[1]}")
        opcode = pkt[0]; psn = struct.unpack("!I", b"\0" + pkt[9:12])[0]; dqp = struct.unpack("!I", b"\0" + pkt[5:8])[0]
        va, r_key, dma_len = struct.unpack("!QII", pkt[12:28])
        if last_psn is not None and psn != ((last_psn + 1) & 0xFFFFFF): bad_seq += 1
        last_psn = psn
        off = va - frame_memory; page = off // page_size; inoff = off % page_size
        if opcode == OP_WRITE:
            data = pkt[28:-4]                                   # payload then iCRC
            frames.setdefault(page, bytearray(page_size))[inoff:inoff + len(data)] = data
            pk_per_frame = pk_per_frame + 1 if "pk_per_frame" in dir() else 1
        elif opcode == OP_WRITE_IMM:
            imm = struct.unpack("!I", pkt[28:32])[0]
            data = pkt[32:-4]
            buf = frames.setdefault(page, bytearray(page_size)); buf[inoff:inoff + len(data)] = data
            md = buf[frame_aligned:frame_aligned + 128]
            flags, mpsn, crc, ts_hi, ts_lo, ts_ns, bw_hi, bw_lo, fnum, md_hi, md_lo, md_ns = struct.unpack(">12I", md[:48])   # the emulator writes these with htonl (network order)
            frame = bytes(buf[:a.frame_bytes]); our_crc = zlib.crc32(frame) & 0xFFFFFFFF
            bytes_written = (bw_hi << 32) | bw_lo
            intact = (our_crc in known) if known else True
            ok = (bytes_written == a.frame_bytes) and intact and (our_crc == crc or crc == 0)
            results.append((fnum, ok, bytes_written, crc, our_crc, flags, dqp & 0xFFFFFF, r_key, imm))
            if done < 3 or not ok:
                print(f"frame {fnum}: bytes {bytes_written} crc {crc:#010x} ours {our_crc:#010x} flags {flags:#x} qp {dqp & 0xFFFFFF:#x} rkey {r_key:#x} imm {imm:#010x} {'OK' if ok else 'MISMATCH'} {'intact' if intact else 'CORRUPT'}")
            done += 1; frames.pop(page, None)
        else:
            print(f"unknown opcode {opcode:#x}")
    t = time.time() - (t_first or time.time())
    good = sum(1 for r in results if r[1])
    print(f"RESULT frames {done} good {good} packets {pkts} ({pkts / done if done else 0:.1f} per frame) bad_psn {bad_seq} bytes {bytes_rx} in {t:.2f} s = {done / t if t else 0:.1f} fps, {bytes_rx * 8 / t / 1e6 if t else 0:.1f} Mbit/s; "
          f"ECB transactions {ecb.n}; enumeration {t_enum:.2f} s")
    sys.stdout.flush()

if __name__ == "__main__":
    main()
