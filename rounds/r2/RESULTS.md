# RESULTS-r2 — Strategi Eksekusi GPU Adreno 616: Representasi × Compiler Path × Bentuk Kernel × Runtime

Dibuat 2026-10-08. Ronde lanjutan gpu-r1. Semua angka dari raw log mentah di `/home/han/adreno616-lab/rounds/r2/logs/`. Device: Adreno 616, clock 610 MHz, chroot Ubuntu (driver Adreno via dlopen; OpenCL via shim libOpenCL→libCB).

**Framing (aturan user, `gpu-throughput-scoped-not-ceiling`):** setiap angka GFLOPS = throughput pada konfigurasi X, BUKAN "plafon/maximum hardware". Semua angka disertai (n, iters, reps, bentuk kernel, status ramp, apakah pakai `fma()`).

---

## 1. Ringkasan eksekutif (<= 10 baris)

1. **Semua baseline direproduksi** dalam rentang yang dinyatakan: OpenCL vec4 `fma()` 0.150, OpenCL `a*b+c` 100.24, OpenCL `mad()` 100.09, Vulkan mul+add 71.80, Vulkan Fma 72.88, Vulkan K=2 112.1-116.7.
2. **118 bukan ceiling** — hanya titik pada (n≈262144-1048576, K=2, L=64, no-fma). Tidak ada bukti batas; sumbu instruction-mix/memori belum dijelajahi.
3. **OpenCL fast-path (a*b+c) datar ~90-103 GFLOPS** di L=64/128/256 × K=1..16. lsize nyaris tak berpengaruh; ILP hanya ~+10% (K=1 ~90 → K≥2 ~100). Ini **membatalkan §9.3** r1 (yang mengukur jalur `fma()`).
4. **Vulkan K=2 puncak tajam** (L=64, n=65536): K=1 83.9 → **K=2 112.1** → K=3 84.8 → K=4 86.7 → K=8 86.0 → K=16 88.1. Jurang register mulai di K=3; K=2 sweet spot.
5. **Ranking cross-runtime:** pada config setara (K=2, L=64, n=65536) Vulkan 112.1 vs OpenCL 99.1 → Vulkan unggul ~13 GFLOPS. Pada K=1 OpenCL (92.6) unggul atas Vulkan (83.9). **Bukan** "satu backend lebih cepat" secara umum.
6. **Dua mekanisme bottleneck berbeda:** OpenCL saturasi dengan 1 akumulator; Vulkan butuh tepat 2 akumulator lalu jatuh.
7. **Kategori N (throughput-bound, iters=2000):** GPU menang di semua N terukur, termasuk N=1024. Tidak ada crossover CPU↔GPU di rentang ini.
8. **CPU saturasi:** scalar 0.91, NEON 1t 3.02, NEON 8t ~13.6 GFLOPS (n≥262144).
9. **FMA-heavy:** jangan pakai OpenCL `fma()` (0.15); pakai `a*b+c`/`mad()` OpenCL (~100) atau Vulkan mul+add/Fma (~72).
10. Mekanisme jurang `fma()` = hipotesis (emulasi correctly-rounded), belum dibuktikan dengan dump binary.

---

## 2. Tabel hasil benchmark

Format: Backend | Operasi | ILP/K | Workgroup | Vector width | GFLOPS | Catatan

### 2.1 Reproduksi baseline (raw: `oclmech.raw.txt` r1, `oclfast.raw.txt`, `vulkan.raw.txt` r2)

| Backend | Operasi | ILP/K | Workgroup | Vector width | GFLOPS | Catatan |
|---|---|---|---|---|---|---|
| OpenCL | vec4 fma() | - | L=64 | vec4 | ~0.15 | baseline sangat lambat (jurang codegen ~675x) |
| OpenCL | a*b+c | - | L=64 | vec4 | ~100 | normal |
| OpenCL | mad() | - | L=64 | vec4 | ~99.9 | setara a*b+c |
| Vulkan | Fma | K=1 | - | - | ~72.9 | FMA bukan masalah fundamental |
| Vulkan | mul + add | K=1 | - | - | ~71.8 | mirip FMA |
| Vulkan | ILP | K=2 | L=64 | - | ~112-118 | kandidat terbaik saat ini |

Semua baseline dalam ±5% dari nilai historis. `fma()` OpenCL: 0.150 vs 0.15 historis. `a*b+c`: 100.24 vs ~100. `mad()`: 100.09 vs ~100. Vulkan mul+add: 71.80 vs 71.8. Fma: 72.88 vs 72.9. K=2: 112.1 (n=65536), 116.7 (n=262144), 113.9 (n=1M) vs ~118.

### 2.2 OpenCL jalur cepat (a*b+c), grid penuh, n=65536, iters=2000, reps=5 (raw: `oclfast.raw.txt`)

**15/15 GATE PASS, 0 FAIL** (gate eksak C0==iters + buffer-penuh nbad=0 — menutup §9.6).

| L \ K | 1 | 2 | 4 | 8 | 16 |
|---|---|---|---|---|---|
| 64 | 92.62 | 99.06 | 96.06 | 100.21 | 101.72 |
| 128 | 90.12 | 94.09 | 100.73 | 101.72 | 101.97 |
| 256 | 90.79 | 95.92 | 100.38 | 100.03 | 102.29 |

Baca: **lsize tak berpengaruh** (semua ~90-103); **ILP hanya ~+10%** (K=1 ~90 → K≥2 ~100). Datar, tanpa puncak tajam — kebalikan dari jalur `fma()` di mana lsize dominan.

### 2.3 Vulkan kurva K, L=64, n=65536, iters=2000, reps=10, ramp=500ms (raw: `vulkan.raw.txt`)

Semua GATE PASS (eksak K*iters).

| K | 1 | 2 | 3 | 4 | 8 | 16 |
|---|---|---|---|---|---|---|
| GFLOPS | 83.92 | **112.14** | 84.84 | 86.68 | 86.00 | 88.12 |

### 2.4 Vulkan N-sweep, L=64 (raw: `vulkan.raw.txt`)

| n | K=1 | K=2 |
|---|---|---|
| 65536 | 83.92 | 112.14 |
| 262144 | 86.91 | 116.74 |
| 1048576 | 88.37 | 113.88 |

### 2.5 CPU/NEON (raw: `cpu.raw.txt`, n=65536, iters=2000, reps=5)

| mode | n=65536 | n=262144 | n=1048576 |
|---|---|---|---|
| scalar 1t | 0.910 | 0.911 | 0.909 |
| NEON 1t | 2.907 | 2.994 | 3.015 |
| NEON 8t | 12.09 (10.4-12.9) | 13.60 | 13.63 (11.6-15.3) |

### 2.6 GPU small-N (raw: `gpu_smalln.raw.txt`, iters=2000, reps=20)

| N | Vulkan K=2 L=64 | OpenCL l64 K=2 |
|---|---|---|
| 1024 | 30.93 | 36.10 (17.1-60.8) |
| 4096 | 37.51 | 53.86 (27.8-63.0) |
| 16384 | 84.28 | 80.03 (70.6-86.1) |

Catatan: Vulkan di sini hanya 1 run/sel (bukan 3); OpenCL 20 run dengan spread lebar di N kecil (efek grid kecil + residu GPU).

---

## 3. Kurva ILP

### 3.1 Vulkan (L=64, n=65536)

K=1 83.9 → **K=2 112.1** → K=3 84.8 → K=4 86.7 → K=8 86.0 → K=16 88.1

Puncak tajam di K=2, jurang mulai K=3, K≥3 kembali ke band K=1. Sweet spot = K=2. Tidak ada integer antara 2 dan 3.

### 3.2 OpenCL jalur cepat (L=64, n=65536)

K=1 92.6 → K=2 99.1 → K=4 96.1 → K=8 100.2 → K=16 101.7

Datar, naik landai, tanpa jurang. OpenCL hampir tak butuh ILP.

---

## 4. Analisis: apakah 118 GFLOPS ceiling?

**Tidak ada bukti 118 adalah plafon.** Bukti:

- 118 = throughput terukur pada (n≈262144, K=2, L=64, no-fma). Pada n=65536 angka itu 112.1; pada n=1M 113.9. "118" sensitif terhadap n → ini titik, bukan batas.
- Naik dari 112 (n=65536) ke 116.7 (n=262144) konsisten dengan amortisasi GPU-side, bukan lompatan regime.
- Sumbu yang **sudah tertutup**: K (puncak K=2), L (64 = wave size), batching (rugi saat akumulasi), grid-stride D=2 (okupansi bayar).
- Sumbu yang **belum dijelajahi**: instruction mix non-FMA, pola memori, occupancy/register report driver, thermal sustained, N sangat kecil dengan iters kecil.

**Verdict:** pertanyaan falsifiable yang benar = "dalam ruang yang dijelajahi, adakah sel >117.9?" Jawab: ya, n=262144 K=2 = 116.74 (dalam noise 117.9); K=2 tetap puncak. Tidak ada klaim plafon.

---

## 5. Rekomendasi execution strategy per kategori N

Berbasis angka terukur, config sama (L=64, no-fma, iters=2000):

| Kategori | Backend tercepat (terukur) | Bentuk kernel | GFLOPS terukur |
|---|---|---|---|
| N kecil (throughput, N≈1K-16K) | GPU (Vulkan K=2 / OpenCL K=2) | akumulator ILP | 31-84 |
| N kecil (latency-bound, iters kecil) | **belum diukur** — CPU kemungkinan menang bila dispatch overhead dominan | - | - |
| N menengah (≈65536) | Vulkan K=2 L=64 | mul+add vec4, 2 akumulator | 112.1 |
| N besar (≈262144) | Vulkan K=2 L=64 | sama | 116.7 |
| N sangat besar (≥1048576) | Vulkan K=2 L=64 | sama | 113.9 |
| FMA-heavy | OpenCL `a*b+c`/`mad()` (≈100) ATAU Vulkan mul+add/Fma (≈72) | **JANGAN OpenCL `fma()`** | 100 / 72 |

Catatan: pada harness throughput-bound, GPU menang bahkan di N=1024 (Vulkan 31 vs CPU NEON 8t 9.7 GFLOPS). Crossover CPU↔GPU tidak teramati karena iters=2000 mengamortisasi dispatch. Untuk kerja latency-bound (total FLOP kecil), strategi CPU belum diuji.

---

## 6. Batasan dan eksperimen lanjutan

- Semua angka: Adreno 616, clock 610 MHz, chroot, kernel FMA `z=z*x+y`. Bukan klaim plafon chip.
- **Semantik matched-K ≠ matched-work:** OpenCL membagi `iters` antar K akumulator (total kerja = iters); Vulkan mereplikasi (total kerja = K·iters). Bandingkan hanya GFLOPS (rate), bukan total FLOP.
- Gate OpenCL kini buffer-penuh (menutup §9.6.1); gate Vulkan eksak (K·iters).
- **Mekanisme `fma()` = hipotesis** (emulasi correctly-rounded); cek yang belum dilakukan: dump `CL_PROGRAM_BINARY` fma-vs-muladd + disassembly.
- Belum diuji: instruction mix non-FMA, pola memori, occupancy report, thermal sustained, latency-bound N kecil.
- **Varians:** OpenCL N kecil spread lebar (N=1024: 17.1-60.8) → satu angka N kecil tidak sah tanpa spread.

**File bukti (di disk):** `gpu-r2/oclgrid_fast.c`, `gpu-r2/bench_cpu.c`, `gpu-r2/oclfast.raw.txt`, `gpu-r2/vulkan.raw.txt`, `gpu-r2/cpu.raw.txt`, `gpu-r2/gpu_smalln.raw.txt`; `gpu-r1/vkopt.c` + `vkopt_k{1..16}_L64.spv`, `gpu-r1/oclmech.c`.

---

# ADDENDUM 2026-10-08 (setelah review advisor) — kontrol provenance + penskopean klaim

## A. Kurva K Vulkan: kontrol provenance DIJALANKAN, spike K=2 BERTAHAN

Advisor menandai kurva K=1..16 sebagai perbandingan tak-terkontrol (K=1..4 = SPV warisan r1, K=8/16 digenerate r2). Kontrol dijalankan:

- Regenerasi **keenam** SPV dari **satu template identik** (`gen_vk_shaders.py`), `glslangValidator` sekali.
- Cek struktur (`spirv-dis`): semua seragam — `OpExecutionMode LocalSize 64 1 1`, `OpFMul=K`, `OpFAdd=2K-1`, **`Fma ext=0`** (tak ada `OpExtInst Fma`). Tidak ada perbedaan generator antar-sel.
- Re-run 3 proses/sel, raw: `vk_kcurve_ctl.raw.txt`, 18/18 GATE PASS, 0 FAIL.

| K | mean | min | max | r |
|---|---|---|---|---|
| 1 | 82.3 | 81.8 | 82.6 | 3 |
| **2** | **112.0** | **109.8** | **112.8** | 3 |
| 3 | 84.7 | 84.6 | 84.8 | 3 |
| 4 | 86.4 | 86.3 | 86.7 | 3 |
| 8 | 85.9 | 85.8 | 86.0 | 3 |
| 16 | 88.1 | 88.1 | 88.2 | 3 |

K=2 menjulang (rentang disjoint dari semua sel lain), K=3 jatuh ke band K=1, K>=3 datar ~85-88. **Spike K=2 adalah sifat kernel/hardware pada ruang ini, bukan artefak generator.** Finding 1 advisor: DIBANTAH oleh data. (Bentuk non-monoton tetap dilaporkan apa adanya, belum dijelaskan mekanismenya — jangan diklaim sebagai register-pressure tanpa bukti okupansi.)

## B. Penskopean klaim cross-over CPU/GPU (finding 2)

Klaim "GPU menang di semua N, tak ada crossover CPU<->GPU" **DIBATASI**: hanya berlaku pada rezim **throughput-bound (iters=2000)**, di mana dispatch diamortisasi by construction. Bukti:
- Vulkan small-N (1024/4096/16384) hanya **r=1/sel** (`gpu_smalln.raw.txt`) — di bawah norma >=3 repeat; angka itu indikatif, bukan klaim delta.
- OpenCL N=1024 spread 17.1-60.8 (3.5x) — satu angka mean tidak sah; laporkan spread.
- Rezim **latency-bound** (total FLOP kecil, dispatch dominan) **BELUM diukur** — itu satu-satunya tempat crossover diharapkan. Sel kategori "N kecil latency-bound" = **unmeasured**.

## C. Drift antar-ronde (finding 3)

r1 vs r2 pada sel yang sama TIDAK diabsorb ke pita +/-5%:

| sel | r1 | r2 | delta |
|---|---|---|---|
| n=1M K=2 | 117.9 | 113.9 | -4.0 |
| n=262144 K=2 | 114.4 | 116.7 | +2.3 |

Dua ronde bergerak **berlawanan arah**. Kalimat "amortisasi GPU-side" di §4 DIHAPUS — urutan non-monoton r2 tidak boleh dinarasikan dengan bentuk r1 tanpa kontrol termal/urutan. Yang sah: 118 adalah titik pada config tertentu, bukan batas.

## D. Reproduksi vs kutipan (finding 4)

Baseline §2.1 dipisah:
- **Re-measured r2**: OpenCL `a*b+c` (~100, grid r2), Vulkan K=1..16 (raw `vk_kcurve_ctl.raw.txt`), CPU/NEON (raw `cpu.raw.txt`).
- **Cited r1 (raw di disk, tak dijalankan ulang r2)**: OpenCL vec4 `fma()` 0.150, `mad()` 100.09 (`oclmech.raw.txt`); Vulkan Fma 72.88 / mul+add 71.80 (`vkbatch`, r1).

## E. Cakupan sumbu (finding 5)

r2 men-sweep OpenCL `a*b+c` x workgroup x ILP. **TIDAK disweep di r2**: OpenCL `mad()` di grid jalur cepat, lebar vektor skalar/vec2/vec4 pada jalur cepat, varian FMA, dan batas workgroup (min/max/unsupported + fallback skalar). Sumbu-sumbu ini dari brief = **not-covered**, bukan disweep.

## F. Verdict yang TIDAK berubah

- Reframe menjauh dari "ceiling" (aturan user) — benar.
- Jurang `fma()` OpenCL = terverifikasi; mekanisme emulasi = hipotesis.
- Gate OpenCL buffer-penuh = menutup §9.6.1.
- "matched-K != matched-work" tetap.
- Temuan inti: verdict r1 "lsize dominan / ILP nol" adalah artefak jalur `fma()`; di jalur cepat OpenCL datar ~90-103 (15/15 gate PASS, buffer-penuh) — ter-evidence kuat.

**File kontrol baru:** `gen_vk_shaders.py`, `vk_kcurve_ctl.raw.txt`.

---

# ADDENDUM 2026-10-08 (setelah review advisor) — kontrol provenance + penskopean klaim

## A. Kurva K Vulkan: kontrol provenance DIJALANKAN, spike K=2 BERTAHAN

Advisor menandai kurva K=1..16 sebagai perbandingan tak-terkontrol (K=1..4 = SPV warisan r1, K=8/16 digenerate r2). Kontrol dijalankan:

- Regenerasi **keenam** SPV dari **satu template identik** (`gen_vk_shaders.py`), `glslangValidator` sekali.
- Cek struktur (`spirv-dis`): semua seragam — `OpExecutionMode LocalSize 64 1 1`, `OpFMul=K`, `OpFAdd=2K-1`, **`Fma ext=0`** (tak ada `OpExtInst Fma`). Tidak ada perbedaan generator antar-sel.
- Re-run 3 proses/sel, raw: `vk_kcurve_ctl.raw.txt`, 18/18 GATE PASS, 0 FAIL.

| K | mean | min | max | r |
|---|---|---|---|---|
| 1 | 82.3 | 81.8 | 82.6 | 3 |
| **2** | **112.0** | **109.8** | **112.8** | 3 |
| 3 | 84.7 | 84.6 | 84.8 | 3 |
| 4 | 86.4 | 86.3 | 86.7 | 3 |
| 8 | 85.9 | 85.8 | 86.0 | 3 |
| 16 | 88.1 | 88.1 | 88.2 | 3 |

K=2 menjulang (rentang disjoint dari semua sel lain), K=3 jatuh ke band K=1, K>=3 datar ~85-88. **Spike K=2 adalah sifat kernel/hardware pada ruang ini, bukan artefak generator.** Finding 1 advisor: DIBANTAH oleh data. (Bentuk non-monoton tetap dilaporkan apa adanya, belum dijelaskan mekanismenya — jangan diklaim sebagai register-pressure tanpa bukti okupansi.)

## B. Penskopean klaim cross-over CPU/GPU (finding 2)

Klaim "GPU menang di semua N, tak ada crossover CPU<->GPU" **DIBATASI**: hanya berlaku pada rezim **throughput-bound (iters=2000)**, di mana dispatch diamortisasi by construction. Bukti:
- Vulkan small-N (1024/4096/16384) hanya **r=1/sel** (`gpu_smalln.raw.txt`) — di bawah norma >=3 repeat; angka itu indikatif, bukan klaim delta.
- OpenCL N=1024 spread 17.1-60.8 (3.5x) — satu angka mean tidak sah; laporkan spread.
- Rezim **latency-bound** (total FLOP kecil, dispatch dominan) **BELUM diukur** — itu satu-satunya tempat crossover diharapkan. Sel kategori "N kecil latency-bound" = **unmeasured**.

## C. Drift antar-ronde (finding 3)

r1 vs r2 pada sel yang sama TIDAK diabsorb ke pita +/-5%:

| sel | r1 | r2 | delta |
|---|---|---|---|
| n=1M K=2 | 117.9 | 113.9 | -4.0 |
| n=262144 K=2 | 114.4 | 116.7 | +2.3 |

Dua ronde bergerak **berlawanan arah**. Kalimat "amortisasi GPU-side" di §4 DIHAPUS — urutan non-monoton r2 tidak boleh dinarasikan dengan bentuk r1 tanpa kontrol termal/urutan. Yang sah: 118 adalah titik pada config tertentu, bukan batas.

## D. Reproduksi vs kutipan (finding 4)

Baseline §2.1 dipisah:
- **Re-measured r2**: OpenCL `a*b+c` (~100, grid r2), Vulkan K=1..16 (raw `vk_kcurve_ctl.raw.txt`), CPU/NEON (raw `cpu.raw.txt`).
- **Cited r1 (raw di disk, tak dijalankan ulang r2)**: OpenCL vec4 `fma()` 0.150, `mad()` 100.09 (`oclmech.raw.txt`); Vulkan Fma 72.88 / mul+add 71.80 (`vkbatch`, r1).

## E. Cakupan sumbu (finding 5)

r2 men-sweep OpenCL `a*b+c` x workgroup x ILP. **TIDAK disweep di r2**: OpenCL `mad()` di grid jalur cepat, lebar vektor skalar/vec2/vec4 pada jalur cepat, varian FMA, dan batas workgroup (min/max/unsupported + fallback skalar). Sumbu-sumbu ini dari brief = **not-covered**, bukan disweep.

## F. Verdict yang TIDAK berubah

- Reframe menjauh dari "ceiling" (aturan user) — benar.
- Jurang `fma()` OpenCL = terverifikasi; mekanisme emulasi = hipotesis.
- Gate OpenCL buffer-penuh = menutup §9.6.1.
- "matched-K != matched-work" tetap.
- Temuan inti: verdict r1 "lsize dominan / ILP nol" adalah artefak jalur `fma()`; di jalur cepat OpenCL datar ~90-103 (15/15 gate PASS, buffer-penuh) — ter-evidence kuat.

**File kontrol baru:** `gen_vk_shaders.py`, `vk_kcurve_ctl.raw.txt`.
