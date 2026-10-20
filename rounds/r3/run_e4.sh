#!/bin/bash
# E4: is the non-monotonic Vulkan shape (K=16 > K=3) a per-N artifact or stable? Two N.
cd "$(dirname "$0")"
LK=/apex/com.android.runtime/bin/linker64
export LD_LIBRARY_PATH=/vendor/lib64:/system/lib64
R=3; RAMP=500
out=e4_nshape.raw.txt
rm -f $out
for N in 65536 1048576; do
  for K in 3 8 16; do
    spv=/home/han/adreno616-lab/rounds/r3/vk_g_L64_K${K}.spv
    for r in 1 2 3; do
      VKOPT_SPV=$spv VKOPT_RAMP=$RAMP $LK /home/han/adreno616-lab/build/vkopt 2000 $N $R 64 $K ramp 2>&1 | grep -E 'GATE|RESULT' >> $out
    done
  done
done
echo E4_DONE >> $out
