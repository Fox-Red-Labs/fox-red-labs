import struct,sys
b=bytes.fromhex(open(sys.argv[1]).read().strip())[:1796]
w=struct.unpack('<7I',b[:28]); print("done",hex(w[1]),"bus",w[6]/1000)
pk,stop=struct.unpack('<2I',b[1592:1600]); print("cal peak mA",pk,"cal stop",stop)
tb=struct.unpack('<6I',b[1600:1624]); ok=struct.unpack('<I',b[1624:1628])[0]
print("table ok",ok,"boundaries (deg, forward order 3,2,6,4,5,1):",[round(x/2**32*360,1) for x in tb])
iq,vm=struct.unpack('<2f',b[1628:1636]); st,isr,lmax,lmean=struct.unpack('<4I',b[1636:1652])
print("Iq ref",iq,"Vmax",vm,"stop",st,"isr",isr,"loop cycles mean/max",lmean,lmax,"of 8000")
hz=struct.unpack('<6f',b[1652:1676]); idd=struct.unpack('<6f',b[1676:1700]); iqq=struct.unpack('<6f',b[1700:1724]); vd=struct.unpack('<6f',b[1724:1748]); vq=struct.unpack('<6f',b[1748:1772]); pp=struct.unpack('<6I',b[1772:1796])
for k in range(6): print(f"{0.5*k:.1f}-{0.5*k+0.5:.1f} s: {hz[k]:+7.2f} Hz el = {hz[k]*15:+6.0f} rpm, Id {idd[k]:+.3f} A, Iq {iqq[k]:+.3f} A, Vd {vd[k]:+.3f} V, Vq {vq[k]:+.3f} V, peak {pp[k]} mA")
