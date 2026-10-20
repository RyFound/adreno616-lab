#!/bin/bash
# E2: L-sweep to discriminate H1 (regs/occupancy) vs H2 (workgroup->wave quantization).
# If the K=2 spike moves with L -> H2. If it stays pinned at K=2 for all L -> not H2.
cd "$(dirname "$0")"
LK=/apex/com.android.runtime/bin/linker64
export LD_LIBRARY_PATH=/vendor/lib64:/system/lib64
N=65536; IT=2000; R=3; RAMP=500
out=logs/e2_lsweep.raw.txt
rm -f $out
for L in 32 64 128; do
  for K in 1 2 3 4; do
    spv=shaders/vk_g_L${L}_K${K}.spv
    for r in 1 2 3; do
      VKOPT_SPV=$spv VKOPT_RAMP=$RAMP $LK ../../build/r3/vkopt $IT $N $R $L $K ramp 2>&1 | grep -E 'GATE|RESULT' >> $out
    done
  done
done
echo E2_DONE >> $out
