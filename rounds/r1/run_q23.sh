#!/bin/bash
# Single-writer OpenCL campaign. Bounded for the pathologically slow Adreno CL runtime.
cd "$(dirname "$0")"
LK=/apex/com.android.runtime/bin/linker64
CL=/home/han/adreno616-lab/build/oclgrid
export LD_LIBRARY_PATH=/vendor/lib64:/system/lib64

# guard: kill any stray oclgrid (safe here: this script body is not in ps)
pkill -9 -f 'linker64 .*/oclgrid ' 2>/dev/null
sleep 1

# Q2: full 12-cell grid, reps=3, 2 process runs => 6 samples/cell
rm -f q2-opencl.raw.txt
for rep in 1 2; do
  echo "### REP=$rep Q2 n=65536 reps=3 $(date +%s)" >> q2-opencl.raw.txt
  $LK $CL 65536 2000 3 >> q2-opencl.raw.txt 2>&1
done
echo "Q2_DONE $(date +%s)" >> q2-opencl.raw.txt

# Q3: large n, restricted to lsize=64 (4 cells), reps=1, 3 process runs
rm -f q3-large-n.raw.txt
for rep in 1 2 3; do
  echo "### REP=$rep Q3 n=1048576 reps=1 lsize=64 $(date +%s)" >> q3-large-n.raw.txt
  OCL_LSIZE=64 $LK $CL 1048576 2000 1 >> q3-large-n.raw.txt 2>&1
done
echo "Q3_DONE $(date +%s)" >> q3-large-n.raw.txt
echo ALL_DONE
