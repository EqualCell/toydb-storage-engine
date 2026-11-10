#!/usr/bin/env python3
import csv
from pathlib import Path
import argparse
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser=argparse.ArgumentParser()
parser.add_argument('--results',type=Path,default=Path(__file__).with_name('results'))
args=parser.parse_args()
with (args.results/'medians.csv').open() as f:rows=list(csv.DictReader(f))
methods=['existing','incremental','bulk']
def val(method,stage,field='seconds'):
    return float(next(r for r in rows if r['method']==method and r['stage']==stage)[field])
fig,axes=plt.subplots(2,2,figsize=(11,8),layout='constrained')
colors=['#2878a0','#db8c32','#479169']
axes[0,0].bar(methods,[val(m,'end_to_end') for m in methods],color=colors)
axes[0,0].set_ylabel('Seconds (median)');axes[0,0].set_title('End-to-end: student file + index + flush')
axes[0,1].bar(['A: existing total','C: bulk total','C: sorted build'],[1000*val('existing','index_total'),1000*val('bulk','index_total'),1000*val('bulk','sorted_construct_total')],color=[colors[0],colors[2],'#86bda0'])
axes[0,1].set_ylabel('Milliseconds (median)');axes[0,1].set_title('Index work with student file already present')
axes[0,1].tick_params(axis='x',labelsize=8)
axes[1,0].bar(methods,[val(m,'end_to_end','index_pages') for m in methods],color=colors)
axes[1,0].set_ylabel('Allocated PF pages');axes[1,0].set_title('Index size; 1020-byte AM nodes in 4096-byte pages')
x=list(range(3))
axes[1,1].bar([v-.18 for v in x],[1000*val(m,'point_index_cold') for m in methods],width=.36,label='Index only',color='#2878a0')
axes[1,1].bar([v+.18 for v in x],[1000*val(m,'point_fetch_cold') for m in methods],width=.36,label='Index + RM fetch',color='#db8c32')
axes[1,1].set_xticks(x,methods);axes[1,1].set_ylabel('Milliseconds (median)');axes[1,1].set_title('Every distinct key; initially empty toyDB cache');axes[1,1].legend()
for ax in axes.flat:
    ax.spines['top'].set_visible(False);ax.spines['right'].set_visible(False);ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
fig.suptitle('Real student data — three construction methods',fontsize=15)
fig.savefig(args.results/'comparison.png',dpi=160);fig.savefig(args.results/'comparison.svg')
print('Wrote comparison.png and comparison.svg')
