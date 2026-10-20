#!/usr/bin/env python3
# E5 (r4): static SPIR-V analysis of the whole L x K grid.
# Goal: decide whether the K=2 anomaly can live in SPIR-V structure, or must be
# entirely in the driver's codegen. Deterministic, no GPU, free information.
import os, re, subprocess, sys, glob

base = sys.argv[1] if len(sys.argv) > 1 else '/home/han/adreno616-lab/gpu-r3'

def stats(path):
    try:
        d = subprocess.run(['spirv-dis', path], capture_output=True, text=True, timeout=30)
    except Exception as e:
        return None, 'dis-fail %s' % e
    if d.returncode != 0:
        return None, 'dis-rc=%d' % d.returncode
    txt = d.stdout
    ops = re.findall(r'\s(Op[A-Za-z]+)\s', txt)
    hist = {}
    for o in ops:
        hist[o] = hist.get(o, 0) + 1
    ids = set(int(m) for m in re.findall(r'%([0-9]+)\s*=', txt))
    maxid = max(ids) if ids else 0
    vals = re.findall(r'OpExecutionMode\s+%\w+\s+(\w+)', txt)
    # count ops inside the loop body only (between OpLoopMerge block and its branch back)
    v = subprocess.run(['spirv-val', path], capture_output=True, text=True, timeout=30)
    return {
        'bytes': os.path.getsize(path),
        'total_ops': len(ops),
        'fmul': hist.get('OpFMul', 0),
        'fadd': hist.get('OpFAdd', 0),
        'fma_ext': hist.get('OpExtInst', 0),
        'fdiv': hist.get('OpFDiv', 0),
        'load': hist.get('OpLoad', 0),
        'store': hist.get('OpStore', 0),
        'composite': hist.get('OpCompositeExtract', 0) + hist.get('OpCompositeConstruct', 0),
        'branch': hist.get('OpBranch', 0) + hist.get('OpBranchConditional', 0),
        'loopmerge': hist.get('OpLoopMerge', 0),
        'phi': hist.get('OpPhi', 0),
        'maxid': maxid,
        'n_ids': len(ids),
        'exe': ','.join(vals),
        'val': 'ok' if v.returncode == 0 else 'VAL_FAIL',
    }, None

rows = []
for path in sorted(glob.glob(os.path.join(base, 'vk_g_L*_K*.spv'))):
    m = re.search(r'vk_g_L(\d+)_K(\d+)\.spv$', path)
    if not m:
        continue
    L, K = int(m.group(1)), int(m.group(2))
    st, err = stats(path)
    if st is None:
        rows.append((L, K, None, err)); continue
    rows.append((L, K, st, None))

rows.sort(key=lambda r: (r[0], r[1]))
hdr = ['L','K','bytes','ops','fmul','fadd','ext','load','store','comp','br','lm','phi','maxid','nids','val']
print('\t'.join(hdr))
for L, K, st, err in rows:
    if st is None:
        print('%d\t%d\tERR\t%s' % (L, K, err)); continue
    print('\t'.join(str(x) for x in [L,K,st['bytes'],st['total_ops'],st['fmul'],st['fadd'],st['fma_ext'],st['load'],st['store'],st['composite'],st['branch'],st['loopmerge'],st['phi'],st['maxid'],st['n_ids'],st['val']]))

print()
print('--- K-curve at fixed L (L=64) ---')
for L, K, st, err in rows:
    if L != 64 or st is None:
        continue
    print('K=%2d bytes=%5d ops=%4d fmul=%2d fadd=%2d load=%3d store=%2d maxid=%4d nids=%4d val=%s'
          % (K, st['bytes'], st['total_ops'], st['fmul'], st['fadd'], st['load'], st['store'], st['maxid'], st['n_ids'], st['val']))
