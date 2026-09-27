import json,statistics,sys
from pathlib import Path
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent
summary=[]
for name in ['baseline','candidate','paint-cache','drawtext','empty-cache','baseline-paced','candidate-paced','inline-tail-paced']:
 r=json.loads((root/(name+'.json')).read_text(encoding='utf-8-sig'))
 samples=[s for s in r['retention'] if s['phase']=='scroll']
 sums=lambda field:[sum(h[field] for h in s['heaps']) for s in samples]
 item=dict(name=name,fixture=r['fixture'],benchmarkSha256=r['benchmarkSha256'],fps=statistics.median(s['fps'] for s in r['scenarios'][1]['rounds']),cppAllocations=statistics.median(s['cppAllocations'] for s in r['scenarios'][1]['rounds']),privateMedian=statistics.median(s['privateBytes'] for s in samples),busyMedian=statistics.median(sums('busyBytes')),freeMedian=statistics.median(sums('freeBytes')),privatePeak=max(s['privateBytes'] for s in samples),busyPeak=max(sums('busyBytes')),privateEnd=samples[-1]['privateBytes'],seconds=samples[-1]['elapsedMs']/1000,heapErrors=sum(h['error']!=0 for s in r['retention']+[r['detached']] for h in s['heaps']),detachedPrivate=r['detached']['privateBytes'],detachedBusy=sum(h['busyBytes'] for h in r['detached']['heaps']))
 summary.append(item)
(root/'summary.txt').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
for r in summary:
 print(r['name'], 'FPS',round(r['fps'],3),'private/busy/free medians',r['privateMedian'],r['busyMedian'],r['freeMedian'],'peak',r['privatePeak'],'end',r['privateEnd'],'duration',round(r['seconds'],2),'heapErrors',r['heapErrors'])
