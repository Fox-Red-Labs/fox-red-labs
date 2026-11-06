import struct,sys,json
b=bytes.fromhex(open(sys.argv[1]).read().strip())[:14676]
w=struct.unpack('<7I',b[:28]); print("done",hex(w[1]),"bus",w[6]/1000, "cal stop", struct.unpack('<I',b[1596:1600])[0])
ref=struct.unpack('<4f',b[1796:1812]); tr=struct.unpack('<1600f',b[1812:8212]); iq=struct.unpack('<1600f',b[8212:14612])
mn=struct.unpack('<4f',b[14612:14628]); sd=struct.unpack('<4f',b[14628:14644]); qi=struct.unpack('<4f',b[14644:14660]); stop,lme,lmx,pk=struct.unpack('<4I',b[14660:14676])
print("stop",stop,"loop mean/max",lme,lmx,"of 8000; peak mA",pk)
for k in range(4):
    t=tr[k*400:(k+1)*400]; prev=ref[k-1] if k else 0.0; r=ref[k]; step=r-prev
    def first(fr):
        for j,x in enumerate(t):
            if (x-prev)/step>=fr: return j*0.5
        return None
    t10,t90=first(0.1),first(0.9); ov=max(((x-r)/step*100 for x in t),default=0)
    print(f"step to {r:.0f} Hz el ({r*15:.0f} rpm): 10-90 % {t10} to {t90} ms, overshoot {ov:.0f} %, last 0.5 s mean {mn[k]:.2f} Hz, spread {sd[k]:.2f} Hz, Iq {qi[k]*1000:.0f} mA")
json.dump(dict(ref=ref,trace=[tr[k*400:(k+1)*400] for k in range(4)],iq=[iq[k*400:(k+1)*400] for k in range(4)]),open(sys.argv[2],'w')) if len(sys.argv)>2 else None
