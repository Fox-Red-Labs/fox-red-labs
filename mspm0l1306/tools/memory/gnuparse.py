import re,sys,collections
APP="Your app (the example's own code)"; RT="RTOS"; DRV="TI drivers and device library"; CL="C library and compiler runtime"; HS="Heap and stacks"; OT="Other"
def classify(sec,obj,ram=False):
    nm=sec.lower()
    if ram and (re.search(r'(stack|arena)',nm) or ('kernel/libkernel.a(init.c.obj)' in obj and sec.startswith('.noinit'))): return HS
    if 'libgcc.a' in obj or 'modules/picolibc' in obj or 'liblib__libc__picolibc' in obj or 'liblib__libc__common' in obj: return CL
    if 'hal__ti__mspm0' in obj: return DRV
    if obj.startswith('app/libapp.a'): return APP
    if obj=='*fill*': return OT
    return RT
def parse(path):
    L=open(path).read().split('Linker script and memory map')[1].splitlines()
    ents=[]; osec=None; run=None; load=None; i=0
    while i<len(L):
        l=L[i]
        m=re.match(r'^(\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)(?:\s+load address 0x([0-9a-f]+))?',l)
        if m and not l.startswith('LOAD'):
            osec=m.group(1); run=int(m.group(2),16); load=int(m.group(4),16) if m.group(4) else None; i+=1; continue
        m=re.match(r'^(\S+)\s*$',l)
        if m and not l.startswith('LOAD') and not l.startswith('OUTPUT') and i+1<len(L):
            m2=re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)(?:\s+load address 0x([0-9a-f]+))?\s*$',L[i+1])
            if m2: osec=m.group(1); run=int(m2.group(1),16); load=int(m2.group(3),16) if m2.group(3) else None; i+=2; continue
        m=re.match(r'^ (\*fill\*|\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s*(.*)$',l)
        if m:
            name,a,s,o=m.group(1),int(m.group(2),16),int(m.group(3),16),m.group(4).strip()
            if name!='*fill*' and not o: pass
            ents.append((name,a,s,o or '*fill*',osec,run,load)); i+=1; continue
        m=re.match(r'^ (\.\S+)\s*$',l)
        if m and i+1<len(L):
            m2=re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$',L[i+1])
            if m2:
                ents.append((m.group(1),int(m2.group(1),16),int(m2.group(2),16),m2.group(3).strip(),osec,run,load)); i+=2; continue
        i+=1
    return ents,'\n'.join(L)
def account(path,fused,rused):
    ents,txt=parse(path)
    fo={}; ro={}  # byte -> (comp,obj)
    def put(d,a,s,c,o):
        for b in range(a,a+s):
            if b not in d: d[b]=(c,o)
    for name,a,s,o,osec,run,load in ents:
        if s==0 or (osec or '').startswith(('.debug','.comment','.ARM','.symtab','.strtab','.shstrtab','.stab')): continue
        c=classify(name,o,a>=0x20000000)
        if a<0x10000: put(fo,a,s,c,o)
        elif 0x20000000<=a<0x20001000:
            put(ro,a-0x20000000,s,c,o)
            if load is not None: put(fo,load+(a-run),s,c,o)
    fl=collections.Counter(); ra=collections.Counter()
    gapf=0; gapr=0
    for b in range(fused):
        if b in fo: fl[fo[b][0]]+=1
        else: fl[OT]+=1; gapf+=1
    for b in range(rused):
        if b in ro: ra[ro[b][0]]+=1
        else: ra[OT]+=1; gapr+=1
    over_f=[b for b in fo if b>=fused]; over_r=[b for b in ro if b>=rused]
    return fl,ra,gapf,gapr,len(over_f),len(over_r),fo,ro
if __name__=='__main__':
    p,f,r=sys.argv[1],int(sys.argv[2]),int(sys.argv[3])
    fl,ra,gf,gr,of,orr,fo,ro=account(p,f,r)
    print(dict(fl),sum(fl.values()),f); print(dict(ra),sum(ra.values()),r); print('gaps',gf,gr,'beyond',of,orr)
