#!/usr/bin/env python3
"""Minimal MQTT 3.1.1 client (standard library only), QoS 0.
  mqtt_min.py pub HOST PORT TOPIC PAYLOAD [--retain]
  mqtt_min.py sub HOST PORT TOPIC [SECONDS]      prints each message as 'topic payload'
Used to prove a broker from a machine with no MQTT client installed."""
import socket, struct, sys, time


def _s(b):
    return struct.pack('>H', len(b)) + b


def _len(n):
    out = b''
    while True:
        d = n % 128
        n //= 128
        out += bytes([d | (128 if n else 0)])
        if not n:
            return out


def connect(host, port):
    s = socket.create_connection((host, int(port)), timeout=10)
    body = _s(b'MQTT') + b'\x04\x02' + struct.pack('>H', 30) + _s(b'frl-%d' % int(time.time() * 1000 % 1e9))
    s.sendall(b'\x10' + _len(len(body)) + body)
    r = s.recv(4)
    if len(r) < 4 or r[0] != 0x20 or r[3] != 0:
        raise SystemExit(f'CONNACK refused: {r!r}')
    return s


def publish(host, port, topic, payload, retain=False):
    s = connect(host, port)
    body = _s(topic.encode()) + payload.encode()
    s.sendall(bytes([0x30 | (1 if retain else 0)]) + _len(len(body)) + body)
    s.sendall(b'\xe0\x00')
    s.close()


def subscribe(host, port, topic, seconds=10):
    s = connect(host, port)
    body = struct.pack('>H', 1) + _s(topic.encode()) + b'\x00'
    s.sendall(b'\x82' + _len(len(body)) + body)
    t0 = time.time()
    buf = b''
    s.settimeout(1)
    while time.time() - t0 < seconds:
        try:
            d = s.recv(4096)
        except socket.timeout:
            continue
        if not d:
            break
        buf += d
        while len(buf) >= 2:
            h = buf[0]
            n, mult, i = 0, 1, 1
            while True:
                if i >= len(buf):
                    break
                b = buf[i]; i += 1
                n += (b & 127) * mult; mult *= 128
                if not b & 128:
                    break
            if len(buf) < i + n:
                break
            pkt, buf = buf[i:i + n], buf[i + n:]
            if h >> 4 == 3:
                tl = struct.unpack('>H', pkt[:2])[0]
                print(pkt[2:2 + tl].decode(), pkt[2 + tl:].decode(), flush=True)


if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == 'pub' and len(a) >= 5:
        publish(a[1], a[2], a[3], a[4], '--retain' in a)
    elif a and a[0] == 'sub' and len(a) >= 4:
        subscribe(a[1], a[2], a[3], float(a[4]) if len(a) > 4 else 10)
    else:
        print(__doc__); sys.exit(2)
