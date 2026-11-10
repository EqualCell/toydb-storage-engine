#!/usr/bin/env python3
"""Build fresh source copies and validate all real key/RID/record results per run."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile


def summaries(rows,output,config):
    groups={}
    for r in rows:groups.setdefault((r['method'],r['stage']),[]).append(r)
    medians=[]
    for (method,stage),group in groups.items():
        row={'method':method,'stage':stage,'repetitions':len(group)}
        for key in ('seconds','logical','physical_reads','physical_writes','results','index_pages','index_bytes','height'):
            row[key]=statistics.median(float(r[key]) for r in group)
        medians.append(row)
    with (output/'medians.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(medians[0]));writer.writeheader();writer.writerows(medians)
    def get(method,stage):return next((r for r in medians if r['method']==method and r['stage']==stage),None)
    lines=['# Phase 3 measured comparisons','',f"{config['buffer_capacity']} PF frames, {config['policy']}, {config['repetitions']} repetitions; timings are medians.",'',
           '| Method | End-to-end s | Existing-file index total s | Sorted construction + flush s | Pages | File bytes | Height |',
           '|---|---:|---:|---:|---:|---:|---:|']
    for method in config['methods']:
        e=get(method,'end_to_end');a=get(method,'index_total');b=get(method,'sorted_construct_total')
        lines.append(f"| {method} | {e['seconds']:.6f} | {a['seconds'] if a else '—'} | {b['seconds'] if b else '—'} | {int(e['index_pages'])} | {int(e['index_bytes'])} | {int(e['height'])} |")
    lines+=['','A/C existing-file totals include extraction, optional sorting, index construction and flush.','B end-to-end includes interleaved record-plus-index creation; it has no separately measured index-only build.','All end-to-end totals include student-file creation. Raw stage timings/counters are retained.','',
            '| Method | Stage | Median ms | Logical requests | Physical reads | Physical writes | Results |','|---|---|---:|---:|---:|---:|---:|']
    for r in medians:
        lines.append(f"| {r['method']} | {r['stage']} | {r['seconds']*1000:.3f} | {int(r['logical'])} | {int(r['physical_reads'])} | {int(r['physical_writes'])} | {int(r['results'])} |")
    lines+=['','Point batches query every distinct key. Fetch batches also resolve each encoded RID through RM and compare full bytes.',
            'Range batches use deterministic existing bounds and operators <, >, <=, >=, !=, < (default six queries).',
            'Each query stage reopens files after closing the previous stage, starting with an empty toyDB cache. The OS cache is not cleared.',
            'Index stages use index-only traffic; fetch and end-to-end stages include both student and index traffic. See raw CSV traffic column.',
            'Result sets, complete scans, node structure and all stored source bytes were verified; no key normalization is performed.']
    (output/'table.md').write_text('\n'.join(lines)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,default=Path(__file__).with_name('config.json'))
    parser.add_argument('--output',type=Path,default=Path(__file__).with_name('public-results'))
    args=parser.parse_args();config=json.loads(args.config.read_text())
    dataset=(args.config.resolve().parent/config['dataset']).resolve()
    if not dataset.is_file():raise ValueError(f'dataset missing: {dataset}')
    if config['policy'] not in ('LRU','MRU') or config['buffer_capacity']<3:raise ValueError('policy LRU/MRU; at least three frames for ordinary AM splits')
    if not 1<=config['key_length']<=255 or config['key_column']<1 or config['repetitions']<1 or config['range_queries']<1:raise ValueError('invalid key/experiment parameters')
    if not config['methods'] or any(m not in ('existing','incremental','bulk') for m in config['methods']):raise ValueError('invalid methods')
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[2]
    snapshot=dict(config);snapshot['dataset_sha256']=hashlib.sha256(dataset.read_bytes()).hexdigest()
    (output/'config.json').write_text(json.dumps(snapshot,indent=2)+'\n')
    rows=[]
    with tempfile.TemporaryDirectory(prefix='toydb-index-experiment-') as temp:
        work=Path(temp)
        for layer in ('pflayer','rmlayer','amlayer','indexing'):
            (work/layer).mkdir()
            for p in (root/layer).iterdir():
                if p.suffix in ('.c','.h') or p.name.lower()=='makefile':shutil.copy2(p,work/layer/p.name)
        with (output/'build.log').open('w') as log:
            r=subprocess.run(['make','-B','-C',str(work/'indexing'),'benchmark'],stdout=log,stderr=subprocess.STDOUT)
            if r.returncode:raise RuntimeError(f'build failed; see {output/"build.log"}')
        for repetition in range(1,config['repetitions']+1):
            for method in config['methods']:
                run=work/f'{method}-{repetition}';run.mkdir()
                for stage in ('build','query'):
                    command=[str(work/'indexing/benchmark'),stage,str(dataset),method,str(config['key_column']),str(config['key_length']),str(config['buffer_capacity']),config['policy'],str(int(config['skip_header'])),str(config['seed']),str(config['range_queries'])]
                    result=subprocess.run(command,cwd=run,text=True,capture_output=True)
                    (output/f'{method}-{repetition}-{stage}.log').write_text(result.stderr)
                    if result.returncode:raise RuntimeError(f'{command}\n{result.stdout}\n{result.stderr}')
                    parsed=list(csv.DictReader(io.StringIO(result.stdout)))
                    if not parsed:raise RuntimeError('no measured rows')
                    for row in parsed:row.update(repetition=repetition,seed=config['seed'],buffer_capacity=config['buffer_capacity'],policy=config['policy']);rows.append(row)
                print(f'PASS: {method} repetition {repetition}, fresh-process queries validated',flush=True)
    with (output/'raw.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    summaries(rows,output,config)
    print(f'PASS: {len(rows)} stage rows; raw CSV and median tables in {output}',flush=True)


if __name__=='__main__':main()
