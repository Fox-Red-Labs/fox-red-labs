import os,struct,sys
b=bytes.fromhex(open(sys.argv[1]).read().strip()); w=struct.unpack('<93I',b[:372]); i=0
def take(k,signed=False):
    global i; v=w[i:i+k]; i+=k
    return [x-(1<<32) if signed and x>>31 else x for x in v]
magic=take(1); zero=take(3); vm0=take(1); net=take(12,True); pk=take(12); stop=take(12); best=take(2); win=take(6); rnet=take(1,True); rpk=take(1); rstop=take(1); rvm=take(1); rcyc=take(1); done=take(1)
sf=3.3/4096*149.04/6.04; R={0:"none",1:"current trip",2:"nFAULT",3:"bus",4:"stall",5:"bad Hall"}
print("magic",hex(magic[0]),"done",hex(done[0]),"zeros",zero,"VM",round(vm0[0]*sf,1))
for t in range(12): print(f"table {t:2d} ({'with' if t<6 else 'against'} order, offset {t%6}): net sectors {net[t]:4d}, peak {pk[t]} mA, stop {R.get(stop[t],stop[t])}")
print("best",best[0],"run: sectors per 0.5 s",win,"net",rnet[0],"peak mA",rpk[0],"stop",rstop[0],"VM",round(rvm[0]*sf,1),"run s",round(rcyc[0]/32e6,2))
w2=w[91:93]; print("ADC: one blocking pair",w2[1],"cycles (",round(w2[1]/32,1),"us); longest wait after a period",w2[0],"cycles")
