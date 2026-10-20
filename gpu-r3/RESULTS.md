# gpu-r3 — membuktikan/menyanggah hipotesis K=2, bentuk non-monoton, dan jalur `fma()`

2026-10-08. Adreno 616, clock 610 MHz, chroot. Harness `vkopt` (NDK r30, template tunggal
`gen_grid.py`), 3 run/sel, gate eksak, ramp 500. Raw: `e2_lsweep.raw.txt`, `e4_nshape.raw.txt`,
`oclbin.raw2.txt`.

## E2 — pembeda spike K=2 (L-sweep) — HASIL

Grid L∈{32,64,128} × K∈{1,2,3,4}, n=65536, 36 sel, **0 gate FAIL**:

| L | K=1 | K=2 | K=3 | K=4 | K=2/K=1 |
|---|---|---|---|---|---|
| 32 | 42.5 | 57.6 | 42.9 | 43.7 | 1.36 |
| 64 | 81.4 | **112.3** | 84.6 | 86.3 | 1.38 |
| 128 | 71.4 | 103.0 | 78.1 | 79.3 | 1.44 |

**Kesimpulan:** spike K=2 **muncul di SETIAP L**, dengan rasio K=2/K=1 hampir konstan
(1.36–1.44). Posisi puncak **terpaku di K=2**, tidak bergerak dengan local_size.
→ **H2 (kuantisasi workgroup→wave) TERBANTAH**: kalau spike adalah efek boundary
workgroup/lane, ia harus bergeser saat L berubah dari 32→64→128. Tidak.
→ Konsisten dengan **H1/H6 (efek per-thread: register/ILP)**: rasio konstan lintas L
adalah tanda beban per-work-item, bukan per-workgroup. **Mekanisme persisnya tetap
hipotesis** — E2 hanya menyanggah satu kandidat (H2) dan mempersempit ke ranah per-thread.

Catatan: L=64 tetap absolut terbaik (112.3), jadi rekomendasi L=64 tetap berlaku.

## E4 — bentuk non-monoton K=3/8/16 pada 2 N — HASIL (sebagian)

| n | K=3 | K=8 | K=16 |
|---|---|---|---|
| 65536 | 85.3 (spread 1.2) | 85.8 (spread 0.0) | **88.2 (spread 0.1)** |
| 1048576 | 86.9 (spread 0.0) | 86.6 (spread 0.4) | **INVALID** |

**n=65536: non-monoton TERKONFIRMASI** — K=16 (88.2) > K=3 (85.3), rentang disjoint
(spread 0.1 vs 1.2). Reproducible.

**n=1M K=16: SEL INVALID** — dilaporkan 27.221.927 GFLOPS (spread 5.577.185), mustahil
secara fisik; wall_s efektif ≈0 → artefak timing/harness. **Dibuang, tidak dipakai.**
Jadi klaim non-monoton hanya sah pada n=65536; perilakunya di n besar **UNRESOLVED**.

## E1 — jalur `fma()` (dari sesi sebelumnya, dikonfirmasi)

Binary `fma()` 5272 B / private-mem 176 B vs `a*b+c`=`mad()` 2616 B / 64 B (2.02× / 2.75×).
`a*b+c` dan `mad()` **byte-identik** → jalur compiler berbeda untuk `fma()`.
Mekanisme (emulasi correctly-rounded) tetap **hipotesis** — disassembly proprietary belum berhasil.

## Status akhir hipotesis

| Hipotesis | Status setelah r3 |
|---|---|
| H2 kuantisasi workgroup→wave | **TERBANTAH** (spike tak bergerak dgn L) |
| H1 register/thread / H6 ILP per-thread | **DIDUKUNG** (rasio konstan lintas L), mekanisme belum dipaku |
| Non-monoton K=16>K=3 | **TERKONFIRMASI @n=64K**; unresolved @n=1M (sel invalid) |
| `fma()` jalur codegen berbeda | **TERBUKTI** (binary beda); mekanisme emulasi hipotesis |
| 117.9 GFLOPS ceiling | **TIDAK ADA** — best observed |

## Risiko/batas
- Satu sel (n=1M K=16) invalid → tidak boleh dikutip.
- E2 hanya menyanggah H2; H1/H3/H4/H5 belum diuji langsung (butuh reg-count/SLM).
- Semua klaim throughput tetap terikat konfigurasi (n, reps, L, K, ramp).

## ADDENDUM (2026-10-08) — adjudikasi finding advisor, terverifikasi

**1. Sel invalid n=1M K=16 = artefak harness, TERBUKTI.** Harness `vkopt_instr.c` mencetak `VkResult` tiap submit/wait. Hasil:
- n=1M K=16: gate `submit=0 wait=0 PASS`, tapi tiap rep terukur `submit=-4` (VK_ERROR_DEVICE_LOST), `wall_s≈0.00002` → GFLOPS mustahil. **Sel dibuang atas dasar bukti, bukan tebakan.** Akar: `vkopt.c` mengabaikan return code; submit gagal → wait balik instan → wall≈0.
- n=64K K=16 (anchor klaim non-monoton): `submit=0 wait=0` tiap rep, `wall_s=0.190` stabil, **88.243 GFLOPS** (cocok 88.2 r2). **Klaim non-monoton @n=64K SELAMAT** — bukan korupsi harness.

Implikasi: H4 (spike per-thread vs ukuran grid) tetap **BLOCKED** di n=1M sampai sel itu dijalankan ulang di sesi bersih; tapi tidak ada alasan meragukan kurva n=64K.

**2. Disasm `fma()` = hasil nyata, bukan asersi.** `file`→`data`, `readelf -h`→"wrong magic bytes", `llvm-objdump -d`→"not recognized as a valid object file" untuk ketiga binary. Jalur disassembly **tertutup secara terverifikasi**; mekanisme emulasi tetap hipotesis (dikuatkan private-mem 176 vs 64 B).

**Status akhir:** `fma()` jalur codegen beda = TERBUKTI; mekanisme = hipotesis. K=2 per-thread (H2 workgroup TERBANTAH) = didukung. Non-monoton @64K = terkonfirmasi; @1M = unresolved.
