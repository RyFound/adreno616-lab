# REVIEW.md — Checklist Verifikasi Harness (r1)

Reviewer: spec-reviewer. Kontrak acuan: `SPEC.md` (beku).
Dokumen ini dipakai DUA kali: (a) checklist pra-build untuk gpu-runner, (b) kriteria pass/fail saat memeriksa kode+raw log yang sudah jadi.

Status aturan: **PASS/FAIL eksplisit per item.** Item FAIL = hasil ronde ditolak (SPEC §7).

---

## A. Checklist pra-build (kirim ke gpu-runner sebelum build)

### [V1] Arm Vulkan `batch1` — re-record, bukan re-submit
**Bahaya termahal.** Kalau writer me-resubmit command buffer 1-dispatch yang sama (pola ramp lama), arm "batch" mengukur nol.
- Cek kode: loop rekam memiliki **16 `vkCmdDispatch` sebelum `vkEndCommandBuffer`**; hanya **1 `vkQueueSubmit`** dan **1 `vkWaitForFences`** per rep.
- Harness **mencetak jumlah dispatch terekam** dan jumlah submit/fence, mis.: `arm=batch1 n=65536 R=16 recorded_dispatch=16 submits=1 fences=1`.
- **PASS:** `recorded_dispatch == R == 16` DAN `submits == 1` DAN `fences == 1` per rep.
- **FAIL:** `recorded_dispatch == 1` (cb 1-dispatch di-resubmit), atau `submits > 1`, atau angka `recorded_dispatch` tidak dicetak sama sekali.
- **Kontrol wajib** arm `batch-fence-per-rep`: R=16, **16 submit + 16 fence terpisah**, n=65536. Log harus menunjukkan `submits=16 fences=16`. PASS bila tercetak dan nilainya sesuai.

### [V2] Gate eksak + exit non-zero
- Setiap arm: `z=z*x+y`, x=y=1, z0=0 → `C[0] == iters`.
- Cetak baris `GATE <arm/sel> C0=<val> iters=<val> PASS|FAIL`.
- **PASS:** jalur FAIL melakukan `exit(1)` / `return 1` non-zero (bukan hanya `printf("FAIL")` lalu lanjut).
- **FAIL:** gate hanya mencetak tanpa exit non-zero, atau gate tidak ada di salah satu arm, atau `iters` tidak didefinisikan = jumlah dispatch×workgroup (gate harus mengikat ke jumlah kerja sebenarnya, bukan konstanta lepas).

### [V3] Raw log = deliverable, bukan ringkasan
- stdout tiap run di-stream ke file: `q1-vulkan.raw.txt`, `q2-opencl.raw.txt`, `q3-large-n.raw.txt` (shell `> file 2>&1`).
- **PASS:** ketiga file ada, berisi header harness + baris GATE + baris RESULT, dan isinya identik dengan yang dicetak program (tidak dikurasi).
- **FAIL:** raw log tidak ada; atau `RESULTS.md` memuat blok "raw log" hasil salin-tempel/ringkas sebagai pengganti file `.raw.txt`.

### [V4] ≥3 repeat per sel + identitas baris
- Setiap sel/arm dijalankan ≥3 kali.
- **Setiap baris log** memuat index run + nama sel, mis. `run=2/3 cell=lsize256_ilp4 n=65536 ...`.
- **PASS:** jumlah baris RESULT per sel ≥3 DAN tiap baris punya `run=` dan nama sel.
- **FAIL:** ada sel <3 repeat; atau baris log tak memuat index run/nama sel (urutan & panas device jadi buta).

### [V5] Status ramp dicatat (arm Vulkan)
- Tiap arm Vulkan memakai nilai ramp tetap dan **mencetak nilai itu** di baris arm.
- Ideal: sampling `cur_freq` **di dalam jendela timing** (kontrol positif).
- **PASS:** nilai ramp tercetak per arm; bila ada sampling in-window, nilainya tercetak.
- **FAIL:** tidak ada nilai ramp di log. Catatan: "Ramp=1000 tercetak" ≠ "clock 610 di jendela ukur" — jangan klaim clock tanpa sampling in-window.

### [V6] Q2 — ILP independen + pemisahan efek
- Kumparan akumulator **independen** (k akumulator per lane, ILP), **bukan** chain serial `z=z*x+y` berulang.
- **PASS kode:** untuk depth k>1, ada k variabel akumulator berbeda yang digabung di akhir (bukan satu variabel di-update berulang).
- **FAIL kode:** depth k hanya mengulang satu chain serial (itu bukan ILP).
- **PASS laporan:** `RESULTS.md`/raw memisahkan **efek utama lsize** (64/128/256) dan **efek utama ILP** (1/2/4/8) sebagai dua tabel/dua analisis terpisah — bukan satu ranking gabungan.
- **FAIL laporan:** hanya ada satu ranking 12 sel tanpa pemisahan efek utama.

### [V7] Q3 — OpenCL n=1048576 apple-to-apple
- Ukur OpenCL pada n=1048576 dengan harness bergate yang **sama** (bukan ekstrapolasi dari 65536).
- **PASS:** ada baris RESULT Q3 dengan `n=1048576`, ≥3 repeat, gate PASS, dan status ramp tercetak.
- **FAIL:** angka Q3 diekstrapolasi, atau tanpa gate, atau tanpa repeat.

### [V8] Jebakan build/run (cek sebelum build)
- (a) Vulkan **tanpa** `-lvulkan` (header NDK saja); OpenCL link `libOpenCL.so`. **FAIL** bila ada `-lvulkan`.
- (b) Jalankan via linker64 dengan **path absolut**: `LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 /apex/com.android.runtime/bin/linker64 /abs/path/bin`. **FAIL** bila `linker64 ./bin` (path relatif).
- (c) `setvbuf(stdout,0,_IONBF,0)` di awal `main`. **FAIL** bila tidak ada (stdout block-buffered tampak hang → log kosong/terpotong).
- (d) Tidak ada `\n` literal di dalam string lewat write_file (korup). **FAIL** bila terdeteksi; gunakan `puts()`.

---

## B. Kriteria pass/fail saat verifikasi akhir (setelah kode+log jadi)

Diperiksa dari **output mentah program** (SPEC §2.6), bukan ringkasan anggota tim.

| # | Item | PASS | FAIL |
|---|---|---|---|
| V1a | batch1 recorded_dispatch | == R == 16, submits=1, fences=1 | ==1 atau submits>1 atau tak dicetak |
| V1b | kontrol fence-per-rep | submits=16 fences=16 tercetak | tak ada / nilai salah |
| V2 | gate eksak + exit | semua arm GATE PASS, exit(1) di jalur FAIL | gate hilang / tak exit non-zero |
| V3 | raw log | 3 file .raw.txt lengkap, tak dikurasi | tak ada / diganti ringkasan |
| V4 | repeat + identitas | ≥3/sel, tiap baris `run=`+sel | <3 / baris tanpa identitas |
| V5 | status ramp | ramp tercetak per arm Vulkan | tak ada |
| V6 | ILP + efek utama | kumparan independen; efek lsize & ILP terpisah | chain serial / ranking gabungan |
| V7 | Q3 large-n | OpenCL n=1048576 bergate, ≥3 repeat | ekstrapolasi / tanpa gate |
| V8 | build/run traps | keempat (a)-(d) bersih | ada pelanggaran |

### Uji pembeda Q1 (untuk memutuskan H1 vs H2) — bukan gate build, tapi gate interpretasi
- batch1 (R=16, 1 submit) n=65536 mencapai **≥ ~86** → **H1 didukung** (overhead submit+fence).
- batch1 tetap **~83** DAN fence-per-rep juga ~83 → **H2 didukung** (efek GPU-side; cerita "overhead" salah).
- Klaim delta antar-sel hanya sah bila **rentang min/max tidak tumpang tindih** (SPEC §2.2).

### Verdict ronde (SPEC §7)
- **SUKSES** bila V1–V8 PASS dan Q1/Q2/Q3 terjawab dari raw log.
- **GAGAL** bila ada angka dikutip tanpa gate PASS, tanpa repeat, atau tanpa raw log.

---

## C. Batas reviewer
- Reviewer **tidak mengedit kode siapa pun** dan **tidak menjalankan GPU/benchmark** (SPEC §5).
- Bila ada item yang tak bisa dipenuhi karena SPEC diam, eskalasi ke lead — jangan improvisasi.

---

## D. Verifikasi aktual kode harness vs checklist (preliminary, 2026-10-07)

Sumber: baca `vkbatch.c`, `oclgrid.c`, `run_q23.sh`, `q1-vulkan.raw.txt`, `q2-opencl.raw.txt`. (q2 dibaca SEBAGIAN; `q3-large-n.raw.txt` **tidak ada** saat pemeriksaan.) Bukan verdict ronde — itu lead.

| Item | Verdict (kode/raw) | Bukti |
|---|---|---|
| V1 batch1 | **PASS** | loop rekam `for r<R { pDispatch; disp_count++ }`; cetak `DISPATCHES_RECORDED 16`; timing `is_b1` = 1 reset + 1 submit + 1 wait per rep. Kontrol `batchfp` = 16 submit + 16 wait per rep, cb 1-dispatch, `disp=1`. |
| V2 gate+exit | **PASS, CATATAN KONTRAK** | `if(g0!=expect){ puts("GATE_FAIL_EXIT"); return 1; }` → non-zero. TAPI invariant batch1 = `C0==iters*R` (32000), **bukan** literal `C0==iters`. Benar matematis (16 dispatch berurutan), namun menyimpang dari SPEC §2.1 kata-per-kata → **butuh adjudikasi lead**. |
| V3 raw log | **FAIL (parsial)** | `q1-vulkan.raw.txt` & `q2-opencl.raw.txt` ada; **`q3-large-n.raw.txt` ABSEN** (list_directory + read_file "File not found") meski gpu-runner klaim V3/V7 PASS. |
| V4 index+sel/baris | **FAIL utk Q1** | RESULT Q1: `mode=... n=... reps=...` — **tanpa index run**. Header `### REP=n ARM=...` membawa urutan, tapi SPEC §2.3 minta tiap baris memuat index. Q2 punya `idx=`+`cell=`. |
| V5 ramp/freq | **PARTIAL** | ramp + `cur_freq_after` tercetak per baris; sampling DI DALAM jendela tidak ada (SPEC "bila memungkinkan"). Korelasi: cold 430MHz/37 vs ramp 610MHz/72. |
| V6 ILP | **PASS (kode)** | `build_kernel_src`: `a0..a{K-1}` akumulator independen, `a_j=fma(a_j,xv,yv)`, lalu dijumlah — bukan chain serial. |
| V8 traps | **PASS** | vkbatch: `-ldl`, tanpa `-lvulkan`, `setvbuf`; oclgrid: dlopen `libOpenCL.so`; `run_q23.sh`: linker64 path absolut; tak ada `\n` literal (pakai array `nl` char 10 + `puts("")`). |

### Anomali yang harus diangkat ke lead
1. **OpenCL ~0.15 GFLOPS vs Vulkan ~77 GFLOPS di silikon yang sama.** Q2 nyaris datar di semua sel (0.147–0.152) → kemungkinan terukur di FLOOR overhead/serialisasi, BUKAN compute-bound. Jika benar, deliverable "bentuk kurva lsize×ILP" tidak informatif sampai mekanisme dijelaskan.
2. **Q3 raw tidak ada** → klaim V7/V3 gpu-runner belum terverifikasi (bisa jadi job masih jalan).
3. **Q2 read terpotong** — belum konfirmasi 12 sel lengkap; serahkan ke log-verifier (task #3).

### Catatan fragilitas kode (non-blocking)
- `uint inner = iters / K;` — integer division; gate `C0==iters` hanya eksak bila `iters % K == 0`. Aman utk iters=2000, K∈{1,2,4,8}, tapi rapuh bila nanti iters diganti.

---

## E. Adjudikasi lead (2026-10-07) — mengikat

Lead mengadjudikasi temuan §D. Berlaku sebagai keputusan, bukan lagi temuan terbuka:

1. **V2 (batch1 `C0==iters*R`) — DITERIMA, BUKAN pelanggaran.** Invariant per-arm sah: batch1 pakai `C0==iters*R` (ekSAK, lebih ketat dari gate 1-dispatch). Cacat ada di SPEC §2.1 (ditulis utk arm single-dispatch), bukan di harness. Lead akan mengamandemen SPEC. **Jangan tandai V2 FAIL.**
2. **V4 (index run di baris RESULT Q1) — deviasi format nyata, NON-BLOCKING.** Q1 memakai header `### REP=n ARM=` sebagai pembawa urutan. **Tidak minta re-run Q1.** SPEC ronde berikutnya akan mewajibkan index di baris RESULT.
3. **V3/V7 (`q3-large-n.raw.txt` absen) — BELUM TERVERIFIKASI, bukan FAIL.** Kampanye Q3 dihentikan lead karena anomali OpenCL. Klaim V7 'PASS' runner belum terbukti.
4. **Anomali OpenCL 0.15 GFLOPS — Q2 DIBEKUKAN.** Sweep dihentikan; runner menjalankan satu arm pembeda (batch R=10 + satu `clFinish` + `CL_QUEUE_PROFILING_ENABLE`, bandingkan `kernel_ns` vs `wall_ns`). Jangan verifikasi Q2 lebih jauh sampai `q2-discriminator.raw.txt` ada.
6. **Verdict Q1 sudah SAH dan tetap** (tidak berubah oleh adjudikasi ini).

---

## F. VERDICT FINAL (state final, 2026-10-07)

Semua file kecuali `q3-large-n.raw.txt` (memang tidak pernah ditulis — kampanye Q3 dihentikan lead). Sumber verdict: `q1-vulkan.raw.txt`, `q2-opencl.raw.txt`, `q2-discriminator.raw.txt`, `q2-worksweep.raw.txt`, `RESULTS.md`, + kode `vkbatch.c`, `oclgrid.c`, `oclprof.c`, `oclws.c`, `run_q23.sh`.

### Verdict per item V1–V8

| # | Item | Verdict FINAL | Dasar |
|---|---|---|---|
| V1 | batch1 re-record | **PASS** | 16 `vkCmdDispatch` direkam ke satu cb, 1 submit+1 fence/rep; `DISPATCHES_RECORDED 16` & `disp=16` di raw; kontrol batchfp `disp=1`/16 submit+16 fence. |
| V2 | gate eksak + exit | **PASS** (adjudikasi lead) | Semua arm GATE PASS; FAIL `exit(1)`. Invariant per-arm sah: batch1 `C0==iters*R=32000`, single `C0==iters=2000`. **Bukan pelanggaran** — SPEC §2.1 diamandemen lead. |
| V3 | raw log | **PASS (dgn 1 catatan)** | `q1-vulkan.raw.txt`, `q2-opencl.raw.txt`, `q2-discriminator.raw.txt`, `q2-worksweep.raw.txt` ada, stdout mentah tak diedit. **`q3-large-n.raw.txt` TIDAK PERNAH ADA** — bukan FAIL, karena Q3 dihentikan lead dan tak ada angka Q3 yang diklaim. |
| V4 | index run + nama sel/baris | **PASS (Q2/Q3), deviasi NON-BLOCKING (Q1)** | Q2 semua baris `idx=`+`cell=`; Q1 RESULT tanpa index (urutan via header `### REP=n ARM=`). Lead: non-blocking, tanpa re-run. |
| V5 | status ramp | **PASS** | `ramp=` + `cur_freq_after` di tiap baris RESULT Q1 (cold 430MHz/37 vs ramp 610MHz/72). Sampling in-window tidak ada (SPEC "bila memungkinkan"). |
| V6 | ILP independen + efek terpisah | **PASS (kode), N/A (laporan)** | `build_kernel_src`: `a0..a{K-1}` independen lalu dijumlah. Q2 grid DIHENTIKAN sebelum 12 sel, jadi pemisahan efek utama lsize/ILP tidak pernah relevan — digantikan arm pembeda. |
| V7 | Q3 large-n | **BELUM TERVERIFIKASI** (bukan FAIL) | `q3-large-n.raw.txt` tak ada; tidak ada angka Q3. |
| V8 | build/run traps | **PASS** | `-ldl` tanpa `-lvulkan`; `setvbuf`; dlopen OpenCL; linker64 path absolut; tak ada `\n` literal. |

### Q1 — TIDAK TERVERIFIKASI (hanya ARAH mekanisme) [dikoreksi, rujuk SPEC §8]
- **Kriteria falsifiable SPEC §1 TIDAK tercapai:** ambang `batch1 n=65536 ≥ ~86 → H1 didukung`, sedangkan batch1 = **76.282** [74.896, 76.981]. Ambang tidak tercapai → **H1 TIDAK dinyatakan** (log-verifier benar).
- **H2 juga TIDAK dinyatakan** — `ramp` (72.5) vs `batchfp` (72.3) **tumpang tindih**, jadi pemisahan tidak konklusif.
- Yang **tetap sah hanyalah ARAH mekanisme**, bukan besaran: batch1 vs batchfp **disjoint** dengan delta +3.98 GFLOPS (~+5.5%), dan `DISPATCHES_RECORDED=16` membuktikan batching benar-benar terjadi. Ini menunjukkan batching punya efek SEARAH H1, tapi tidak membuktikan H1 pada ambang yang ditetapkan.
- **Otoritas: SPEC §8 (adjudikasi lead).** Verdict Q1 di §F ini tunduk pada SPEC §8.

### Q2 — DITUTUP sebagai TEMUAN POSITIF (bukan artefak)
Dua arm pembeda membalik hipotesis 'overhead tetap':
1. **`q2-worksweep`**: wall **SKALA LINEAR** dengan kerja — iters 2000→20 ⇒ wall 7.0→0.074 s (100×); n 65536→6553 ⇒ wall 7.0→0.716 s (10×). `gflops` DATAR ~0.147 di semua konfigurasi. **Bukan overhead tetap.**
2. **`q2-discriminator`**: `batch ≈ perrep` (0.1459 vs 0.1473) → protokol `clFinish`-per-rep **bukan** artefak. `kernel_ns` TIDAK dapat dipercaya (inkonsisten 4.3 ms vs 0.6 ms untuk kernel identik; menyiratkan ~1700 GFLOPS, ~47× di atas Vulkan nyata 37 GFLOPS di GPU yang sama).

**AMENDEMEN FINAL (2026-10-07, mengikat — menggantikan verdict 'compute-bound' di atas):**

Dua kontrol terakhir (`q2-fullbuf.raw.txt`) membalik verdict:
- **FULLBUF_PASS ok=262144 mn=2000 mx=2000** → gate buffer-penuh lolos (262144 elemen, semua == iters). Jadi 0.147 mengukur **kerja penuh**, bukan artefak gate/partial-work.
- **CPU_FMA gflops=4.958 (1 thread) vs OCL_KERNEL gflops=0.1515** → pada jumlah FMA identik, **satu thread CPU mengalahkan jalur OpenCL ini ~33×**.

**Verdict Q2 yang benar: jalur OpenCL lewat `libOpenCL.so` ini PATOLOGIS.** Ini **kegagalan jalur loader/driver**, **BUKAN** karakteristik hardware Adreno 616. Karena gate penuh sudah diverifikasi, ini **bukan** artefak pengukuran. Framing 'compute-bound ~0.147 GFLOPS (temuan positif)' di atas **DIBATALKAN**.

**Yang TETAP berlaku:**
- **Q1 TIDAK TERVERIFIKASI** (hanya arah mekanisme; H1 tidak dinyatakan karena ambang §1 tidak tercapai — lihat §F Q1).
- **Q3 tidak pernah dijalankan** (`q3-large-n.raw.txt` tak ada).
- Mekanisme tersangka (**`maxcu=1`/`type=0x0`** — kemungkinan serialisasi/device-info rusak) **belum dikonfirmasi**; ditandai untuk ronde berikutnya.
- `q2-opencl.raw.txt` parsial (l64) & grid 12-sel tidak diselesaikan → klaim 'bentuk kurva' tetap tidak dibuat.
- `kernel_ns` profiling tetap tidak dapat dipercaya (inkonsisten; menyiratkan kecepatan di atas Vulkan).

**BATAS WAJIB:**
- '0.147 GFLOPS' adalah deskripsi **jalur OpenCL lewat loader ini**, bukan klaim hardware. Dilarang menulis 'Adreno 616 memang lambat' dari data ini.
- Kontras CPU ~33× adalah bukti bahwa bottleneck ada di jalur loader/driver (host-side atau dispatch), bukan di ALU FP32 GPU.

### Q2 — efek bentuk kernel (dikoreksi, rujuk SPEC §8)
- Klaim awal 'datar terhadap bentuk kernel' **DIBATALKAN untuk lsize**: lsize 64→128 **turun ~40% (rentang disjoint)** → **efek lsize DOMINAN**.
- **Efek ILP TIDAK signifikan** (1/2/4/8 praktis datar).
- Jadi bukan 'nol efek bentuk kernel' — yang benar: **lsize dominan, ILP nol**. Verdict 'jalur OpenCL patologis' tetap berlaku.
- **Otoritas: SPEC §8 (adjudikasi lead).**

### Status ronde (SPEC §7)
- Kriteria sukses **sebagian**: **Q1 TIDAK TERVERIFIKASI** (hanya arah mekanisme; ambang §1 tidak tercapai); **Q2 dijawab sebagai temuan PATOLOGI jalur OpenCL** dgn lsize dominan/ILP nol (bukan karakteristik hardware, bukan artefak); **Q3 tidak dijalankan** (dihentikan lead, bukan gagal).
- Tidak ada angka yang dikutip tanpa gate PASS, tanpa repeat, atau tanpa raw log. Semua GATE PASS dengan `exit(1)` di jalur FAIL.

### Batas reviewer
- Verdict dari output mentah program + kode. Tidak mengedit kode, tidak menjalankan GPU/benchmark (SPEC §5).

