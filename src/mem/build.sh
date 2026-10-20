#!/bin/sh
# Build bench sekecil mungkin tanpa merusak akurasi benchmark.
# -Os       : optimasi ukuran untuk semua kode
# HOT kernel: mem_kern_{write,read,copy} tetap di-O3 via __attribute__((optimize))
#             supaya loop tidak jatuh ke store-port (trap -O2 yang bikin angka datar).
# kernels.S : kernel NEON tangan (8 accumulator, 128 B/iter) untuk angka atap sejati.
# -ffunction-sections -fdata-sections + --gc-sections : buang kode mati
# -fno-asynchronous-unwind-tables -fno-unwind-tables : tabel unwind tak dipakai
# -fno-ident -Wl,--build-id=none                     : buang string ident & build-id
# -fno-stack-protector -Wl,-z,norelro                : boleh untuk tool lokal
# strip -s                                           : buang simbol
set -e
cd "$(dirname "$0")"
FL='-ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident -fno-stack-protector -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro'
mkdir -p ../../build
gcc -Os -pthread $FL -o ../../build/bench bench.c kernels.S -w
strip -s ../../build/bench
ls -l ../../build/bench
