import json
from pathlib import Path
from statistics import median

root = Path(__file__).parent
metrics = {'fps': -5, 'frameP50Ms': 5, 'frameP95Ms': 5, 'prepareP95Ms': 5,
           'composeCpuP95Ms': 5, 'privateBytes': 2, 'privatePeakBytes': 2,
           'workingSetBytes': 2, 'workingSetPeakBytes': 2, 'surfaceBytes': 0,
           'replacementPeakBytes': 0, 'cppAllocations': 0, 'composeAllocations': 0}
results = []
for configuration in ['Debug', 'Release', 'ASan Debug']:
    variants = {variant: [json.loads(path.read_text(encoding='utf-8-sig')) for path in sorted((root/'interleaved').glob(f'{configuration}-*-{variant}.json'))] for variant in ['A','B']}
    assert all(len(values) == 4 for values in variants.values())
    for scenario in ['clean', 'dirty']:
        for metric, band in metrics.items():
            values = {variant: [median(r[metric] for s in receipt['scenarios'] if s['name'] == scenario for r in s['rounds']) for receipt in receipts] for variant, receipts in variants.items()}
            a, b = median(values['A']), median(values['B'])
            delta = 100*(b/a - 1) if a else 0 if b == 0 else None
            flag = b < a*(1+band/100) if metric == 'fps' else b > a*(1+band/100)
            row = dict(configuration=configuration, scenario=scenario, metric=metric, baseline=a, candidate=b, delta=b-a, percent=delta, flag=flag, processMedians=values)
            results.append(row)
            if flag:
                print(f'{configuration} {scenario}/{metric}: {a:g} -> {b:g} ({delta:+.3f}%), ranges A={min(values["A"]):g}..{max(values["A"]):g}, B={min(values["B"]):g}..{max(values["B"]):g}')
    for receipts in variants.values():
        assert all(r['hiddenPreparations'] == 0 and r['hiddenComposites'] == 0 for r in receipts)
(root/'interleaved-summary.json').write_text(json.dumps(results, indent=2)+'\n')
print(f'All {len(results)} profile/scenario/metric comparisons retained; {sum(r["flag"] for r in results)} investigation flags.')
