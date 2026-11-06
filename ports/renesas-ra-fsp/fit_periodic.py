import re,sys,collections
# fit host time against tick count for each period setting; slope = seconds per tick
seg=collections.defaultdict(list)
for ln in open(sys.argv[1]):
    m=re.match(r'\s*([\d.]+)\s.*\[periodic (\d+) ticks at (\d+) counts\]',ln)
    if m: seg[int(m.group(3))].append((int(m.group(2)),float(m.group(1))))
for counts,pts in seg.items():
    pts=pts[1:] if len(pts)>3 else pts
    n=len(pts); mx=sum(p[0] for p in pts)/n; my=sum(p[1] for p in pts)/n
    sxx=sum((p[0]-mx)**2 for p in pts); sxy=sum((p[0]-mx)*(p[1]-my) for p in pts)
    sl=sxy/sxx; nominal=counts/32e6
    res=max(abs(p[1]-(my+sl*(p[0]-mx))) for p in pts)
    print(f"{counts} counts: {n} markers over {pts[-1][1]-pts[0][1]:.2f} s, fitted {sl*1e3:.4f} ms per period, nominal {nominal*1e3:.4f} ms, {100*(sl/nominal-1):+.2f} %, max residual {res*1e3:.1f} ms")
