import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1); s.setsockopt(socket.SOL_SOCKET,socket.SO_BROADCAST,1); s.bind(('',12267)); s.settimeout(4)
try:
    p,a=s.recvfrom(2048); print('BOOTP from',a,len(p),'bytes; vendor',p[236:246])
except Exception as e: print('no bootp:',e)
