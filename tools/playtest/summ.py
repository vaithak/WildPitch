import re,sys
for f in sys.argv[1:]:
  rows={}
  for l in open(f):
    m=re.match(r'(\w+) drib\s*(\d+) (.+?)\s+H=\s*(\d+) (tap |thru) v0=\s*([\d.]+).*got=\s*(-?\d+) in\s+(-?[\d.]+)s out=(\d).*ball@rcv\s+([\d.]+) rel\s+([\d.]+) rcv_v\s+([\d.]+) \| minV=([\d.]+) still=([\d.]+)s\s+mostReversed\(cos\)=([+-][\d.]+) ran=([\d.]+)',l)
    if not m: continue
    pol=m.group(3).strip(); typ=m.group(5).strip()
    k=(pol,typ); r=rows.setdefault(k,[])
    r.append(dict(v0=float(m.group(6)),got=int(m.group(7))==4,t=float(m.group(8)),out=int(m.group(9)),vb=float(m.group(10)),rel=float(m.group(11)),rv=float(m.group(12)),still=float(m.group(14)),cos=float(m.group(15))))
  print("==",f)
  for k in sorted(rows):
    r=rows[k]; g=[x for x in r if x['got']]
    av=lambda key,L: sum(x[key] for x in L)/len(L) if L else float('nan')
    mn=lambda key,L: min(x[key] for x in L) if L else float('nan')
    print(f"  {k[0]:14s} {k[1]:4s} recv {len(g):2d}/{len(r):2d} out {sum(x['out'] for x in r)} | v0 {mn('v0',r):4.1f}-{max(x['v0'] for x in r):4.1f} | when received: t {av('t',g):.2f}s ball {av('vb',g):4.1f} rcv_speed {av('rv',g):3.1f} (min {mn('rv',g):3.1f}) still {av('still',g):.2f}s worst-cos {mn('cos',g):+.2f}")
