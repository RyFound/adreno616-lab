#!/bin/bash
cd "$(dirname "$0")"
LK=/apex/com.android.runtime/bin/linker64
export LD_LIBRARY_PATH=/vendor/lib64:/system/lib64
out=logs/r5_b4b5.raw.txt
rm -f $out
run_cell() {
  local TAG=$1 K=$2
  for r in 1 2 3; do
    VKOPT_SPV=shaders/$TAG.spv VKOPT_RAMP=500 $LK ../../build/r4/vkopt8 2000 65536 10 64 $K ramp 2>&1 | grep -E 'GATE|RESULT|REP ' | sed "s/^/[$TAG r$r] /" >> $out
  done
}
run_cell r5_k1 1
run_cell r5_k2 2
run_cell r5_b5 2
run_cell r5_b4 4
echo R5_DONE >> $out
