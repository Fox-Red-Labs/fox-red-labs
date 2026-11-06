import re,sys,json,collections
def parse(path):
    txt=open(path).read()
    mc=re.search(r'^\s+FLASH\s+(\w+)\s+(\w+)\s+(\w+)',txt,re.M); fu=int(mc.group(3),16)
    ms=re.search(r'^\s+SRAM\s+(\w+)\s+(\w+)\s+(\w+)',txt,re.M); ru=int(ms.group(3),16)
    sec=txt.split('SECTION ALLOCATION MAP')[1].split('MODULE SUMMARY')[0]
    fl=collections.defaultdict(int); ra=collections.defaultdict(int); det=collections.defaultdict(int)
    cur=None; lib=None; osec=None
    for line in sec.splitlines():
        m=re.match(r'^(\S+)\s+\d+\s+([0-9a-f]{8})\s+([0-9a-f]{8})',line)
        if m: osec=m.group(1); lib=None; continue
        if re.match(r'^\S',line) and not line.startswith(' '): 
            osec=line.split()[0] if line.split() else osec; lib=None; continue
        m=re.match(r'^\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+(.*)$',line)
        if not m: continue
        a=int(m.group(1),16); n=int(m.group(2),16); rest=m.group(3)
        if n==0: continue
        if a<0x10000: reg=fl
        elif 0x20000000<=a<0x20001000: reg=ra
        else: continue
        if 'HOLE' in rest and osec in ('.stack','.sysmem'): c='Heap and stacks'; obj='hole'
        elif 'HOLE' in rest: c='Other'; obj='hole'
        else:
            mm=re.match(r'(?:(\S+)\s+)?:\s+(\S+)\s+\((.*)\)',rest)
            if mm:
                if mm.group(1): lib=mm.group(1)
                obj=mm.group(2)
            else:
                lib=None
                mm=re.match(r'(\S+)\s+\((.*)\)',rest)
                if mm: obj=mm.group(1)
                else:
                    mm=re.match(r'\((.*?)\)',rest); obj='('+mm.group(1)+')'
            if osec in ('.stack','.sysmem'): c='Heap and stacks'
            elif obj.startswith('(.cinit') or obj.startswith('(__TI'): c='Other'
            elif lib and 'driverlib' in lib: c='TI drivers and device library'
            elif lib and re.search(r'libc|libsysbm|libclang_rt|libm',lib): c='C library and compiler runtime'
            elif lib and 'model' in lib: c='Your app (the example\'s own code)'
            elif lib: c='LIB?'+lib
            elif obj.startswith('startup') or obj.startswith('ticlang_startup') or obj.startswith('ti_msp_dl_config'): c='TI drivers and device library'
            elif obj.startswith('(.common'): c='Your app (the example\'s own code)'
            else: c='Your app (the example\'s own code)'
        reg[c]+=n; det[(c,obj)]+=n
    return fu,ru,fl,ra,det
if __name__=='__main__':
    fu,ru,fl,ra,det=parse(sys.argv[1])
    print(fu,sum(fl.values()),ru,sum(ra.values())); print(dict(fl)); print(dict(ra))
    if len(sys.argv)>2:
        for k,v in sorted(det.items()): print(k,v)
