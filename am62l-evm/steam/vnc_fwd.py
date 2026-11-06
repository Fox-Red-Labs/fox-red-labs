#!/usr/bin/env python3
"""On the board (host): forward 127.0.0.1:PORT to x11vnc's unix socket inside the Steam container.
Nothing listens on the LAN: x11vnc has no TCP port (-rfbport 0, -unixsock) and this forwarder binds loopback only.
Host root never opens a path in the container's writable mount: each connection runs
`docker exec -i <container> socat STDIO UNIX-CONNECT:<socket>` and pipes bytes to it.
Reach it from a Mac with an ssh tunnel:  ssh -N -L 5901:127.0.0.1:5901 root@<board>  then  open vnc://localhost:5901
Usage: python3 vnc_fwd.py [PORT=5901] [CONTAINER=frl-steam-user] [SOCKET=/data/vnc/vnc.sock]"""
import socket, subprocess, sys, threading
port = int(sys.argv[1]) if len(sys.argv) > 1 else 5901
ctr = sys.argv[2] if len(sys.argv) > 2 else 'frl-steam-user'
sock = sys.argv[3] if len(sys.argv) > 3 else '/data/vnc/vnc.sock'
def serve(c):
    p = subprocess.Popen(['docker', 'exec', '-i', '-u', '1000', ctr, 'socat', 'STDIO', 'UNIX-CONNECT:' + sock],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
    def up():
        try:
            while (d := c.recv(65536)): p.stdin.write(d)
        except OSError: pass
        finally:
            try: p.stdin.close()
            except OSError: pass
    threading.Thread(target=up, daemon=True).start()
    try:
        while (d := p.stdout.read1(65536) if hasattr(p.stdout, 'read1') else p.stdout.read(65536)): c.sendall(d)
    except OSError: pass
    finally:
        c.close(); p.kill()
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port)); srv.listen(2)
print(f"vnc_fwd: 127.0.0.1:{port} -> {ctr}:{sock}", flush=True)
while True:
    c, _ = srv.accept()
    threading.Thread(target=serve, args=(c,), daemon=True).start()
