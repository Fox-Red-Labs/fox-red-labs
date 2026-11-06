import re,sys,statistics,collections
txt=open(sys.argv[1]).read()
blocks=re.split(r'^=== ',txt,flags=re.M)
data=collections.defaultdict(lambda: collections.defaultdict(list))
for b in blocks:
    m=re.match(r'rep (\d) zmode/lmode (\d) (\d)',b)
    if not m: continue
    z,l=m.group(2),m.group(3)
    key=('z=%s l=%s'%('poll' if z=='1' else 'irq','poll' if l=='1' else 'irq'))
    for sec in re.split(r'\n(?=rtt size|tput )',b):
        h=re.match(r'rtt size=(\d+) n=(\d+)',sec)
        if h:
            for name in ('rtt','to-thread','linux>zeph','zeph-proc','zeph>linux'):
                mm=re.search(r'^%s\s+median\s+(\d+)\s+min\s+(\d+)\s+p90\s+(\d+)\s+p99\s+(\d+)\s+max\s+(\d+)'%re.escape(name),sec,re.M)
                if mm: data[key][('rtt',h.group(1),name)].append(tuple(map(int,mm.groups())))
        t=re.match(r'tput (\S+) size=(\d+)',sec)
        if t:
            mm=re.search(r'elapsed (\d+) ns\s+(\d+) msgs/s\s+(\d+) kB/s',sec)
            bad=re.search(r'bad (\d+)',sec); cm='COUNT MISMATCH' in sec
            if mm: data[key][('tput',t.group(2),t.group(1))].append((int(mm.group(2)),int(mm.group(3)),int(bad.group(1)) if bad else 0,cm))
for key in sorted(data):
    print('##',key)
    for k,v in sorted(data[key].items()):
        if k[0]=='rtt':
            med=[x[0] for x in v]; print('  rtt %s %-10s medians %s -> median %d ns  (min of mins %d, max of p99 %d)'%(k[1],k[2],med,statistics.median(med),min(x[1] for x in v),max(x[3] for x in v)))
        else:
            r=[x[0] for x in v]; kb=[x[1] for x in v]; print('  tput %s %-14s msgs/s %s kB/s %s -> median %d msgs/s %d kB/s  bad=%s mismatch=%s'%(k[1],k[2],r,kb,statistics.median(r),statistics.median(kb),[x[2] for x in v],[x[3] for x in v]))
