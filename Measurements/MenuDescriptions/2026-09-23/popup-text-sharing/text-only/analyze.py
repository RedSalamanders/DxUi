from pathlib import Path
import json
from statistics import median

p = Path(__file__).parent
records = [json.loads(line) for line in (p/'run.log').read_text(encoding='utf-8-sig').splitlines() if line.startswith('{')]
assert len(records) == 384, len(records)
indexed = {}
for row in records:
    assert row['fixture'] == 'combined-layout-isolation-v1'
    assert all(h['error'] == 0 for h in row['heaps'])
    key = row['cycle'], row['mode'], row['phase']
    assert key not in indexed
    indexed[key] = sum(h['busyBytes'] for h in row['heaps'])
output = {}
for mode in range(3):
    result = {}
    for phase in ['created', 'measured', 'released']:
        values = [indexed[cycle, mode, phase] - indexed[cycle, mode, 'before'] for cycle in range(16,32)]
        result[phase] = {'median': median(values), 'min': min(values), 'max': max(values), 'samples': values}
    output[str(mode)] = result
(p/'summary.json').write_text(json.dumps(output, indent=2), encoding='utf-8')
print(json.dumps({mode: {phase: data['median'] for phase,data in result.items()} for mode,result in output.items()}, indent=2))
