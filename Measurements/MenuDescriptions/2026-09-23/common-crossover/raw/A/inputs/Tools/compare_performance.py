"""Compare matched complex-UI receipts; regressions require advice, never automatic rebaselining."""
import argparse
import json
import math
from pathlib import Path
from statistics import median

IDENTITY = ('fixture', 'renderer', 'width', 'height', 'dpi', 'controls', 'modelRows', 'framesPerRound', 'roundCount',
            'platform', 'configuration', 'nativeArchitecture', 'machine', 'cpu', 'os', 'compiler', 'warpVersion', 'powerPolicy')
METRICS = {'fps': ('higher', 0.05), 'frameP50Ms': ('lower', 0.05), 'frameP95Ms': ('lower', 0.05),
           'prepareP95Ms': ('lower', 0.05), 'composeCpuP95Ms': ('lower', 0.05),
           'privateBytes': ('lower', 0.02), 'privatePeakBytes': ('lower', 0.02),
           'workingSetBytes': ('lower', 0.02), 'workingSetPeakBytes': ('lower', 0.02),
           'surfaceBytes': ('lower', 0), 'replacementPeakBytes': ('lower', 0),
           'cppAllocations': ('lower', 0), 'composeAllocations': ('lower', 0)}


def validate(receipt):
    for key in IDENTITY + ('sourceCommit', 'sourceFingerprint', 'executableSha256', 'benchmarkSha256'):
        if key not in receipt or receipt[key] is None or receipt[key] == '':
            raise ValueError(f'Missing identity/evidence: {key}')
    if receipt['roundCount'] != 5 or receipt['framesPerRound'] != 40:
        raise ValueError('Expected five rounds of forty frames')
    scenarios = receipt.get('scenarios', [])
    if len(scenarios) != 2 or {s['name'] for s in scenarios} != {'clean', 'dirty'}:
        raise ValueError('Exactly clean and dirty scenarios are required')
    for scenario in scenarios:
        if len(scenario['rounds']) != receipt['roundCount']:
            raise ValueError('Incomplete measurement rounds')
        for record in scenario['rounds']:
            for metric in METRICS:
                value = record.get(metric)
                if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
                    raise ValueError(f'Invalid {scenario["name"]}/{metric}')
                if metric in ('fps', 'frameP50Ms', 'frameP95Ms') and value <= 0:
                    raise ValueError(f'Invalid zero {metric}')
            if record['composeAllocations'] != 0:
                raise ValueError('Composition allocated C++ heap memory')
    if receipt.get('hiddenPreparations') != 0 or receipt.get('hiddenComposites') != 0:
        raise ValueError('Hidden work must be zero')


def compare(before, after):
    validate(before)
    validate(after)
    for key in IDENTITY + ('benchmarkSha256',):
        if before[key] != after[key]:
            raise ValueError(f'Unmatched fixture: {key}')
    previous = {s['name']: s for s in before['scenarios']}
    changes = []
    for scenario in after['scenarios']:
        for metric, (direction, noise) in METRICS.items():
            old = median(r[metric] for r in previous[scenario['name']]['rounds'])
            new = median(r[metric] for r in scenario['rounds'])
            regressed = new < old * (1 - noise) if direction == 'higher' else new > old * (1 + noise)
            changes.append({'scenario': scenario['name'], 'metric': metric, 'before': old, 'after': new,
                            'changePercent': (new / old - 1) * 100 if old else None,
                            'noisePercent': noise * 100, 'regressed': regressed})
    return {'status': 'advice-required' if any(c['regressed'] for c in changes) else 'within-noise-budget', 'changes': changes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        candidate = json.loads(args.candidate.read_text(encoding='utf-8-sig'))
        validate(candidate)
        result = compare(json.loads(args.baseline.read_text(encoding='utf-8-sig')), candidate) if args.baseline else {'status': 'unpaired', 'changes': []}
    except (ValueError, KeyError, TypeError, OSError) as error:
        result = {'status': 'invalid-evidence', 'error': str(error), 'changes': []}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'Performance comparison: {result["status"]}')
    for change in result['changes']:
        if change['regressed']:
            print(f'REGRESSION {change["scenario"]}/{change["metric"]}: {change["before"]:.3f} -> {change["after"]:.3f}')
    if result['status'] == 'advice-required':
        print('Repeat on the same quiet fixture to confirm. Ask the developer for advice before accepting a confirmed regression. '
              'Options: optimize the affected work/cache, reduce optional scope, or defer/revert the change. Do not replace the baseline to pass.')
    return 1 if result['status'] in ('advice-required', 'invalid-evidence') else 0


if __name__ == '__main__':
    raise SystemExit(main())
