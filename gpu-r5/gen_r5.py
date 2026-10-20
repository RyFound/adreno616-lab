#!/usr/bin/env python3
# r5 generator: emit every cell from ONE template so SPIR-V provenance is uniform.
#
# cell: (name, ACC, FMAS)
#   z[i] = sum(a_j) - offset, with a_j (j<FMAS) updated in the loop and
#   a_j (j>=FMAS) merely LIVE (initialised from y outside the loop).
#   => z = FMAS * iters, so run the harness with K=FMAS.
import sys, os

L = 64
CELLS = [
    ("r5_k1", 1, 1),   # baseline, latency-bound
    ("r5_k2", 2, 2),   # K=2 ILP reference
    ("r5_b5", 4, 2),   # 4 acc live, 2 FMA
    ("r5_b4", 4, 4),   # 4 acc live, 4 FMA
]

def emit(ACC, FMAS):
    s = []
    s.append('#version 450')
    s.append('// r5: one template. ACC=%d FMAS=%d L=%d' % (ACC, FMAS, L))
    s.append('layout(local_size_x = %d) in;' % L)
    s.append('layout(std430, binding = 0) buffer X { vec4 x[]; };')
    s.append('layout(std430, binding = 1) buffer Y { vec4 y[]; };')
    s.append('layout(std430, binding = 2) buffer Z { vec4 z[]; };')
    s.append('layout(push_constant) uniform PC { uint iters; } pc;')
    s.append('void main() {')
    s.append('    uint i = gl_GlobalInvocationID.x;')
    s.append('    vec4 xv = x[i];')
    s.append('    vec4 yv = y[i];')
    for j in range(FMAS):
        s.append('    vec4 a%d = vec4(0.0);' % j)
    # extra accumulators: live, not updated inside the loop
    offset = 0
    for j in range(FMAS, ACC):
        if j == FMAS:
            s.append('    vec4 a%d = yv;' % j)
            offset += 1
        else:
            s.append('    vec4 a%d = a%d + yv;' % (j, j - 1))
            offset += (offset + 1) if False else 0
    # recompute offset cleanly
    offset = sum(range(1, ACC - FMAS + 1))
    s.append('    for (uint k = 0u; k < pc.iters; k++) {')
    for j in range(FMAS):
        s.append('        a%d = a%d * xv + yv;' % (j, j))
    s.append('    }')
    expr = ' + '.join('a%d' % j for j in range(ACC))
    if offset:
        s.append('    z[i] = %s - vec4(%.1f);' % (expr, float(offset)))
    else:
        s.append('    z[i] = %s;' % expr)
    s.append('}')
    return '\n'.join(s) + '\n'

base = sys.argv[1] if len(sys.argv) > 1 else '.'
for name, acc, fmas in CELLS:
    p = os.path.join(base, name + '.comp')
    open(p, 'w').write(emit(acc, fmas))
    print('wrote', p, 'ACC=%d FMAS=%d' % (acc, fmas))
