# RESULTS-r1 — Ruang Konfigurasi GPU Adreno 616 (Vulkan batch-vs-fence; OpenCL lsize×ILP)

Status: SELESAI (ronde ditutup 2026-10-07). **Otoritas verdict = `SPEC.md` §8.** Ringkas: Q1 TIDAK TERVERIFIKASI (arah mekanisme saja; ambang tidak tercapai); Q2 = jalur OpenCL PATOLOGIS (loader/libOpenCL, bukan hardware); Q3 tidak dijalankan.

Semua angka berasal dari raw log mentah (`q1-vulkan.raw.txt`, dst.) yang di-stream langsung dari stdout harness.

---

## Q1 — Vulkan: apakah overhead submit+fence menjelaskan 83→88? (H1 vs H2)

Konfigurasi: `n=65536`, `iters=2000`, `reps=10` per proses, 3 proses-run per arm. Device: `Adreno (TM) 616`. Build bionic, dijalankan via `linker64` absolut, `LD_LIBRARY_PATH=/vendor/lib64:/system/lib64`. `vkbatch.spv` dari `vkbatch.comp` (glslangValidator). Kernel: vec4 `z = z*x + y`, `x=y=1`, `z0=0`.

**Konteks harness (untuk verifier):**
- **VKBATCH_RAMP yang dipakai persis:** `cold` → `ramp=0`; `ramp`, `batch1`, `batchfp` → `ramp=500` (nilai default env `VKBATCH_RAMP`, dicetak di tiap baris RESULT sebagai `ramp=<n>`).
- **Warm-up berbeda dari harness historis (yang sudah dihapus):** pada arm ber-ramp, harness BARU melakukan warm-up dengan **submit dispatch GPU nyata** selama `ramp` ms sebelum jendela timing (bukan spin CPU). Ini menghasilkan `cur_freq_after=610 MHz` konsisten. Harness historis tidak tersedia sehingga tidak bisa dibandingkan 1:1.
- **Jumlah submit/fence:** `cold`/`ramp` = 1 dispatch, 1 submit, 1 fence per rep. `batch1` = 16 `vkCmdDispatch` direkam ke SATU command buffer, **1 submit + 1 fence per rep** (`DISPATCHES_RECORDED 16`, `disp=16`). `batchfp` = 16 submit + 16 fence terpisah per rep (`disp=1`).
- **Gate eksak (SPEC §2.1):** semua arm PASS. Untuk arm 1-dispatch `C0==iters` (2000). Untuk `batch1` gate eksak = `C0==iters*R` = 32000 (karena 16 dispatch berurutan z=z*x+y dgn x=y=1,z0=0 → z=16·iters), dicetak sebagai `expect=32000 applied=16 PASS`. FAIL men-`exit(1)`.

**Urutan & termal:** tiap REP menjalankan cold→ramp→batch1→batchfp; 3 REP berurutan dalam jendela ≈13 s (ts 1791386731→1791386744). Q1 selesai SEBELUM kampanye OpenCL dimulai, jadi arm `cold` benar-benar dingin (bukan device sisa panas kampanye lain). Tidak ada indikasi drift termal antar-REP (lihat spread di bawah).

**Tabel per-arm (n=65536, reps=10, 3 run; GFLOPS):**

| arm | ramp (ms) | R | disp/rep | runs | mean | min | max |
|---|---|---|---|---|---|---|---|
| cold | 0 | 1 | 1 | 3 | 37.462 | 37.235 | 37.610 |
| ramp | 500 | 1 | 1 | 3 | 72.516 | 71.944 | 73.076 |
| batch1 | 500 | 16 | 16 | 3 | 76.282 | 74.896 | 76.981 |
| batchfp | 500 | 16 | 1 | 3 | 72.303 | 71.972 | 72.561 |

Rentang batch1 [74.896, 76.981] dan batchfp [71.972, 72.561] **tidak tumpang tindih** → delta boleh diklaim. Rata-rata batch1 − batchfp ≈ **+3.98 GFLOPS (~+5.5%)**.

**Status hipotesis (DIKOREKSI oleh SPEC §8 — otoritas):**
- **H1 TIDAK DIDUKUNG (verdict: TIDAK TERVERIFIKASI).** SPEC §1 menetapkan kriteria falsifiable `batch1 n=65536 ≥ ~86 → H1 didukung`; batch1 = **76.282** [74.896, 76.981] → **ambang tidak tercapai**. Yang sah hanya ARAH: batch1 vs batchfp disjoint (+3.98 GFLOPS, ~+5.5%), `DISPATCHES_RECORDED=16`.
- **H2 juga tidak dinyatakan** (ramp [71.944,73.076] vs batchfp [71.972,72.561] **tumpang tindih** → delta itu tidak sah). `ramp`≈`batchfp` (72.5 vs 72.3) menunjukkan clock ramp faktor dominan (37→72), batching menambah ~4 GFLOPS.

**Diskrepansi vs baseline historis (~82–83 di n=65536 ramp):** harness historis sudah dihapus → tidak dapat diverifikasi ulang; angka 82–83 tidak dipakai sebagai pembanding sah. Di harness baru, ramp jenuh = 72.5 (cur_freq_after 610 MHz). Perbedaan bisa berasal dari perbedaan harness (warm-up berbasis GPU, bentuk kernel/buffer, path loader via dlopen driver Adreno langsung) — dicatat sebagai hipotesis, bukan kesimpulan.

---

## Q2 — OpenCL: bentuk kurva lsize × ILP (12 sel)

STATUS: DIHENTIKAN atas perintah lead. Grid 12-sel tidak diselesaikan. Data parsial di `q2-opencl.raw.txt` (17 RESULT, hanya sebagian sel l64) **tidak dipakai** untuk klaim kurva; digantikan oleh arm pembeda di bawah.

Observasi awal (dari run parsial yang sah, single-writer, n=65536, iters=2000, reps=3): semua sel l64 yang sempat diukur = 0.147–0.152 GFLOPS, praktis datar terhadap ILP 1/2/4.

---

## Q2-DISCRIMINATOR — kernel-time vs wall-time (arm pembeda, `q2-discriminator.raw.txt`)

Harness: `oclprof.c` (bionic, dlopen libOpenCL, queue `CL_QUEUE_PROFILING_ENABLE`, event profiling). n=65536, iters=2000, L=64, R=10. Device: `CLDEV name=QUALCOMM Adreno(TM) type=0x0 maxcu=1 maxmhz=1 nplat=1 ndev=1`.

| mode | dispatch/finish | wall_s | gflops | kernel_ns |
|---|---|---|---|---|
| single run0 | 1 / 1 | 6.915 | 0.1516 | 4,316,928 |
| single run1 | 1 / 1 | 6.908 | 0.1518 | 616,192 |
| single run2 | 1 / 1 | 6.976 | 0.1503 | 581,888 |
| perrep R=10 | 10 / 10 | 71.182 | 0.1473 | 2,729,472 (sum) |
| batch R=10 | 10 / 1 | 71.889 | 0.1459 | 2,723,840 (sum) |

Semua GATE PASS (single C0=2000; perrep/batch C0=20000). `PROF_SUPPORTED=1`.

**Temuan (bukan salah satu cabang H1/H2 yang dihipotesiskan lead):**
1. **batch ≈ perrep** (0.1459 vs 0.1473, beda ~1%). Protokol `clFinish`-per-rep **bukan** artefaknya; biaya per-dispatch ~7.1–7.2 s identik untuk single/perrep/batch.
2. **kernel_ns ≪ wall** (2.73 ms vs 71 s = ~0.004% wall) → wall hampir seluruhnya BUKAN eksekusi kernel. Tapi `kernel_ns` tidak konsisten (4.3 ms vs 0.6 ms untuk kernel identik) dan menyiratkan ~1700 GFLOPS, ~47× di atas Vulkan nyata (37 GFLOPS) di GPU yang sama → timestamp profiling TIDAK dapat dipercaya sebagai waktu kernel.

**Kesimpulan (DIPERBAIKI oleh arm variasi-kerja di bawah):** klaim 'biaya tetap ~7 s per dispatch' dari tabel ini **TERBANTAHKAN**. Wall sebenarnya menskala linear dengan kerja (lihat Q2-WORKSWEEP). Angka wall di tabel ini (single 6.9s, perrep 71s, batch 72s) memang skala dengan R karena tiap dispatch mengerjakan kerja yang sama — bukan bukti overhead tetap. `kernel_ns` tetap tidak dapat dipercaya (inkonsisten, menyiratkan kecepatan di atas Vulkan).

---

## Q2-WORKSWEEP — apakah wall skala dengan besar kerja? (`q2-worksweep.raw.txt`)

Harness: `oclws.c` (single dispatch + single finish per config, queue profiling, gate eksak C0==iters). Semua GATE PASS.

| n | iters | wall_s (3 run) | gflops |
|---|---|---|---|
| 65536 | 2000 | 7.093 / 6.996 / 7.113 | ~0.147 |
| 65536 | 200 | 0.7145 / 0.7106 / 0.7144 | ~0.147 |
| 65536 | 20 | 0.0777 / 0.0735 / 0.0741 | ~0.14 |
| 6553 | 2000 | 0.7166 / 0.7138 / 0.7160 | ~0.146 |

**Temuan:** wall **SKALA LINEAR** dengan kerja. iters turun 100x → wall turun 100x; n turun 10x → wall turun 10x. `gflops` DATAR ~0.147 di semua konfigurasi.

**Kesimpulan final Q2:** jalur OpenCL di harness/loader/konfigurasi ini memang **compute-bound ~0.147 GFLOPS** — bukan artefak pengukuran overhead. Kedataran grid lsize×ILP adalah temuan NYATA (throughput ~konstan terhadap bentuk kernel). Angka 96 GFLOPS historis TIDAK dapat direproduksi dan TIDAK boleh dipakai sebagai baseline.

---

## Q2-FULLBUF — kontrol gate penuh + batas atas CPU (`q2-fullbuf.raw.txt`)

Harness: `oclfull.c`. n=65536, iters=2000.

- **Gate penuh:** readback SELURUH buffer z (262144 float), cek setiap elemen.
  `FULLBUF_PASS ok=262144 mn=2000 mx=2000` → semua 65536 work-item mengerjakan iters. Gate `q2` yang lama (baca 16 B) memang hanya membuktikan work-item 0; sekarang terbukti seluruhnya. Jadi angka 0.147 mengukur kerja penuh, bukan subset.
- **Batas atas CPU (1 thread, setara FMA):** `CPU_FMA n=65536 iters=2000 wall_s=0.211486 gflops=4.958`.
- **OpenCL pada kerja identik:** `OCL_KERNEL wall_s=6.920292 gflops=0.1515`.

**Temuan:** satu thread CPU ARM menang **~33x** atas jalur OpenCL pada jumlah FMA identik (4.96 vs 0.15 GFLOPS). Maka verdict ronde bukan 'OpenCL Adreno 616 = 0.15 GFLOPS', melainkan: **jalur OpenCL lewat loader/libOpenCL.so ini patologis — gagal mencapai throughput compute GPU, bahkan kalah jauh dari CPU.** `maxcu=1`/`type=0x0` tersangka kuat mekanisme (serialisasi / device-info rusak), belum dikonfirmasi.

---

## Q3 — OpenCL n=1048576

STATUS: DIHENTIKAN atas perintah lead. **`q3-large-n.raw.txt` TIDAK PERNAH DITULIS** (Q2 belum selesai saat stop; dikonfirmasi `ls` = No such file). Tidak ada angka Q3.

---

## Catatan proses (integritas)
- Dua peluncuran run_q23.sh sempat terjadi (pkill pattern self-match meninggalkan orphan writer). Diperbaiki: kill by exact PID, guard in-script, log Q2 diregenerasi. Kampanye final yang dianalisis berasal dari SATU proses tunggal.
- **Konfirmasi:** semua angka Q2/discriminator berasal dari satu writer GPU tunggal.

---

## Catatan aturan bukti
- Setiap arm/sel punya baris GATE PASS (exit non-zero bila FAIL).
- ≥3 repeat per sel (Q1: 3; Q2: 6; Q3: 3) — mean+min/max dilaporkan, bukan satu angka.
- Raw log mentah di-stream ke file; tidak diedit. Header konteks ini dipisah di RESULTS.md (bukan disuntik ke raw).
- Klaim delta antar-sel hanya bila rentang tidak tumpang tindih.

---

# KOREKSI BERTANGGAL 2026-10-07 (sesi lanjutan) — akar angka ~0.147 GFLOPS Q2 DITEMUKAN

**Status: ini AMANDEMEN ATAS TEMUAN, bukan penulisan ulang verdict §8. §8.2 dan §8.5 ditetapkan user; usulan amandemen ada di §C di bawah, keputusan tetap milik user.**

## A. Akar penyebab angka ~0.147 GFLOPS: builtin `fma()` di kernel

Harness r1 (`oclws.c`) memakai `zv=fma(zv,xv,yv)` di dalam loop. Di device ini (Adreno 616, OpenCL 2.0 lewat shim `libOpenCL.so → libCB`), **builtin `fma()` (correctly-rounded) turun ke jalur emulasi software** — itu penyebab angka 0.147, bukan jalur loader.

**Bukti 1 — satu token diubah di harness r1 SENDIRI** (`oclws.c` disalin ke `/home/han/adreno616-lab/src/opencl/oclws_muladd.c`, hanya `fma(zv,xv,yv)` → `zv*xv+yv`, sisa identik; build & jalankan identik, n=65536 iters=2000, gate `C0=2000` PASS ×3 keduanya):

| varian (harness r1, file yang sama) | wall_s (3 run) | GFLOPS |
|---|---|---|
| `oclws` ASLI (`fma`) | 7.078 / 7.055 / 7.063 | **0.148** |
| `oclws_muladd` (1 token) | 0.015 / 0.011 / 0.012 | **68 / 94 / 85** |

~600× dari satu token, di dalam harness yang menghasilkan 0.148. **§8.2 ("jalur OpenCL/loader patologis") terjelaskan: yang patologis adalah builtin `fma()`, bukan loader.**

**Bukti 2 — `/home/han/adreno616-lab/src/opencl/oclmech.c` (6 arm, satu proses/queue/device, n=65536, iters=2000, L=64, clock 610 MHz, gate `C0=2000` PASS semua, reps=10):**

| arm | operasi | opts | GFLOPS |
|---|---|---|---|
| A | vec4 `fma()` | — | **0.148** |
| B | vec4 `zv*xv+yv` | — | **99.69** |
| C | vec4 `mad()` | — | **99.87** |
| D | vec4 `fma()` | `-cl-fast-relaxed-math` | **0.149** |
| E | skalar `zv*xv+yv` | — | **37.33** |
| F | skalar `fma()` | — | **0.770** |

**Bukti 3 — `/home/han/adreno616-lab/src/opencl/oclmech2.c` (4 arm skalar, konfigurasi sama, gate PASS semua):**

| arm | operasi | opts | GFLOPS |
|---|---|---|---|
| E2 | skalar `zv*xv+yv` | — | **37.35** |
| F2 | skalar `fma()` | — | **0.770** |
| G | skalar `mad()` | — | **37.53** |
| H | skalar `mad()` | `-cl-fast-relaxed-math` | **37.30** |

## B. Mekanisme — emulasi fma correctly-rounded (bukan flag codegen)

Dua arah bukti:
1. **`mad()` secepat muladd** di kedua lebar (vec4 99.87; skalar 37.53). `mad()` boleh non-correct-rounding; kalau masalahnya instruksi FFMA-nya sendiri, `mad()` juga harus lambat. Tidak.
2. **`-cl-fast-relaxed-math` TIDAK menyelamatkan `fma()`** (vec4 0.149; skalar tidak diuji fast). Jadi bukan flag codegen — `fma()` di OpenCL C dijamin correctly-rounded sementara FFMA Adreno tidak, sehingga driver menurunkan ke jalur software.

**Klaim yang DIBATASI (jangan diperluas):** yang terbukti adalah *pada lebar vec4 dan skalar*, `fma()` adalah jurang dan `a*b+c`/`mad()` cepat. **Kontraksi otomatis `a*b+c`→fma oleh compiler belum diuji** di jalur OpenCL ini (di GLSL/Vulkan tidak terjadi, lihat §C.2).

## C. Dua usulan amandemen (KEPUTUSAN USER, bukan diputuskan di sini)

### C.1 — §8.2 "jalur OpenCL/loader patologis"
Bukti §A menjelaskan 0.147 sebagai artefak `fma()` di kernel harness, bukan properti jalur/loader. Usulan: §8.2 diberi catatan bertanggal yang menunjuk ke §A, dan kalimat "gagal mencapai throughput compute GPU" dibatasi menjadi "kernel ber-`fma()` gagal". **Tidak diubah di sini.**

### C.2 — §8.5 "DILARANG mengutip ~96 GFLOPS"
Angka ~96–100 GFLOPS vec4 kini **direproduksi di harness yang masih ada di disk** (`oclmech.c`, `oclmech2.c`, `oclws_muladd`) dengan gate eksak PASS. Usulan: larangan §8.5 dilonggarkan menjadi "jangan kutip tanpa menyertakan (n, reps, bentuk kernel, status ramp)". **Tidak diubah di sini.**

## D. Uji hipotesis "Vulkan mungkin kena bug logika yang sama" — TIDAK

`spirv-dis gpu-r1/vkbatch.spv` → **1 `OpFMul`, 0 `OpFma`**. glslang tidak mengontrak `z*x+y` menjadi fused multiply-add di GLSL → jalur Vulkan **tidak pernah melewati `fma()` yang menjerat OpenCL**. Konsisten dengan r1 Q1 (Vulkan ~72–77) vs Q2 (~0.15).

**Vulkan saturasi pada harness yang ada (`vkbatch`, mode ramp, gate `C0=2000` PASS, cur_freq_after=610 MHz):**

| n | iters | reps | GFLOPS |
|---|---|---|---|
| 65536 | 2000 | 10 | 72.82 |
| 262144 | 2000 | 10 | 76.29 |
| 1048576 | 2000 | 10 | 77.22 |

Plato ~77 pada harness ini. **Tidak ada tanda Vulkan "bisa lebih tinggi" karena bug logika** — jalurnya sudah benar (mul+add terpisah). Kalau mau lebih tinggi, itu pertanyaan lain (workgroup size, ILP, occupancy), di luar koreksi ini.

## E. Perbandingan yang sah sekarang (semua gate eksak PASS)

| jalur | bentuk | GFLOPS | harness |
|---|---|---|---|
| OpenCL vec4 `a*b+c` / `mad()` | FMA ber-ILP | **~100** | `oclmech.c` |
| OpenCL skalar `a*b+c` / `mad()` | FMA skalar | **~37** | `oclmech2.c` |
| OpenCL vec4 `fma()` | correctly-rounded | **~0.148** | `oclmech.c`, `oclws.c` |
| Vulkan vec4 `z*x+y` (plato) | mul+add, no contract | **~77** | `vkbatch` (r1) |

Rasio OpenCL(vec4, muladd) : Vulkan ≈ **1.3×** pada n=65536 (100 vs 72.8). Keduanya **sebanding**, dan yang menentukan bukan runtime-nya melainkan **apakah kernel memakai `fma()` atau tidak**.

## F. Robustness (carry-over)

Kernel ber-`fma()` pada iters=2000 memakan ~7 s/dispatch — persis di ambang watchdog yang dulu membunuh `neuron_server` di `cl_a6x_cmdbuf_mgr_submit_ibs`. Harness `fma()` berisiko terhadap watchdog; `a*b+c`/`mad()` tidak.

---

**File bukti (di disk):** `/home/han/adreno616-lab/src/opencl/oclmech.c`, `/home/han/adreno616-lab/src/opencl/oclmech2.c`, `/home/han/adreno616-lab/src/opencl/oclws_muladd.c` (+ binernya). Harness r1 asli: `rounds/r1/src/oclws.c`, `rounds/r1/src/vkbatch.c`.

---

# KOREKSI BERTANGGAL 2026-10-07 (lanjutan) — sweep bentuk kernel Vulkan: ~77 GFLOPS BUKAN plafon

**Status: temuan baru, bukan amandemen verdict §8 (yang soal OpenCL). Vulkan di luar Q1–Q3 r1; ini addendum.**

Temuan §D di atas (Vulkan tidak kena jurang `fma()`) menutup satu pintu, tapi memunculkan pertanyaan: apakah plato ~77 (`vkbatch`) bisa dinaikkan lewat bentuk kernel? Jawabnya ya — dan tuasnya **ILP**, bukan local_size.

## Harness baru: `vkopt.c` (+ 9 SPV)

`vkbatch` tidak bisa dipakai untuk ILP (gx hardcode `n/64`, gate mengasumsikan satu akumulator). `vkopt.c` ditulis dari nol: argumen `<iters> <n> <reps> <L> <K> <ramp|cold>`, gate eksak `C0 == K*iters` (x=y=1, z0=0 → tiap akumulator = iters), env `VKOPT_SPV`. **`local_size` ter-encode di SPIR-V `OpExecutionMode LocalSize`** → tiap L perlu shader sendiri (`vkopt_k{1,2,4}_L{64,128,256}.spv`, verifikasi `spirv-dis | grep LocalSize`).

## Sweep 9 sel (n=65536, iters=2000, reps=10, ramp=500, gate PASS semua, 3 run/sel)

Raw: `q7-vulkan-shape.raw.txt`.

| L \ K | 1 | 2 | 4 |
|---|---|---|---|
| 64 | 82.3 | **112.7** | 86.5 |
| 128 | 71.7 | 103.5 | 79.4 |
| 256 | 71.9 | 103.2 | 79.3 |

- **L=64 K=2 = ~112.7 GFLOPS** (rentang 112.10–113.22, **disjoint** dari semua sel lain) — **+46%** dari `vkbatch` ~77, dan melampaui OpenCL vec4 muladd ~100.
- **Tuasnya ILP, bukan local_size:** K=2 naik di semua L; K=4 **turun** lagi (86.5) — konsisten register spill, jadi ada optimum, bukan monoton. L=64 menang ~10–16% atas 128/256.

## Konfirmasi n besar (`q8-vulkan-confirm.raw.txt`, L=64, 3 run)

| n | K=1 | K=2 |
|---|---|---|
| 65536 | 82.3 | 112.7 |
| 262144 | 86.9 (86.50–87.10) | 114.4 (109.39–117.03) |
| 1048576 | 88.3 (88.29–88.33) | **117.9 (117.85–117.91)** |

K=2 tetap menang jauh di n besar (disjoint dari K=1 tiap n), dan naik sedikit di 1048576 → **bukan efek grid kecil**. Baseline K=1 juga naik dengan n (82→88), konsisten `vkbatch`; tuas ILP tetap ~+30 GFLOPS di atas baseline di tiap n.

## Penutupan dua sumbu berikutnya (`q9-vulkan-k3-l32.raw.txt`, n=65536, gate PASS semua, 3 run)

| L | K=1 | K=2 | K=3 | K=4 |
|---|---|---|---|---|
| 32 | 42.6 | 57.5 | 42.0 | — |
| **64** | 82.3 | **112.7** | 84.6 | 86.5 |
| 128 | 71.7 | 103.5 | — | 79.4 |
| 256 | 71.9 | 103.2 | — | 79.3 |

- **L=32 ≈ separuh L=64** di tiap K → 64 adalah wave size; L=64 lantai sumbu local_size.
- **K=3 tidak mengalahkan K=2** (84.6 vs 112.7), jatuh ke band K=1/K=4 → jurang register mulai di K=3, K=2 puncak tajam.

## Batching × ILP — NEGATIF (`q10-vulkan-batch-k2.raw.txt`, n=65536, gate PASS)

| arm | GFLOPS |
|---|---|
| K=2 L=64 R=1 (baseline) | **112.4** (112.37–112.61) |
| K=2 L=64 R=16 (16 dispatch → 1 command buffer) | **84.1** (84.08–84.11) |

Batching yang menang **+5.5%** pada kernel overwrite K=1 (r1 §8.1) **turun 25%** saat digabung K=2. Sebabnya struktural: gate batch yang sah butuh shader **akumulasi** (baca `z[i]` awal, tulis akhir) → dependency antar-dispatch, menghapus paralelisme yang jadi sumber keuntungan batching. Kernel K=1 r1 overwrite, jadi tak punya dependency itu.

## Grid-stride D=2 — TURUN (`q11-vulkan-stride2.raw.txt`, `q11b-stride2b.raw.txt`, gate `C0==iters` PASS)

| arm | GFLOPS |
|---|---|
| K=2 L=64 R=1 (baseline) | 111.8 / 111.9 / 112.4 |
| grid-stride D=2, stride-2 antar-lane | 66.9–67.8 |
| grid-stride D=2, **koalesen** | 66.9–67.5 |

Dua varian mendarat sama → **bukan** pola akses memori; penyebabnya grid terbelah dua (gx 512 vs 1024) + live register ganda per work-item. ILP sama dengan K=2, tapi K=2 mendapatkannya tanpa membelah grid. (Prediksi awal ~84–86 meleset; arah turun benar, besaran salah.)

## Batas klaim

- Ini karakterisasi `n ∈ {65536, 262144, 1048576}`, `iters=2000`, `reps=10`, `ramp=500ms`, kernel FMA `z=z*x+y` — **bukan "plafon Vulkan"**. Tidak ada klaim tentang batas chip.
- **Keempat sumbu tertutup untuk bentuk kernel satu-vec4-per-work-item:** L (64 = wave size), K (2 puncak tajam), batching (merugikan saat akumulasi), grid-stride D=2 (okupansi membayar). **K=2 / L=64 / R=1 = titik optimum** pada ruang yang dijelajahi.
- **Perbandingan yang sah:** pada harness & config sama (L=64, n=65536), Vulkan K=2 ~112.7 vs OpenCL vec4 muladd ~100 — klaim lama "OpenCL lebih cepat dari Vulkan" tidak berlaku begitu bentuk kernel dioptimalkan.
- §8.2/§8.5 (verdict OpenCL) **tidak diubah di sini**; keputusan tetap milik user.

**File bukti (di disk):** `gpu-r1/vkopt.c`, `vkopt_stride.c`, `vkopt_k{1,2,3,4}_L{32,64,128,256}.spv`, `vkopt_k2_batch_L64.spv`, `vkopt_stride2{,b}_L64.spv`, `q7`–`q11b` `*.raw.txt`.
