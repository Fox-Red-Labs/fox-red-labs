#!/usr/bin/env python3
"""Mac-side client for TI's Bluetooth OAD (over-the-air download) profile, F3 SDK 9.21 (CC23xx).

Sends an MCUboot image to a TI OAD target (basic_ble_oad_dual_image, basic_ble_oad_offchip or
basic_persistent) from a Mac terminal, with the Python library bleak. No phone app, no Windows tool.

  oad_client.py --dry-run [--keys-c keys.c] image.bin
        Parse and check the image without Bluetooth: MCUboot header, TLV area, SHA-256,
        key hash and ECDSA P-256 signature (with --keys-c), trailer, and the transfer plan.
  oad_client.py --name "FRL OAD 1" --expect-name "FRL OAD 2" image.bin
        Scan for the advertised name, send the image, enable it, then wait for the SAME board
        (same address) to come back advertising the new name and read its software version.
        If more than one device advertises the name the client refuses and lists them with
        their addresses and signal strength; pick one with --address.
  oad_client.py --address 6C:C3:74:xx:xx:xx image.bin
        Target one board by BLE address (a macOS UUID on a Mac). --name is then optional.
        Exit status is nonzero if the target answers Enable Image with anything but success.

Protocol, as implemented by the SDK source (source/ti/ble/profiles/oad/src/oad_profile.c and
source/ti/ble/services/oad/src/oad_service.c). Where the user guide and the source differ, this
follows the source; the differences are listed in cc2340r5/oad/README.md.
  Service 0xFFC0, characteristics 0xFFC1 image identify, 0xFFC2 image block, 0xFFC5 control,
  all on TI's 128-bit base F000xxxx-0451-4000-B000-000000000000.
  1. Notifications on 0xFFC1 (identify status) and 0xFFC5 (block requests and command replies).
  2. 0x01 Get Block Size on 0xFFC5 -> 01 <u16 block size>. The source always answers 240.
  3. Image identify: the 32-byte MCUboot header, with ih_img_size replaced by the number of
     bytes to send (TI's SimpleLink Connect app does the same), written to 0xFFC1 -> 1-byte status.
  4. 0x03 Start OAD on 0xFFC5 -> 12 <status> <u32 block number> requests.
  5. Each block: <u32 block number> + (block size - 4) bytes of the file, written to 0xFFC2.
  6. 12 0E <n> means download complete; send 0x04 Enable Image -> 04 00, the target drops the
     link and resets, and MCUboot copies the new image into place.
"""
import argparse, asyncio, hashlib, json, math, re, struct, sys, time

IMAGE_MAGIC = 0x96F3B83D
TLV_INFO_MAGIC, TLV_PROT_INFO_MAGIC = 0x6907, 0x6908
TLV_NAMES = {0x01: "KEYHASH", 0x02: "PUBKEY", 0x10: "SHA256", 0x11: "SHA384", 0x20: "RSA2048_PSS",
             0x22: "ECDSA_SIG", 0x24: "ED25519", 0x50: "SEC_CNT", 0x60: "DEPENDENCY"}
BOOT_MAGIC = bytes.fromhex("77c295f360d2ef7f3552500f2cb67980")   # MCUboot trailer magic
DEFAULT_SLOT = 0x3B000          # dual-image slot size on the CC2340R5 (SysConfig postbuild --slot-size)
DEFAULT_BLOCK = 240             # OAD_DEFAULT_BLOCK_SIZE in oad_profile.c, 4 bytes of it are the block number

BASE = "f000{:04x}-0451-4000-b000-000000000000"
SVC, IDENTIFY, BLOCK, CONTROL = (BASE.format(u) for u in (0xFFC0, 0xFFC1, 0xFFC2, 0xFFC5))
CMD_GET_BLK_SZ, CMD_START, CMD_ENABLE, CMD_CANCEL, CMD_GET_SW_VER, CMD_GET_STATUS = 0x01, 0x03, 0x04, 0x05, 0x07, 0x08
RSP_BLOCK_REQ = 0x12
STATUS = ["SUCCESS", "VALIDATION_ERR", "FLASH_ERR", "BUFFER_OFL", "ALREADY_STARTED", "NOT_STARTED",
          "DL_NOT_COMPLETE", "NO_RESOURCES", "IMAGE_TOO_BIG", "INCOMPATIBLE_IMAGE", "INVALID_FILE",
          "INCOMPATIBLE_FILE", "AUTH_FAIL", "EXT_NOT_SUPPORTED", "DL_COMPLETE", "CCCD_NOT_ENABLED",
          "IMG_ID_TIMEOUT", "APP_STOP_PROCESS", "ERROR"]
DL_COMPLETE = 14


def sw_version(rsp):
    """Get Software Version reply: 07 major minor rev_hi rev_lo build_hi build_lo (oadSendswVersionRsp)."""
    v = rsp[1:]
    return f"{v[0]}.{v[1]}.{(v[2] << 8) | v[3]}" if len(v) >= 4 else v.hex()


def status_name(s):
    return STATUS[s] if s < len(STATUS) else ("CMD_NOT_SUPPORTED" if s == 0xFF else f"0x{s:02X}")


# ---------- ECDSA P-256 verify, standard library only ----------
P = 2**256 - 2**224 + 2**192 + 2**96 - 1
N = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
G = (0x6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296,
     0x4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5)


def _add(p1, p2):
    if p1 is None:
        return p2
    if p2 is None:
        return p1
    (x1, y1), (x2, y2) = p1, p2
    if x1 == x2 and (y1 + y2) % P == 0:
        return None
    if p1 == p2:
        lam = (3 * x1 * x1 - 3) * pow(2 * y1, -1, P) % P
    else:
        lam = (y2 - y1) * pow(x2 - x1, -1, P) % P
    x3 = (lam * lam - x1 - x2) % P
    return x3, (lam * (x1 - x3) - y1) % P


def _mul(k, pt):
    acc = None
    while k:
        if k & 1:
            acc = _add(acc, pt)
        pt = _add(pt, pt)
        k >>= 1
    return acc


def _der_ints(sig):
    """DER SEQUENCE { INTEGER r, INTEGER s } -> (r, s)."""
    if sig[0] != 0x30:
        raise ValueError("signature is not a DER sequence")
    i, out = 2, []
    for _ in range(2):
        if sig[i] != 0x02:
            raise ValueError("signature integer missing")
        ln = sig[i + 1]
        out.append(int.from_bytes(sig[i + 2:i + 2 + ln], "big"))
        i += 2 + ln
    return out


def ecdsa_p256_verify(point65, digest, sig_der):
    if point65[0] != 4:
        raise ValueError("public key is not an uncompressed point")
    q = (int.from_bytes(point65[1:33], "big"), int.from_bytes(point65[33:65], "big"))
    r, s = _der_ints(sig_der)
    if not (0 < r < N and 0 < s < N):
        return False
    e, w = int.from_bytes(digest, "big"), pow(s, -1, N)
    pt = _add(_mul(e * w % N, G), _mul(r * w % N, q))
    return pt is not None and pt[0] % N == r


def keys_c_pubkey(path):
    src = open(path).read()
    m = re.search(r"MCUBOOT_SIGN_EC256.*?root_pub_der\[\]\s*=\s*\{(.*?)\}", src, re.S)
    if not m:
        raise SystemExit(f"{path}: no EC256 root_pub_der found")
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(1)))


# ---------- image parsing ----------
def parse_image(data, slot_size=DEFAULT_SLOT, keys_c=None):
    """Return a dict describing the image and a list of problems (empty if it passes)."""
    problems, info = [], {"file_bytes": len(data)}
    if len(data) < 32:
        return info, ["file shorter than an MCUboot header"]
    magic, load, hdr, prot, isz, flags, maj, mnr, rev, build = struct.unpack_from("<IIHHIIBBHI", data, 0)
    info.update(magic=f"0x{magic:08X}", load_addr=f"0x{load:08X}", hdr_size=hdr, protect_tlv_size=prot,
                img_size=isz, flags=f"0x{flags:08X}", version=f"{maj}.{mnr}.{rev}+{build}")
    if magic != IMAGE_MAGIC:
        problems.append(f"header magic 0x{magic:08X}, expected 0x{IMAGE_MAGIC:08X}")
        return info, problems
    if flags:
        problems.append(f"header flags 0x{flags:08X} set (encrypted or compressed images are not handled here)")
    if len(set(data[32:hdr])) > 1:
        problems.append("header padding (bytes 32 to hdr_size) is not one erased value (imgtool writes 0xFF)")
    end_body = hdr + isz
    t = end_body
    # optional protected TLV area, then the TLV info area
    tlvs, tlv_total = [], 0
    for want in ([TLV_PROT_INFO_MAGIC] if prot else []) + [TLV_INFO_MAGIC]:
        if t + 4 > len(data):
            problems.append("file ends before the TLV area")
            return info, problems
        m, tot = struct.unpack_from("<HH", data, t)
        if m != want:
            problems.append(f"TLV magic 0x{m:04X} at 0x{t:X}, expected 0x{want:04X}")
            return info, problems
        pos, end = t + 4, t + tot
        while pos < end:
            typ, ln = struct.unpack_from("<HH", data, pos)
            tlvs.append((typ, data[pos + 4:pos + 4 + ln], m == TLV_PROT_INFO_MAGIC))
            pos += 4 + ln
        tlv_total += tot
        t = end
    info["tlv_bytes"] = tlv_total
    info["tlvs"] = [f"{TLV_NAMES.get(ty, hex(ty))} ({len(v)} B{', protected' if pr else ''})" for ty, v, pr in tlvs]
    signed_len = end_body + prot
    digest = hashlib.sha256(data[:signed_len]).digest()
    tv = {ty: v for ty, v, _ in tlvs}
    if 0x10 not in tv:
        problems.append("no SHA256 TLV")
    else:
        info["sha256_tlv"] = tv[0x10].hex()
        info["sha256_ok"] = tv[0x10] == digest
        if not info["sha256_ok"]:
            problems.append("SHA-256 of header and body does not match the SHA256 TLV")
    if keys_c:
        der = keys_c_pubkey(keys_c)
        if 0x01 in tv:
            info["keyhash_ok"] = tv[0x01] == hashlib.sha256(der).digest()
            if not info["keyhash_ok"]:
                problems.append("KEYHASH TLV does not match the key in keys.c: MCUboot would refuse it")
        if 0x22 in tv:
            info["signature_ok"] = ecdsa_p256_verify(der[-65:], digest, tv[0x22])
            if not info["signature_ok"]:
                problems.append("ECDSA P-256 signature does not verify against keys.c")
        else:
            problems.append("no ECDSA_SIG TLV")
    image_end = end_body + tlv_total
    rest = data[image_end:]
    info["trailer_magic"] = data[-16:] == BOOT_MAGIC
    padding = rest[:-16] if info["trailer_magic"] else rest
    info["padding_bytes"] = len(padding)
    if padding and set(padding) != {0xFF}:
        problems.append("bytes after the TLV area are not all 0xFF")
    # What to send: TI's SimpleLink Connect app sends the whole file when it ends with the MCUboot
    # trailer magic (the magic tells MCUboot an upgrade is waiting), otherwise header + body + TLVs.
    send = len(data) if info["trailer_magic"] else image_end
    info["send_bytes"] = send
    if send > slot_size:
        problems.append(f"{send} bytes to send is more than the {slot_size}-byte slot")
    if info["trailer_magic"] and len(data) != slot_size:
        problems.append(f"padded file is {len(data)} bytes, slot is {slot_size}: the trailer would land in the wrong place")
    return info, problems


def identify_payload(data, send_bytes):
    hdr = bytearray(data[:32])
    struct.pack_into("<I", hdr, 12, send_bytes)
    return bytes(hdr)


def plan(send_bytes, block=DEFAULT_BLOCK):
    per = block - 4
    blocks = math.ceil(send_bytes / per)
    last = send_bytes - per * (blocks - 1)
    return {"block_size": block, "data_per_block": per, "blocks": blocks, "last_block_data": last,
            "bytes_over_air": send_bytes + 4 * blocks}


def dry_run(args):
    data = open(args.image, "rb").read()
    info, problems = parse_image(data, args.slot_size, args.keys_c)
    pl = plan(info.get("send_bytes", len(data)))
    print(f"image: {args.image}")
    for k in ("file_bytes", "magic", "load_addr", "hdr_size", "protect_tlv_size", "img_size", "flags", "version",
              "tlv_bytes", "tlvs", "sha256_ok", "keyhash_ok", "signature_ok", "trailer_magic", "padding_bytes", "send_bytes"):
        if k in info:
            print(f"  {k:17s} {info[k]}")
    if "send_bytes" in info:
        print(f"  identify payload  {identify_payload(data, info['send_bytes']).hex()}")
    print(f"  transfer plan     {pl['blocks']} blocks of {pl['data_per_block']} data bytes "
          f"(block size {pl['block_size']} with the 4-byte block number), last block {pl['last_block_data']} bytes, "
          f"{pl['bytes_over_air']} bytes written in total")
    for ms in (15, 30, 50):
        for per_block in (2, 3):
            print(f"  estimate          {ms} ms connection interval, {per_block} intervals a block: "
                  f"{pl['blocks'] * per_block * ms / 1000:.0f} s")
    if "trailer_magic" in info and not info["trailer_magic"]:
        print("  note              no MCUboot trailer magic at the end of the file: an overwrite or swap MCUboot may not see a\n"
              "                    pending upgrade. TI's dual-image post-build pads to the slot size and adds it (imgtool --pad).")
    if problems:
        print("PROBLEMS:")
        for p in problems:
            print("  - " + p)
        return 1
    print("dry run OK: the image parses, its hash and signature check out, and it fits the slot")
    return 0


# ---------- live transfer ----------
def same_address(a, b):
    return a is not None and b is not None and a.replace("-", ":").upper() == b.replace("-", ":").upper()


def select_device(matches, what):
    """matches: list of (device, rssi). Return the one device, or raise SystemExit that lists the candidates."""
    if not matches:
        return None
    if len(matches) > 1:
        rows = "\n".join(f"    {d.address}  RSSI {r if r is not None else '?'} dBm" for d, r in
                          sorted(matches, key=lambda m: -(m[1] if m[1] is not None else -999)))
        raise SystemExit(f"{len(matches)} devices match {what}; refusing to pick one. "
                         f"Re-run with --address set to one of these:\n{rows}")
    return matches[0][0]


async def find(name, timeout, address=None, settle=3.0):
    """Scan for a device whose advertised name is `name` and/or whose address is `address`.

    With an address, return as soon as that device is seen. With only a name, keep scanning `settle`
    seconds after the first hit so a second board with the same name is noticed, then require exactly one.
    """
    from bleak import BleakScanner
    t0 = time.monotonic()
    found, first = {}, [None]

    def on_adv(d, adv):
        if address is not None and not same_address(d.address, address):
            return
        if name is not None and adv.local_name != name:
            return
        found[d.address] = (d, adv.rssi)
        if first[0] is None:
            first[0] = time.monotonic()

    async with BleakScanner(detection_callback=on_adv):
        while time.monotonic() - t0 < timeout:
            if found and (address is not None or time.monotonic() - first[0] >= settle):
                break
            await asyncio.sleep(0.1)
    what = " and ".join(x for x in (f"the name {name!r}" if name else "", f"address {address}" if address else "") if x)
    dev = select_device(list(found.values()), what)
    return dev, time.monotonic() - t0


async def run(args):
    from bleak import BleakClient
    data = open(args.image, "rb").read()
    info, problems = parse_image(data, args.slot_size, args.keys_c)
    if problems:
        raise SystemExit("image check failed: " + "; ".join(problems) + " (run --dry-run for details)")
    send = info["send_bytes"]
    result = {"image": args.image, "version": info["version"], "send_bytes": send, "name": args.name, "address": args.address}
    log = lambda *a: print(f"[{time.strftime('%H:%M:%S')}]", *a, flush=True)

    dev, t_scan = await find(args.name, args.scan_timeout, args.address)
    if not dev:
        raise SystemExit(f"no device matching {args.name or args.address!r} within {args.scan_timeout} s")
    target_address = dev.address
    result["address"] = target_address
    log(f"found {args.name or ''!r} ({target_address}) in {t_scan:.2f} s")
    result["scan_s"] = round(t_scan, 2)

    q_ctrl, q_id = asyncio.Queue(), asyncio.Queue()
    disconnected = asyncio.Event()
    t0 = time.monotonic()
    async with BleakClient(dev, disconnected_callback=lambda c: disconnected.set(), timeout=20) as client:
        result["connect_s"] = round(time.monotonic() - t0, 2)
        mtu = client.mtu_size
        result["att_mtu"] = mtu
        log(f"connected in {result['connect_s']} s, ATT MTU {mtu}")
        svc = client.services.get_service(SVC)
        if not svc:
            raise SystemExit("OAD service 0xFFC0 not found: is this an OAD image (dual-image, off-chip or persistent)?")
        await client.start_notify(IDENTIFY, lambda c, d: q_id.put_nowait(bytes(d)))
        await client.start_notify(CONTROL, lambda c, d: q_ctrl.put_nowait(bytes(d)))

        async def control(cmd, expect=None, timeout=5.0):
            await client.write_gatt_char(CONTROL, bytes([cmd]), response=False)   # 0xFFC5 is write-no-response only
            while True:
                rsp = await asyncio.wait_for(q_ctrl.get(), timeout)
                if expect is None or rsp[0] == expect:
                    return rsp

        rsp = await control(CMD_GET_SW_VER, CMD_GET_SW_VER)
        result["running_version"] = sw_version(rsp)
        log(f"running image version {result['running_version']} (raw {rsp[1:].hex()})")
        rsp = await control(CMD_GET_BLK_SZ, CMD_GET_BLK_SZ)
        block = struct.unpack_from("<H", rsp, 1)[0]
        result["block_size"] = block
        log(f"block size {block} ({block - 4} data bytes a block)")
        if mtu - 3 < block:
            raise SystemExit(f"ATT MTU {mtu} carries at most {mtu - 3} bytes a write, the target wants {block}. "
                             "The target ignores the offset of a long write, so this would corrupt the image. Stopping.")
        pl = plan(send, block)
        result.update(blocks=pl["blocks"])

        await client.write_gatt_char(IDENTIFY, identify_payload(data, send), response=True)
        st = (await asyncio.wait_for(q_id.get(), 5.0))[0]
        log(f"image identify: {status_name(st)}")
        if st != 0:
            raise SystemExit(f"target refused the image header: {status_name(st)}")

        await client.write_gatt_char(CONTROL, bytes([CMD_START]), response=False)
        per, t_dl, last_pct, sent = block - 4, time.monotonic(), -1, 0
        while True:
            try:
                rsp = await asyncio.wait_for(q_ctrl.get(), args.block_timeout)
            except asyncio.TimeoutError:
                raise SystemExit(f"no block request for {args.block_timeout} s after {sent} blocks: stopped")
            if rsp[0] != RSP_BLOCK_REQ:
                log(f"control notification {rsp.hex()} (ignored)")
                continue
            st, blk = rsp[1], struct.unpack_from("<I", rsp, 2)[0]
            if st == DL_COMPLETE:
                break
            if st != 0:
                raise SystemExit(f"target reported {status_name(st)} at block {blk}")
            chunk = data[blk * per:min((blk + 1) * per, send)]
            await client.write_gatt_char(BLOCK, struct.pack("<I", blk) + chunk, response=not args.no_response)
            sent += 1
            pct = int(100 * (blk + 1) / pl["blocks"])
            if pct != last_pct and pct % 5 == 0:
                el = time.monotonic() - t_dl
                log(f"{pct:3d}%  block {blk + 1}/{pl['blocks']}  {el:6.1f} s  {(blk + 1) * per * 8 / el / 1000:6.1f} kbit/s")
                last_pct = pct
        t_xfer = time.monotonic() - t_dl
        result.update(transfer_s=round(t_xfer, 2), kbit_s=round(send * 8 / t_xfer / 1000, 1))
        log(f"download complete: {send} bytes in {t_xfer:.1f} s ({result['kbit_s']} kbit/s)")

        rsp = await control(CMD_ENABLE, CMD_ENABLE)
        log(f"enable image: {status_name(rsp[1])}")
        result["enable_status"] = status_name(rsp[1])
        if rsp[1] != 0:
            result["verified"] = False
            if args.json:
                json.dump(result, open(args.json, "w"), indent=1)
            print(json.dumps(result, indent=1))
            raise SystemExit(f"target answered Enable Image with {status_name(rsp[1])}: the update was NOT applied")
        t_enable = time.monotonic()
        try:
            await asyncio.wait_for(disconnected.wait(), 10)
            log("target dropped the link to reset")
        except asyncio.TimeoutError:
            log("target did not drop the link within 10 s")

    if args.expect_name:
        # Same board only: the address must match the one we updated, and it must advertise the new name.
        dev2, _ = await find(args.expect_name, args.reboot_timeout, target_address)
        if not dev2:
            result["verified"] = False
            print(json.dumps(result, indent=1))
            raise SystemExit(f"{args.expect_name!r} not seen at address {target_address} within {args.reboot_timeout} s of the enable command")
        result["enable_to_new_name_s"] = round(time.monotonic() - t_enable, 2)
        log(f"{args.expect_name!r} advertising {result['enable_to_new_name_s']} s after the enable command")
        async with BleakClient(dev2, timeout=20) as c2:
            q = asyncio.Queue()
            await c2.start_notify(CONTROL, lambda c, d: q.put_nowait(bytes(d)))
            await c2.write_gatt_char(CONTROL, bytes([CMD_GET_SW_VER]), response=False)
            rsp = await asyncio.wait_for(q.get(), 5)
            result["new_version"] = sw_version(rsp)
            log(f"new image version {result['new_version']} (raw {rsp[1:].hex()})")
        result["verified"] = True
    if args.json:
        json.dump(result, open(args.json, "w"), indent=1)
    print(json.dumps(result, indent=1))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", help="signed MCUboot image, for example basic_ble_oad_dual_image_v2.bin")
    ap.add_argument("--dry-run", action="store_true", help="check the file and print the plan; no Bluetooth")
    ap.add_argument("--keys-c", help="MCUboot keys.c, to check the key hash and the ECDSA signature")
    ap.add_argument("--slot-size", type=lambda s: int(s, 0), default=DEFAULT_SLOT, help="target slot size (default 0x3B000)")
    ap.add_argument("--name", help="advertised name of the target running the old image")
    ap.add_argument("--address", help="BLE address of the target (a macOS UUID on a Mac); use it when several boards share a name")
    ap.add_argument("--expect-name", help="advertised name the new image uses; checked after the reset, on the same address")
    ap.add_argument("--no-response", action="store_true", help="write blocks without response (TI's app writes with response)")
    ap.add_argument("--scan-timeout", type=float, default=20)
    ap.add_argument("--block-timeout", type=float, default=9, help="seconds to wait for a block request (target gives up at 10)")
    ap.add_argument("--reboot-timeout", type=float, default=60)
    ap.add_argument("--json", help="write the result to this file")
    args = ap.parse_args()
    if args.dry_run:
        sys.exit(dry_run(args))
    if not args.name and not args.address:
        ap.error("--name or --address is required unless --dry-run")
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
