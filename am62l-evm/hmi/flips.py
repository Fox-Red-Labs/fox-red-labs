import sys,re,statistics
def secs(t): h,m,s=t.split(':'); return int(h)*3600+int(m)*60+float(s)
lo,hi=sys.argv[2].split('..'); lo,hi=secs(lo),secs(hi)
n=0
for l in open(sys.argv[1]):
    if 'PAGE_FLIP' in l or 'MODE_ATOMIC' in l:
        t=secs(l.split()[1])
        if lo<=t<hi: n+=1
print(sys.argv[1], n, 'flips', round(n/(hi-lo),1),'fps')
