import json
import statistics
import sys
from pathlib import Path

source = Path(sys.argv[1])
rows = [json.loads(line) for line in source.read_text(encoding='utf-8-sig').splitlines()
        if line.startswith('{"fixture":"dxui-menu-text-layout-resources-v1"')]
assert len(rows) == 384
samples = {}
for row in rows:
    assert row['rows'] == 12 and row['widthDip'] == 396
    assert row['heaps'] and all(h['error'] == 0 for h in row['heaps'])
    row['busy'] = sum(h['busyBytes'] for h in row['heaps'])
    phases = samples.setdefault((row['cycle'], row['mode']), {})
    assert row['phase'] not in phases
    phases[row['phase']] = row
assert set(samples) == {(c, m) for c in range(32) for m in range(3)}
assert all(set(p) == {'before', 'created', 'measured', 'released'} for p in samples.values())
summary = []
for mode in range(3):
    for phase in ('created', 'measured', 'released'):
        selected = [samples[c, mode] for c in range(16, 32)]
        values = [p[phase]['busy'] - p['before']['busy'] for p in selected]
        summary.append(dict(mode=mode, phase=phase, median=statistics.median(values),
                            minimum=min(values), maximum=max(values)))
result = dict(fixture='dxui-menu-text-layout-resources-v1', source=str(source),
              cycles=32, rows=12, widthDip=396, summary=summary)
source.with_suffix('.summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, indent=2))
