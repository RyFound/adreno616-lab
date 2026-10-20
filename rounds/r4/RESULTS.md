# gpu-r4 — mengeliminasi mekanisme spike K=2 dan menemukan batas eksekusi dispatch

2026-10-08. Adreno 616, clock 610 MHz, chroot. Harness **vkopt8** (NDK r30 clang, dari `vkopt_instr.c`
r3) dengan gate **full-buffer** (`nbad`/`nzero`/`firstbad`/`lastbad`, bukan lagi satu elemen) +
pelaporan `VkResult` submit/wait per rep. Raw: `e5_spv_stats.out.txt`, `e7_kcurve.raw.txt`,
`e8_accindep.raw.txt`. E9 mentah ada di transkrip (tidak di-tee ke file).

Semua sel r4: `submit=0 wait=0`, gate `nbad=0` kecuali yang ditandai FAIL.

## Ringkasan tujuan r4 (5 baris)

1. Jelaskan spike K=2: apakah artefak SPIR-V, artefak harness, atau mekanisme driver.
2. Pisahkan kontribusi "2 akumulator hidup" vs "2 FMA per iterasi" terhadap spike.
3. Validasi setiap pengukuran dengan `VkResult` + gate seluruh buffer (bukan satu elemen).
4. Eliminasi hipotesis secara eksperimental, bukan dengan narasi.
5. Perbaiki sel invalid r3 (n=1M K=16) dengan penjelasan mekanistiknya.

## Tabel prioritas eksperimen r4

| Prioritas | Eksperimen | Hipotesis | Variabel | Metrik | Information gain | Biaya | Risiko | Kriteria konfirmasi/refutasi |
|---|---|---|---|---|---|---|---|---|
| 1 | **E5** analisis statis SPIR-V seluruh grid L×K | H_SPV: anomali K=2 hidup di representasi SPIR-V | L, K (sumber) | bytes, op-count, fmul/fadd, load/store, id-count, `spirv-val` | **tinggi** (gratis, deterministik) | ~nol | rendah | Konfirmasi bila ada diskontinuitas di K=2; refutasi bila linier |
| 1 | **E8** poros independensi akumulator (B2 vs B3) | H_ILP: spike = 2 FMA independen/iter, bukan liveness register | jumlah FMA/iter (1 vs 2), jumlah acc tetap 2 | GFLOPS | **tinggi** (memisahkan dua kandidat mekanisme) | rendah | sedang (butuh shader baru) | H_ILP terkonfirmasi bila 1 fma/iter ≈ K=1 dan 2 fma/iter ≈ K=2 |
| 2 | **E7** reproduksi kurva K di L=64 pada harness tervalidasi | Spike r3 nyata, bukan artefak gate satu-elemen | K, harness (lama vs baru) | GFLOPS + status gate | sedang-tinggi | rendah | rendah | Spike bertahan + gate full-buffer PASS di semua sel |
| 2 | **E9** sel n=1M K=16 (invalid di r3) pada proses bersih | Kegagalan r3 punya akar deterministik (bukan transient) | n, K | `nbad`, `nzero`, `firstbad`, `lastbad`, `VkResult` | **tinggi** (validitas pengukuran) | rendah | rendah | Konfirmasi bila pola kegagalan reproducible, bukan `wall≈0` |

## E5 — SPIR-V statis: tidak ada anomali K=2

24 shader (`vk_g_L*_K*.spv`) dianalisis dengan `spirv-dis`, semua `spirv-val = ok`.

| L | K | bytes | ops | fmul | fadd | ext(FMA) | load | store | n_ids |
|---|---|---|---|---|---|---|---|---|---|
| 32/64/96/128 | 1 | 1992 | 132 | 1 | 1 | 0 | 13 | 8 | 31 |
| 32/64/96/128 | 2 | 2168 | 143 | 2 | 3 | 0 | 17 | 10 | 38 |
| 32/64/96/128 | 3 | 2344 | 154 | 3 | 5 | 0 | 21 | 12 | 45 |
| 32/64/96/128 | 4 | 2520 | 165 | 4 | 7 | 0 | 25 | 14 | 52 |
| 32/64/96/128 | 8 | 3224 | 209 | 8 | 15 | 0 | 41 | 22 | 80 |
| 32/64/96/128 | 16 | 4632 | 297 | 16 | 31 | 0 | 73 | 38 | 136 |

**Kesimpulan E5:** struktur **identik lintas L** dan **linier sempurna terhadap K**
(`fmul=K`, `fadd=2K−1`, `n_ids=7K+24`, tidak ada `OpExtInst Fma`). **Tidak ada diskontinuitas di K=2.**
→ **H_SPV (spike berasal dari representasi sumber SPIR-V) TEREFUTASI.** Anomali K=2 harus hidup
di codegen/scheduling driver, bukan di shader.

## E7 — kurva K tervalidasi ulang (L=64, n=65536, ramp 500, 3 run/sel)

| K | GFLOPS (3 run) | mid | gate |
|---|---|---|---|
| 1 | 81.80 / 80.08 / 81.27 | ~81.0 | PASS (nbad=0) |
| 2 | 108.90 / 109.84 / 110.68 | **~109.8** | PASS |
| 3 | 84.43 / 84.35 / 84.02 | ~84.3 | PASS |
| 4 | 86.28 / 86.25 / 86.33 | ~86.3 | PASS |
| 8 | 85.89 / 85.84 / 85.83 | ~85.85 | PASS |
| 16 | 88.21 / 88.16 / 88.06 | ~88.14 | PASS |

Rasio K=2/K=1 = **1.36**. Seluruh buffer benar (`nbad=0`), `submit=0 wait=0` di setiap rep.

**E7b (spike lintas L, harness baru):** L=32 K=1 ~42.4 / K=2 ~57.2 (rasio 1.35);
L=128 K=1 68.0–72.3 (**noisy, spread ~4.3**) / K=2 ~101.97 (rasio ~1.45, provisional).

**Kesimpulan E7:** spike K=2 **bertahan** pada harness dengan gate full-buffer + `VkResult` penuh.
→ **H_HARNESS (spike r3 artefak gate satu-elemen) TEREFUTASI.** Kurva r3 tereproduksi.
Catatan: L=128 K=1 terlalu noisy untuk rasio presisi; L=64 tetap paling terkontrol.

## E8 — poros independensi akumulator (mekanisme) — HASIL KUNCI

Dua shader mengubah **satu variabel** (jumlah FMA/iterasi) sambil menahan jumlah akumulator hidup = 2:

- **B2**: `a1 = y[i]` dimuat (tidak bisa di-fold), `a0` di-update 1 FMA/iter, output `a0 + a1 − 1`.
  Dua akumulator hidup, **1 FMA/iter**. Dijalankan dengan K=1 (gate cocok).
- **B3**: dua akumulator, **2 FMA/iter** (kontrol positif; harus = K=2 asli).

| Sel | K | GFLOPS (3 run) | mid | gate |
|---|---|---|---|---|
| B2 (2 acc, 1 fma/iter) | 1 | 75.54 / 75.40 / 75.33 | ~75.4 | PASS |
| B3 (2 acc, 2 fma/iter) | 2 | 113.29 / 112.15 / 111.73 | **~112.4** | PASS |
| ref K=1 asli | 1 | 81.37 / 82.84 / 80.57 | ~81.6 | PASS |

**Kesimpulan E8:** menambah akumulator hidup **tanpa** menambah FMA/iter (B2) **tidak** mempercepat —
bahkan sedikit lebih lambat dari K=1 (75.4 vs 81.6, kemungkinan tekanan register ekstra).
Menambah **FMA independen kedua per iterasi** (B3) memunculkan spike penuh (~112.4).
→ **H_ILP (dua rantai FMA independen per iterasi = ILP) TERKONFIRMASI.**
→ **H_LIVENESS (sekadar "punya 2 register akumulator hidup") TEREFUTASI.**

## E9 — sel n=1M K=16: bukan device-lost, tapi eksekusi parsial terpotong

Pada harness tervalidasi, n=1M K=16 **tidak** menghasilkan `submit=-4`/`wall≈0` seperti r3.
Yang terjadi: `submit=0 wait=0`, gate FAIL, dan **seluruh elemen yang salah bernilai nol (tidak tertulis)**.

| n | K | nbad | nzero | firstbad (idx float) | lastbad | gate |
|---|---|---|---|---|---|---|
| 262144 | 16 | 0 | 0 | −1 | −1 | PASS |
| 524288 | 16 | 0 | 0 | −1 | −1 | PASS |
| 786432 | 16 | 290560 | 290560 | 2854912 | 3145727 | FAIL |
| 1048576 | 16 | ~1.28–1.35 M | = nbad | ~2.85 M | 4194303 | FAIL |
| 1048576 | 2 / 4 / 8 | 0 | 0 | −1 | −1 | PASS |

**Pola:** kegagalan adalah **ekor kontigu yang tidak pernah ditulis** (`firstbad`→akhir buffer,
semua nol). `firstbad/4` ⇒ **~713.000 work-item selesai** dan itu **konstan** di kedua n yang gagal
(713728 @ n=786432; 713344 @ n=1048576) — jadi batasnya adalah **jumlah kerja tetap**, bukan fraksi n.
Dengan laju ~2.9 µs/work-item (dari n=64K K=16), ~713K work-item ≈ **~2 s** waktu GPU.

**Kesimpulan E9:** kegagalan n=1M K=16 adalah **pemotongan dispatch oleh anggaran waktu GPU
(~2 s, hipotesis mekanisme), dengan driver melaporkan sukses**. Ini menjelaskan sel invalid r3
(27 juta GFLOPS): `wall` hanya mencerminkan pekerjaan parsial, sisanya tidak pernah jalan.
Bukan artefak acak; deterministik. K≤8 pada n=1M aman.

## Status hipotesis akhir r4

| Hipotesis | Status setelah r4 |
|---|---|
| H_SPV — spike hidup di representasi SPIR-V | **TEREFUTASI** (struktur linier & seragam) |
| H_HARNESS — spike r3 artefak gate satu-elemen | **TEREFUTASI** (bertahan di gate full-buffer) |
| H_ILP — spike = 2 FMA independen/iterasi | **TERKONFIRMASI** (B2 vs B3, satu variabel) |
| H_LIVENESS — spike = 2 register akumulator hidup | **TEREFUTASI** (B2 punya 2 acc, tanpa spike) |
| H2 workgroup→wave (dari r3) | tetap TERBANTAH |
| Non-monoton K=16>K=3 @n=64K | tetap terkonfirmasi (r4: 88.14 > 84.3, rentang disjoint) |
| n=1M K=16 "device lost" | **DIREVISI**: eksekusi parsial terpotong anggaran waktu GPU |
| `fma()` jalur codegen beda | tidak disentuh r4; mekanisme emulasi tetap hipotesis |

## Edge cases dan penanganan

- **`VK_ERROR_DEVICE_LOST` / `submit=-4`**: dilaporkan per rep; sel dengan nilai apa pun ≠ 0 dibuang.
  Di r4 tidak ada satu pun (0 kemunculan) — tapi justru muncul mode kegagalan baru: `submit=0` + hasil parsial.
- **Hasil parsial (ekor nol)**: dideteksi oleh gate full-buffer (`nzero=nbad`, `firstbad`→akhir).
  Gate satu-elemen r3 akan **melewatkan** ini (elemen[0] benar). Ini alasan gate full-buffer wajib.
- **`wall≈0` / GFLOPS mustahil**: tidak muncul di r4; bila muncul → invalid, bukan data.
- **L=128 K=1 noisy** (spread ~4.3): rasio L=128 ditandai *provisional*; L=64 dipakai untuk klaim.
- **`linker64` menuntut path absolut**: `./vkopt8` gagal (`expected absolute path`); selalu pakai path absolut.
- **Shader baru harus lolos `spirv-val`** sebelum dijalankan (B2/B3 lolos).
- **`write_file` mengubah `\n` di literal string C**: tidak relevan di r4 (shader ditulis via write_file
  sebagai file .comp terpisah, bukan disematkan ke C).

## Kriteria sukses r4

1. **Satu hipotesis mekanisme terkonfirmasi + satu terefutasi dengan bukti** → H_ILP terkonfirmasi,
   H_LIVENESS/H_SPV/H_HARNESS terefutasi. ✔
2. **Semua pengukuran tervalidasi `VkResult` + gate seluruh buffer** → semua sel r4 `submit=0 wait=0`,
   gate `nbad=0` kecuali sel yang dinyatakan FAIL. ✔
3. **Keputusan berbasis information gain, bukan GFLOPS** → E5/E8/E9 dipilih untuk memisahkan hipotesis. ✔
4. **Tidak ada klaim kausal tanpa bukti mekanistik** → klaim ILP dari poros satu-variabel B2/B3. ✔
5. **Output memuat tabel prioritas, edge cases, kriteria sukses** → dokumen ini. ✔

## Rekomendasi r9 (rencana; belum dijalankan)

1. **Konfirmasi anggaran waktu ~2 s**: ulangi ambang n pada K=16 dengan `VKOPT_R`/iterasi bervariasi;
   prediksi: batas ~713K work-item bergeser bila iterasi diubah, tetap bila n diubah.
2. **Uji ILP lebih lanjut**: B4 = 4 FMA/iter dengan 4 acc (prediksi: turun bila register spill) vs
   B5 = 2 FMA/iter dengan 4 acc (memisahkan ILP dari jumlah acc pada K>2).
3. **Batas dispatch**: jalankan n=1M K=16 dipecah jadi 2 dispatch (VKOPT_R atau grid-stride) —
   bila hasil benar, anggaran waktu per-dispatch terkonfirmasi.
4. Jangan kutip n=1M K=16 sebagai angka apa pun sampai mode parsial ini ditangani.

## Risiko/batas

- Mekanisme anggaran waktu GPU masih **hipotesis** (bukti: batas work-item konstan, estimasi ~2 s).
- L=128 K=1 terlalu noisy untuk rasio presisi.
- Semua klaim throughput tetap terikat konfigurasi (n, reps, L, K, ramp, bentuk kernel).
- 117.9 GFLOPS tetap **best observed**, bukan plafon.
