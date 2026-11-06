#!/usr/bin/env python3
"""Check the bytes ecdh_p256 left in RAM against an independent pure-Python P-256 (no third-party library).
   ecdh_check.py <xr hex as printed by run_image.py, first 424 bytes>  -> prints which byte order matches and PASS/FAIL"""
import sys, struct
p=0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
n=0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
a=p-3; b=0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
G=(0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296,0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5)
def add(P,Q):
    if P is None: return Q
    if Q is None: return P
    if P[0]==Q[0] and (P[1]+Q[1])%p==0: return None
    l=(3*P[0]*P[0]+a)*pow(2*P[1],-1,p)%p if P==Q else (Q[1]-P[1])*pow(Q[0]-P[0],-1,p)%p
    x=(l*l-P[0]-Q[0])%p; return (x,(l*(P[0]-x)-P[1])%p)
def mul(k,P):
    R=None
    while k:
        if k&1: R=add(R,P)
        P=add(P,P); k>>=1
    return R
def main():
    h=sys.argv[1]; h=h.split(':')[-1].strip(); raw=bytes.fromhex(h)[:424]
    magic,cpu,passf,stkg,sta,stb,match,c0,c1,c2=struct.unpack('<10I',raw[:40]); f=raw[40:]
    names=['skA','pkAx','pkAy','skB','pkBx','pkBy','aX','aY','bX','bY','seedA','seedB']; d={k:f[i*32:i*32+32] for i,k in enumerate(names)}
    print('pass',passf,'keygen_ok',stkg,'statA',hex(sta),'statB',hex(stb),'match',match,'cycles',c0,c1,c2)
    for order in ('big','little'):
        sk=int.from_bytes(d['skA'],order); P=mul(sk,G)
        px=int.from_bytes(d['pkAx'],order); py=int.from_bytes(d['pkAy'],order)
        print(order,'skA*G == pkA:',P==(px,py) if P else None)
        skb=int.from_bytes(d['skB'],order); Q=mul(skb,G)
        if Q:
            S=mul(sk,Q); 
            if S: print(order,'ECDH x (skA*pkB):',hex(S[0]),' aX==',int.from_bytes(d['aX'],order)==S[0])
main()
