#!/usr/bin/env python3
# Generate vkopt K=1..16 L=64 compute shaders from ONE identical template.
# Control for advisor finding 1: single template => uniform SPIR-V provenance.
import sys, os
base = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'shaders')
os.makedirs(base, exist_ok=True)
L = 64
for K in (1,2,3,4,8,16):
    s = []
    s.append('#version 450')
    s.append('// generated from one template, K=%d L=%d' % (K, L))
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
    path = os.path.join(base, 'vkopt_k%d_L64.comp' % K)
    open(path, 'w').write('\n'.join(s) + '\n')
    print('wrote', path)
