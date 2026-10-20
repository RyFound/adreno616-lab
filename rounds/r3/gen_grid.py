#!/usr/bin/env python3
# Generate vkopt shaders for a full (L x K) grid from ONE identical template.
# Uniform template => uniform SPIR-V provenance (control from r2).
import sys, os
base = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'shaders')
LS = [32, 64, 96, 128]
KS = [1, 2, 3, 4, 8, 16]
for L in LS:
    for K in KS:
        s = []
        s.append('#version 450')
        s.append('// generated from one template, L=%d K=%d' % (L, K))
        s.append('layout(local_size_x = %d) in;' % L)
        s.append('layout(std430, binding = 0) buffer X { vec4 x[]; };')
        s.append('layout(std430, binding = 1) buffer Y { vec4 y[]; };')
        s.append('layout(std430, binding = 2) buffer Z { vec4 z[]; };')
        s.append('layout(push_constant) uniform PC { uint iters; } pc;')
        s.append('void main() {')
        s.append('    uint i = gl_GlobalInvocationID.x;')
        s.append('    vec4 xv = x[i];')
        s.append('    vec4 yv = y[i];')
        for j in range(K):
            s.append('    vec4 a%d = vec4(0.0);' % j)
        s.append('    for (uint k = 0u; k < pc.iters; k++) {')
        for j in range(K):
            s.append('        a%d = a%d * xv + yv;' % (j, j))
        s.append('    }')
        s.append('    z[i] = ' + ' + '.join('a%d' % j for j in range(K)) + ';')
        s.append('}')
        path = os.path.join(base, 'vk_g_L%d_K%d.comp' % (L, K))
        open(path, 'w').write('\n'.join(s) + '\n')
        print('wrote', path)
