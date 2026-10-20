#!/bin/bash
# E7 (r4): reproduce the K-curve at L=64 on the UPGRADED harness (full-buffer gate +
# per-rep VkResult), and re-run the r3-invalid cell n=1M K=16 in a clean process.
# Harness vkopt8 = vkopt_instr + full-buffer gate (nbad) + submit/wait VkResult per rep.
cd "$(dirname "$0")"
LK=/apex/com.android.runtime/bin/linker64
export LD_LIBRARY_PATH=/vendor/lib64:/system/lib64
RAMP=500; REPS=3
out=e7_kcurve.raw.txt
rm -f $out

run_cell() {  # iters n L K tag
  local IT=$1 N=$2 L=$3 K=$4 TAG=$5
  local spv=/home/han/adreno616-lab/rounds/r3/vk_g_L${L}_K${K}.spv
  for r in 1 2 3; do
    VKOPT_SPV=$spv VKOPT_RAMP=$RAMP \
      $LK /home/han/adreno616-lab/build/vkopt8 $IT $N $REPS $L $K ramp 2>&1 \
      | grep -E 'GATE|RESULT|REP ' | sed "s/^/[$TAG r$r] /" >> $out
  done
}

echo "### E7a: K-curve at L=64, n=65536 (reproduce r3)" >> $out
for K in 1 2 3 4 8 16; do run_cell 2000 65536 64 $K E7a_L64; done

echo "### E7b: K=1,2 at L=32 and L=128 (spike across L, upgraded gate)" >> $out
for L in 32 128; do for K in 1 2; do run_cell 2000 65536 $L $K E7b_L${L}; done; done

echo "### E9: n=1048576 K=16 (r3 INVALID cell) + K=1 control, clean" >> $out
run_cell 2000 1048576 64 16 E9_n1M
run_cell 2000 1048576 64 1  E9_n1M
echo E7_E9_DONE >> $out
