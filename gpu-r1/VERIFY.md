# VERIFY.md — Verdict verifier independen (log-verifier)

Tanggal: 2026-10-07. Verifikasi dilakukan **hanya terhadap raw log mentah**, bukan terhadap RESULTS.md, REVIEW.md, atau ringkasan siapa pun.

Sumber yang dibaca:
- `q1-vulkan.raw.txt` (Vulkan: cold / ramp / batch1 / batchfp, 3 rep)
- `q2-worksweep.raw.txt` (OpenCL: sweep iters & n)
- `q2-discriminator.raw.txt` (OpenCL: single / perrep / batch)
- `q2-opencl.raw.txt` (OpenCL grid lsize×ILP — **parsial**)
- `q3-large-n.raw.txt` — **TIDAK ADA** (tidak pernah ditulis)

Catatan konvensi aritmetika (dibuktikan dari reproduksi angka, lihat bawah):
- **Q1 Vulkan:** `flops = n*4*iters*2*reps*R`, `gflops = flops/wall_s/1e9`. (reps=10 di semua arm; R=1 untuk cold/ramp, R=16 untuk batch1/batchfp.)
- **Q2/worksweep OpenCL:** `flops = n*4*iters*2`, `gflops = flops/wall_s/1e9` (tanpa faktor reps).

---

## 1. Aritmetika — hitung ulang tiap baris RESULT

### Q1 Vulkan (flops = n*4*iters*2*reps*R)

| arm | rep | n | iters | reps | R | disp | wall_s (raw) | gflops raw | gflops hitung ulang | Δ |
|---|---|---|---|---|---|---|---|---|---|---|
| cold | 1 | 65536 | 2000 | 10 | 1 | 1 | 0.281614 | 37.235 | 37.234 | 0.003% |
| cold | 2 | 65536 | 2000 | 10 | 1 | 1 | 0.278805 | 37.610 | 37.610 | ~0% |
| cold | 3 | 65536 | 2000 | 10 | 1 | 1 | 0.279325 | 37.540 | 37.540 | ~0% |
| ramp | 1 | 65536 | 2000 | 10 | 1 | 1 | 0.145748 | 71.944 | 71.944 | ~0% |
| ramp | 2 | 65536 | 2000 | 10 | 1 | 1 | 0.144576 | 72.528 | 72.529 | ~0% |
| ramp | 3 | 65536 | 2000 | 10 | 1 | 1 | 0.143491 | 73.076 | 73.076 | ~0% |
| batch1 | 1 | 65536 | 2000 | 10 | 16 | 16 | 2.179410 | 76.981 | 76.981 | ~0% |
| batch1 | 2 | 65536 | 2000 | 10 | 16 | 16 | 2.240062 | 74.896 | 74.896 | ~0% |
| batch1 | 3 | 65536 | 2000 | 10 | 16 | 16 | 2.179757 | 76.968 | 76.969 | ~0% |
| batchfp | 1 | 65536 | 2000 | 10 | 16 | 1 | 2.331062 | 71.972 | 71.972 | ~0% |
| batchfp | 2 | 65536 | 2000 | 10 | 16 | 1 | 2.312163 | 72.561 | 72.562 | ~0% |
| batchfp | 3 | 65536 | 2000 | 10 | 16 | 1 | 2.318106 | 72.375 | 72.375 | ~0% |

**Semua dalam < 0.01%. Tidak ada selisih > 1%.**

### Q2 worksweep (flops = n*4*iters*2)

| cfg (n, iters) | wall_s raw (3 rep) | gflops raw | hitung ulang | Δ |
|---|---|---|---|---|
| 65536, 2000 | 7.092506 / 6.995608 / 7.113152 | 0.14784 / 0.14989 / 0.14741 | 0.14784 / 0.14989 / 0.14741 | ~0% |
| 65536, 200 | 0.714523 / 0.710553 / 0.714415 | 0.14675 / 0.14757 / 0.14677 | 0.14675 / 0.14757 / 0.14677 | ~0% |
| 65536, 20 | 0.077749 / 0.073535 / 0.074109 | 0.13487 / 0.14260 / 0.14149 | 0.13487 / 0.14259 / 0.14148 | ~0% |
| 6553, 2000 | 0.716628 / 0.713811 / 0.715957 | 0.14631 / 0.14688 / 0.14644 | 0.14630 / 0.14687 / 0.14644 | ~0% |

### Q2 discriminator (flops = n*4*iters*2, R=10 pada perrep/batch)

| mode | wall_s raw | gflops raw | hitung ulang | Δ |
|---|---|---|---|---|
| single run0 | 6.914858 | 0.1516 | 0.15164 | ~0% |
| single run1 | 6.907863 | 0.1518 | 0.15180 | ~0% |
| single run2 | 6.975702 | 0.1503 | 0.15034 | ~0% |
| perrep R=10 | 71.182211 | 0.1473 | 0.14731 | ~0% |
| batch R=10 | 71.888909 | 0.1459 | 0.14587 | ~0% |

### Q2 opencl grid (flops = n*4*iters*2)

| cell | wall_s raw | gflops raw | hitung ulang | Δ |
|---|---|---|---|---|
| l64a1 | 6.969291 / 6.903131 / 6.922866 | 0.150 / 0.152 / 0.151 | 0.15045 / 0.15190 / 0.15146 | ~0% |
| l64a2 | 7.070965 / 7.083051 / 7.052129 | 0.148 / 0.148 / 0.149 | 0.14830 / 0.14805 / 0.14869 | ~0% |
| l64a4 | 7.142070 / 7.113881 / 6.984141 | 0.147 / 0.147 / 0.150 | 0.14682 / 0.14740 / 0.15015 | ~0% |
| l64a8 | 7.050136 / 7.099623 / 7.119161 | 0.149 / 0.148 / 0.147 | 0.14873 / 0.14770 / 0.14729 | ~0% |
| l128a1 | 11.691300 / 11.598921 / 11.526239 | 0.090 / 0.090 / 0.091 | 0.08969 / 0.09040 / 0.09095 | ~0% |
| l128a2 | 11.956781 / 11.975820 | 0.088 / 0.088 | 0.08770 / 0.08756 | ~0% |

**Kesimpulan §1: seluruh baris RESULT di keempat file direproduksi < 0.01%. Tidak ada pelanggaran ambang 1%.**

---

## 2. Gate

Semua baris run di keempat file punya `GATE ... PASS`. **Tidak ada satu pun FAIL.**

- Q1 cold/ramp: `GATE C0=2000 expect=2000 applied=1 PASS` ✓ (3 rep masing-masing)
- Q1 batch1: `GATE C0=32000 expect=32000 applied=16 PASS` ✓ (C0 = iters*R = 2000*16 = 32000) — dan **`DISPATCHES_RECORDED 16` tercetak** di ketiga rep.
- Q1 batchfp: `GATE C0=2000 expect=2000 applied=1 PASS` ✓
- Worksweep: `GATE run=0/1/2 C0=iters expect=iters PASS` ✓ (iters 2000/200/20)
- Discriminator: single C0=2000; perrep & batch `C0=20000 expect=20000 PASS` ✓
- opencl grid: tiap sel `GATE cell=... C0=2000 expect=2000 PASS` ✓

**Karena tidak ada FAIL, seluruh sel yang terukur sah secara gate.**

---

## 3. Q1 — mean / min / max per arm (dari raw) + disjointness

| arm | gflops (3 rep) | mean | min | max |
|---|---|---|---|---|
| cold | 37.235, 37.610, 37.540 | **37.462** | 37.235 | 37.610 |
| ramp | 71.944, 72.528, 73.076 | **72.516** | 71.944 | 73.076 |
| batch1 | 76.981, 74.896, 76.968 | **76.282** | 74.896 | 76.981 |
| batchfp | 71.972, 72.561, 72.375 | **72.303** | 71.972 | 72.561 |

Cek disjointness rentang (min–max):
- cold [37.235, 37.610] vs ramp [71.944, 73.076] → **disjoint** ✓
- ramp [71.944, 73.076] vs batch1 [74.896, 76.981] → **disjoint** ✓ (batch1 lebih tinggi)
- **batch1 [74.896, 76.981] vs batchfp [71.972, 72.561] → DISJOINT** ✓ — klaim leader BENAR.
- ramp [71.944, 73.076] vs batchfp [71.972, 72.561] → **TUMPANG TINDIH** (irisan 71.972–72.561). batchfp ≈ ramp; klaim delta ramp-vs-batchfp TIDAK sah.

**Verifikasi legalitas arm batch1:** `DISPATCHES_RECORDED 16` tercetak di ketiga rep → command buffer di-record ulang dengan 16 `vkCmdDispatch` (bukan resubmit cb 1-dispatch). Gate `applied=16`, `C0=32000`. **Arm batch1 legal.**

### Verdict Q1
Fakta terukur: batching 16 dispatch → 1 submit/1 fence memberi kenaikan nyata dan disjoint terhadap fence-per-rep (76.282 vs 72.303, +5.5%), sedangkan fence-per-rep ≈ ramp single (72.303 ≈ 72.516, tumpang tindih). Ini menunjukkan overhead submit+fence memang ada dan teramortisasi saat batching.

Namun tes pembeda SPEC memerlukan arm batch n=65536 mencapai **≥ ~86** (setara sel n=1048576 "88"). batch1 mentok di **max 76.981 / mean 76.282 < 86** → ambang H1 TIDAK tercapai. Sebaliknya kriteria H2 ("tetap ~83") juga tidak cocok, karena baseline ramp harness BARU ini = 72.5, bukan 83. Sel n=1048576 (Vulkan) **tidak ada di raw ronde ini**, jadi perbandingan langsung 83→88 vs batch tidak dapat dilakukan.

**Q1 = TIDAK TERVERIFIKASI.** Arah bukti konsisten dengan H1 (batching amortizes submit/fence, disjoint), tetapi ambang keputusan H1 tidak tercapai dan sel rujukan n-besar tidak terukur — H1 tidak dapat dinyatakan didukung, H2 juga tidak. (Bukan FAIL; kedua arm bergate PASS.)

---

## 4. Q2 — worksweep (paling penting)

Data raw (3 rep per cfg):

| cfg | wall mean (s) | gflops mean | gflops min/max |
|---|---|---|---|
| n=65536, iters=2000 | 7.0671 | 0.14838 | 0.14741 / 0.14989 |
| n=65536, iters=200 | 0.7132 | 0.14703 | 0.14675 / 0.14757 |
| n=65536, iters=20 | 0.07513 | 0.13965 | 0.13487 / 0.14260 |
| n=6553, iters=2000 | 0.71547 | 0.14654 | 0.14631 / 0.14688 |

**Wall skala dengan kerja (dikonfirmasi):**
- iters 2000→200 (10×): 7.0671 → 0.7132 = **9.91×** (harap 10×, selisih 0.9%)
- iters 2000→20 (100×): 7.0671 → 0.07513 = **94.1×** (harap 100×, selisih 5.9%)
- n 65536→6553 (10.001×): 7.0671 → 0.71547 = **9.88×** (selisih 1.2%)

Jadi wall **≈ linear terhadap kerja** pada reduksi 10× (dalam ~1%); pada sel terkecil (iters=20) sedikit sub-linear (wall 6% lebih tinggi dari proporsional) dan gflops turun ke ~0.140.

**gflops datar:** 0.1484 / 0.1470 / 0.1397 / 0.1465 → datar di sekitar **~0.147**, dengan pengecualian sel iters=20 (~0.140, ~5% di bawah). Klaim "~0.147 datar" benar kecuali di sel kerja terkecil.

**Membantah "biaya tetap 7 s/dispatch":** BENAR. Jika ada biaya tetap ~7 s per dispatch, wall di iters=20 (yang hanya 1/100 kerja) tetap ~7 s. Kenyataannya wall = 0.0751 s (≈ 1/94 dari 7.07 s). Tidak ada biaya tetap 7 s.

### Verdict worksweep
**TERVERIFIKASI.** Angka persis sesuai raw: wall ~7.07 → ~0.713 → ~0.0751 s (iters 2000→200→20), ~7.07 → ~0.7155 s (n 65536→6553), gflops ~0.147 datar. Klaim runner sebelumnya "biaya tetap 7 s/dispatch" **TERBANTAH**.

---

## 5. Q2 — discriminator & grid lsize×ILP

### Discriminator (n=65536, iters=2000)
- single (R=1): 0.1516 / 0.1518 / 0.1503 → mean **0.1512**, [0.1503, 0.1518]
- perrep (R=10, 10 submit+fence): **0.1473**
- batch (R=10, 1 submit+fence): **0.1459**

`CLDEV name=QUALCOMM Adreno(TM) type=0x0 maxcu=1 maxmhz=1 nplat=1 ndev=1`

### Grid lsize×ILP (parsial — hanya lsize 64 penuh + lsize 128 sebagian)
| cell (L,K) | gflops raw | mean | range |
|---|---|---|---|
| l64a1 (64,1) | 0.150, 0.152, 0.151 | **0.1510** | [0.150, 0.152] |
| l64a2 (64,2) | 0.148, 0.148, 0.149 | 0.1483 | [0.148, 0.149] |
| l64a4 (64,4) | 0.147, 0.147, 0.150 | 0.1480 | [0.147, 0.150] |
| l64a8 (64,8) | 0.149, 0.148, 0.147 | 0.1480 | [0.147, 0.149] |
| l128a1 (128,1) | 0.090, 0.090, 0.091 | **0.0903** | [0.090, 0.091] |
| l128a2 (128,2) | 0.088, 0.088 (2 run) | 0.0880 | [0.088, 0.088] |

Yang terukur:
- **Efek ILP (K) pada lsize=64: NEGLIGIBLE.** l64a1/a2/a4/a8 = 0.151/0.148/0.148/0.148; rentang saling tumpang tindih (l64a1 [0.150,0.152] vs l64a4 [0.147,0.150] beririsan di 0.150). Argmax terukur = **l64a1 (0.1510)**, tetapi lemah — hanya disjoint terhadap a2 dan a8, tidak terhadap a4.
- **Efek lsize: DOMINAN.** lsize 64→128 menjatuhkan throughput ~0.148 → ~0.090 (≈ -40%), **rentang disjoint** terhadap semua sel lsize=64.
- **Titik belok** = transisi **lsize 64→128**.
- **Grid TIDAK lengkap:** lsize=256 tidak diukur sama sekali; lsize=128 hanya a1 (3 run) & a2 (2 run), a4/a8 absen. Jadi 12-sel tidak terpenuhi.

### Verdict Q2
- **Bentuk kurva penuh + titik belok + argmax: TIDAK TERVERIFIKASI** (grid ~50% lengkap; lsize=256 absen; l128 tak lengkap).
- Yang terverifikasi dari data yang ada: **efek lsize besar (64→128, ~-40%, disjoint)**; **efek ILP (K=1..8 pada lsize=64) tidak signifikan/overlap**; **argmax terukur = l64a1** (lemah).

---

## 6. Q3 — OpenCL n=1048576

`q3-large-n.raw.txt` **tidak pernah ditulis** (tidak ada di direktori). Tidak ada satu baris pun untuk diverifikasi.

**Verdict Q3 = TIDAK TERVERIFIKASI** (bukan FAIL — pengukuran dihentikan, bukan gagal gate).

---

## 7. Fakta yang dicatat (bukan penyebab terbukti)

- `CLDEV ... type=0x0 maxcu=1 maxmhz=1` — tercetak di discriminator dan grid. `type=0x0` dan `maxcu=1`/`maxmhz=1` adalah nilai mencurigakan (tersangka device-info rusak pada jalur OpenCL ini). **Ini fakta yang tercetak, BUKAN penyebab yang terbukti** untuk throughput ~0.147 GFLOPS.
- Perbandingan apple-to-apple single-dispatch n=65536 iters=2000: Vulkan ramp wall ≈ 0.1457 s; OpenCL worksweep wall ≈ 7.07 s → OpenCL **~48–49× lebih lambat** pada kerja identik. (Catatan: angka gflops Q1 memakai faktor reps*R=10; setelah dinormalisasi, rasio ini tetap ~49×.)
- `kernel_ns` di worksweep/raw sangat bervariasi (mis. 4192000 vs 365056 pada cfg identik) → tidak reliabel; gflops dihitung dari `wall_s`, bukan `kernel_ns`.

---

## 8. Verdict ronde

Klaim: *"OpenCL di harness/konfigurasi ini ~0.147 GFLOPS, linear terhadap kerja, datar terhadap bentuk kernel."*

| Klaim | Verdict | Bukti |
|---|---|---|
| ~0.147 GFLOPS | **TERVERIFIKASI** | worksweep 0.1484/0.1470/0.1397/0.1465; discriminator 0.1459–0.1512; grid l64 0.148–0.151 |
| linear terhadap kerja | **TERVERIFIKASI** (kira-kira) | wall ∝ iters (9.91× untuk 10×, 94.1× untuk 100×) dan ∝ n (9.88× untuk 10×); deviasi ≤ ~6% di sel terkecil |
| datar terhadap bentuk kernel | **TERVERIFIKASI SEBAGIAN / sebagian TERBANTAH** | datar terhadap **ILP** (K=1..8 pada lsize=64, saling tumpang tindih); **TIDAK datar terhadap lsize** (64→128 turun ~40% ke ~0.090, disjoint) |

**Verdict ronde: TERVERIFIKASI SEBAGIAN.** Sub-klaim "~0.147 GFLOPS" dan "linear terhadap kerja" didukung raw. Sub-klaim "datar terhadap bentuk kernel" hanya benar untuk variasi ILP; variasi lsize mengubah hasil secara material sehingga klaim itu, apa adanya, **tidak** didukung.

---

## 9. Ringkas semua status

| Pertanyaan | Status |
|---|---|
| Q1 (H1 vs H2) | **TIDAK TERVERIFIKASI** — arm batch1 legal (DISPATCHES_RECORDED 16, gate PASS), batch1 [74.896,76.981] disjoint vs batchfp [71.972,72.561] dan vs ramp [71.944,73.076]; tetapi ambang H1 (≥~86) tak tercapai dan sel n-besar rujukan tak terukur. |
| Q2 bentuk kurva penuh / titik belok / argmax | **TIDAK TERVERIFIKASI** (grid ~50% lengkap). Terukur: titik belok = lsize 64→128; argmax = l64a1 (lemah); ILP tak signifikan. |
| Q3 OpenCL n=1048576 | **TIDAK TERVERIFIKASI** (file tak pernah ditulis). |
| Worksweep: wall skala dengan kerja | **TERVERIFIKASI**. |
| Worksweep: refutasi "biaya tetap 7 s/dispatch" | **TERBANTAH** (klaim runner lama). |
| maxcu=1 / type=0x0 | Fakta tercetak; penyebab TIDAK terbukti. |

Tidak ada `GATE ... FAIL` di raw mana pun. Tidak ada selisih aritmetika > 1%.

---

## 10. Addendum — bukti `q2-fullbuf.raw.txt` (verifikasi penuh buffer + CPU vs OpenCL)

Isi mentah `q2-fullbuf.raw.txt`:
```
CPU_FMA n=65536 iters=2000 wall_s=0.211486 gflops=4.95813 sink=5.24288e+08
OCL_KERNEL n=65536 iters=2000 wall_s=6.920292 gflops=0.15152
FULLBUF_PASS ok=262144 mn=2000 mx=2000
DONE
```

### 10.1 Aritmetika (flops = n*4*iters*2 = 65536*4*2000*2 = 1.048576e9)
- CPU_FMA: 1.048576e9 / 0.211486 / 1e9 = **4.95813** → cocok dengan tercetak (4.95813). Δ ~0%.
- OCL_KERNEL: 1.048576e9 / 6.920292 / 1e9 = **0.15152** → cocok dengan tercetak (0.15152). Δ ~0%.
- Rasio CPU:OCL = 4.95813 / 0.15152 = **32.72×**. (Rasio wall 6.920292 / 0.211486 = 32.72× — konsisten.) Klaim leader "~33×" **BENAR**.

### 10.2 FULLBUF_PASS — gate diperluas ke seluruh buffer
`FULLBUF_PASS ok=262144 mn=2000 mx=2000` — **TERVERIFIKASI**: seluruh **262144** elemen buffer z di-scan (bukan hanya C[0]), min==max==2000==iters → setiap elemen z == iters. Ini menghapus kemungkinan "hanya 1 elemen benar / sisa buffer tidak ditulis". Gate penuh lolos; hasil OpenCL bukan artefak gate.

### 10.3 Verdict yang dinilai

| Klaim leader | Verdict verifier | Dasar angka |
|---|---|---|
| Q1 SAH: H1 didukung arah+besaran (batch1 > batchfp, disjoint) | **SETUJU (sebagian)** | batch1 [74.896, 76.981] vs batchfp [71.972, 72.561] = **disjoint**, batch1 lebih tinggi. Arah & disjointness TERVERIFIKASI. **Catatan**: ini belum menutup tes pembeda SPEC (ambang batch n=65536 ≥ ~86 tidak tercapai; sel rujukan n=1048576 tidak diukur) → "H1 didukung penuh" tetap tidak dapat disertifikasi; yang sah adalah klaim *relatif* batch1>batchfp. |
| Q2 PATOLOGIS: pada FMA identik, 1 thread CPU (4.96) mengalahkan jalur OpenCL (0.15) ~33× | **SETUJU** | rasio raw = 32.72×; gate penuh `FULLBUF_PASS ok=262144 mn=mx=2000` lolos → bukan artefak gate. Menunjukkan kegagalan jalur loader/driver, **bukan** karakteristik hardware Adreno 616. |
| Mekanisme (maxcu=1 / type=0x0) TERSANGKA, belum dikonfirmasi | **SETUJU** | tercetak `CLDEV ... type=0x0 maxcu=1 maxmhz=1`; tidak ada eksperimen di raw yang membuktikan kausalitas. Tetap tersangka. |
| Baseline 96 GFLOPS historis dilarang | **SETUJU** | tidak ada di raw ronde ini; tidak dapat direproduksi. |

### 10.4 Ringkas status terbaru

| Pertanyaan | Status |
|---|---|
| Q1 | Arah+besaran (batch1>batchfp, disjoint) **TERVERIFIKASI**; H1 penuh vs ambang SPEC **TIDAK TERVERIFIKASI** (sel n-besar absen). |
| Q2 grid lsize×ILP | Bentuk kurva penuh **TIDAK TERVERIFIKASI** (parsial); efek lsize 64→128 (~-40%, disjoint) TERVERIFIKASI. |
| Q2 patologis (CPU vs OpenCL) | **TERVERIFIKASI** — CPU 1-thread 4.958 GFLOPS vs OpenCL 0.1515 GFLOPS (32.72×), gate penuh buffer lolos. |
| Q3 | **TIDAK TERVERIFIKASI** (file tak pernah ada). |

**Verdict ronde (revisi): TERVERIFIKASI SEBAGIAN.** Klaim inti "jalur OpenCL lewat loader ini patologis (~0.15 GFLOPS, ~33× lebih lambat dari 1 thread CPU pada FMA identik, gate penuh lolos)" **DIDUKUNG RAW**. Klaim itu **bukan** pernyataan tentang plafon hardware Adreno 616 dan **tidak** boleh dikutip sebagai karakteristik hardware.
