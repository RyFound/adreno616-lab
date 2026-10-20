# SPEC-r1 — Ruang Konfigurasi GPU Adreno 616 (OpenCL lsize×ILP, Vulkan batch-vs-fence)

Dibuat 2026-10-07. **Kontrak ini beku.** Setiap anggota tim membacanya; tidak ada improvisasi.

## 1. Pertanyaan yang harus dijawab (falsifiable, bukan "cari angka tercepat")

### Q1 — Vulkan: apakah overhead submit+fence menjelaskan 83→88?

Hipotesis H1: kenaikan 83 (n=65536) → 88 (n=1048576) pada clock jenuh disebabkan **overhead submit+fence per-rep** yang teramortisasi saat n besar.
Hipotesis alternatif H2: disebabkan **efek GPU-side** (grid 1024 workgroup per dispatch kurang memanfaatkan / tail), sehingga n besar lebih baik terlepas dari batching.

**Tes pembeda (wajib):** batch `R` dispatch ke dalam **satu** command buffer, **satu** submit, **satu** fence, dengan `R = 16` dan `n = 65536` → total kerja per submit ≈ `16 × 65536 = 1048576`, menyamai sel 88.3.

- Jika batched n=65536 mencapai **≥ ~86** (dalam rentang sel 88) → **H1 didukung**: overhead submit/fence adalah penyebabnya.
- Jika tetap **~83** → **H2 didukung**: penyebabnya GPU-side, cerita "overhead" SALAH.

**Kontrol wajib (untuk memisahkan batching dari total-kerja):** arm R=16 dengan **fence per-rep** (16 submit terpisah) pada n=65536. Ekspektasi di bawah H1: tetap ~83. Ini yang membedakan "jumlah dispatch" dari "cara submit".

**Verifikasi legalitas (jangan diasumsikan):** command buffer batch harus **di-record ulang** dengan 16 `vkCmdDispatch` (atau dispatch count dikalikan), bukan me-resubmit command buffer 1-dispatch yang sama. Inspeksi jumlah `vkCmdDispatch` yang terekam sebelum mempercayai angkanya. Kalau writer memakai ulang `cb` 1-dispatch, arm "batch" mengukur tidak ada apa-apa.

**AMANDEMEN 2026-10-07 — invariant per-arm (adjudikasi lead atas temuan spec-reviewer V2).** SPEC §2.1 menulis gate sebagai literal `C0==iters`; itu ditulis untuk arm *single-dispatch* dan tidak mengantisipasi arm batch. Invariant yang benar dan mengikat:
- Arm `cold` / `ramp` / `batchfp` (1 dispatch per rep): gate `C0 == iters`.
- Arm `batch1` (R dispatch dalam satu command buffer): gate `C0 == iters * R`.
Justifikasi: R dispatch berurutan dari `z=z*x+y` dengan `x=y=1`, `z0=0` menghasilkan `z = R*iters`. Gate tetap **eksak** dan justru lebih ketat. Karena itu arm `batch1` yang mencetak `C0 == iters*R` adalah **PASS**, bukan pelanggaran kontrak. Verifikasi legalitas tetap wajib: `DISPATCHES_RECORDED == R` dan `submits==1`/`fences==1`.

### Q2 — OpenCL: bentuk kurva lsize × ILP

**AMANDEMEN 2026-10-07 — Q2 DIHENTIKAN di tengah; grid penuh DIBATALKAN.** Data parsial (12 run pertama, lsize=64, ILP 1/2/4/8) menunjukkan angka **datar ~0.148–0.152 GFLOPS** di clock maks (610 MHz) — mengubah lsize maupun ILP tidak menggerakkan hasil sama sekali. Kedataran ini adalah sinyal bahwa yang terukur bukan throughput kernel melainkan floor jalur submit/finish driver. Karena itu sweep 12-sel tidak dilanjutkan; Q3 juga dihentikan. Sebagai gantinya dijalankan **satu arm pembeda** (`q2-discriminator.raw.txt`):
- queue dengan `CL_QUEUE_PROFILING_ENABLE`, cetak `kernel_ns` per dispatch di samping `wall_s`;
- arm batch R=10 dispatch + **satu** `clFinish`, timing sebagai satu batch;
- cetak `CL_DEVICE_MAX_COMPUTE_UNITS` dari run ini.
Keputusan ronde bergantung pada arm itu: (a) kalau batch jauh di atas 0.15 → protokol `clFinish`-per-rep adalah artefaknya, grid Q2 harus dijalankan ulang dalam mode batch; (b) kalau tetap ~0.15 dan `kernel_ns ≈ wall_s` → jalur OpenCL di device ini memang ~0.15 GFLOPS di konfigurasi ini.

Grid: `lsize ∈ {64, 128, 256}` × `accumulator depth ∈ {1, 2, 4, 8}` = **12 sel**.

- `accumulator depth = k` berarti k akumulator independen per lane (ILP), bukan chain serial.
- Kernel tetap FMA dengan gate eksak; bentuk vec4 seperti arm tercepat sebelumnya.
- **Yang diminta adalah BENTUK KURVA, termasuk titik belok** — bukan satu sel tercepat. Wajib laporkan: rata-rata per sel + spread, sel argmax, dan sel pertama tempat kurva turun.
- **Ekspektasi regresi:** ILP tinggi bisa menurunkan performa (register spill). Itu deliverable, bukan kejutan.
- Laporkan **efek utama lsize** dan **efek utama ILP secara terpisah** (jangan campur jadi satu ranking).

### Q3 — Apple-to-apple large-n (wajib, karena belum pernah diukur)

Ukur **OpenCL pada n=1048576** dengan harness yang sama, supaya perbandingan OpenCL-vs-Vulkan di `n` besar punya dasar. Jangan ekstrapolasi dari n=65536.

## 2. Aturan bukti (keras — pelanggaran = hasil ditolak)

1. **Gate eksak wajib di setiap arm:** kernel `z=z*x+y`, x=y=1, z0=0 → `C[0]` harus == iters. Cetak `GATE ... PASS/FAIL`. **Exit non-zero saat FAIL.**
2. **≥3 repeat per sel.** Laporkan mean + min/max. **Tidak ada klaim delta antar-sel kecuali rentangnya tidak tumpang tindih (disjoint).**
3. **Urutan run dicatat.** Setiap baris log memuat index run + nama sel. Panas device menekan sel yang dijalankan belakangan; urutan harus terlihat di log.
4. **Status ramp dicatat.** Setiap arm Vulkan memakai nilai ramp tetap dan mencetak nilai itu. "Ramp=1000 tercetak" ≠ "clock 610 di jendela ukur". Untuk arm Vulkan, sertakan **sampling `cur_freq` di dalam jendela timing** sebagai kontrol positif bila memungkinkan.
5. **Raw log wajib.** Setiap run ter-timing **stream stdout ke file** (`*.raw.txt`), tanpa diedit, termasuk header harness, baris GATE, dan baris RESULT. File raw adalah deliverable kontrak, bukan lampiran opsional.
6. **Verifikasi membaca output mentah program**, bukan ringkasan anggota tim.

## 3. Deliverable

| File | Isi |
|---|---|
| `SPEC.md` | dokumen ini (beku) |
| `oclgrid.c` (+ `.cl` bila perlu) | harness OpenCL: lsize×ILP grid + n besar |
| `vkbatch.c` (+ `.comp`/`.spv`) | harness Vulkan: arm cold / ramp / batch-1-submit / batch-fence-per-rep |
| `q1-vulkan.raw.txt` | stdout mentah semua arm Vulkan, semua repeat |
| `q2-opencl.raw.txt` | stdout mentah semua sel grid OpenCL, semua repeat |
| `q3-large-n.raw.txt` | stdout mentah OpenCL n=1048576 |
| `RESULTS.md` | interpretasi + tabel + status hipotesis H1/H2 |
| `VERIFY.md` | verdict verifier terhadap raw log |

## 4. Build & jalankan (jebakan yang sudah dikenal — jangan diulang)

- Cross-compile bionic: `aarch64-linux-android33-clang -O2 -o <bin> <src>.c -ldl` (OpenCL: link ke `libOpenCL.so`; Vulkan: **tanpa** `-lvulkan`, header NDK saja).
- Binary bionic **wajib** dijalankan lewat linker64 dengan **path absolut**:
  `LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 /apex/com.android.runtime/bin/linker64 /abs/path/bin`
  `linker64 ./bin` gagal (`expected absolute path`); exec langsung gagal (`required file not found`).
- Vulkan: loader Android menemukan 0 device di chroot → `dlopen("/vendor/lib64/hw/vulkan.adreno.so")`, simbol mangled `_ZN11qglinternal21vkGetInstanceProcAddrEP12VkInstance_TPKc`, sisanya via GIPA/GDPA.
- `setvbuf(stdout,0,_IONBF,0)` wajib (stdout block-buffered tampak hang).
- **Jangan tulis `\n` di dalam string literal** lewat write_file (korup); pakai `puts()`.

## 5. Aturan eksekusi tim

- **TEPAT SATU runner GPU.** Semua harness GPU (OpenCL & Vulkan) dijalankan oleh satu anggota (writer/runner) secara **berurutan**. GPU adalah resource contention-sensitive: angka dari dua proses paralel saling merusak.
- **Anggota lain DILARANG memanggil `run_shell_command`** untuk apa pun yang menyentuh GPU/benchmark. Riset & review tidak boleh menjalankan benchmark.
- Satu penulis file kode. Reviewer/verifier tidak mengedit kode.
- `read_only` memblokir write_file, shell, **dan** web_fetch/git-MCP. Anggota yang deliverable-nya FILE, atau yang perlu riset web, **jangan** di-spawn `read_only` — batasi lewat instruksi.
- Setiap peneliti: batasi usaha (~4–6 fetch), **wajib lapor walau hasilnya "tidak konklusif"**, dan akhiri turn setelah lapor.

## 6. Non-goal (eksplisit)

- **Bukan** mencari "angka GFLOPS tercepat", dan **bukan** mengklaim "maksimum hardware Adreno 616".
- **Bukan** menjelajahi ruang di luar Q1–Q3 (workgroup tuning lain, instruction mix, pola memori, state termal lanjut) — itu ronde berikutnya.
- **Bukan** mengubah/menghapus file non-round di `/home/han/adreno616-lab` (`bench.c`, `gles.c`, `vdec*`, `dsp*`, `clprobe*`, `clver`, `clinfo`, `gpufma`, `gpucl`, `fma.comp/spv`, `kernels.S`, dll).

## 7. Kriteria sukses

Ronde ini **sukses** bila:
1. Q1 terjawab dengan tes pembeda R=16 (H1 atau H2 dinyatakan, dengan kontrol fence-per-rep).
2. Q2 menghasilkan tabel 12 sel dengan mean+spread, argmax, dan titik belok — dari raw log.
3. Q3 menghasilkan angka OpenCL n=1048576 dari harness bergate.
4. Semua angka disertai (n, reps, status ramp) dan rentang non-overlapping untuk setiap klaim delta.
5. `VERIFY.md` menyatakan verdict berdasarkan raw log, bukan ringkasan.

Ronde ini **gagal** bila ada angka yang dikutip tanpa gate PASS, tanpa repeat, atau tanpa raw log.

## 8. ADJUDIKASI AKHIR LEAD (2026-10-07) — mengikat

### 8.1 Q1 — H1 TIDAK DIDUKUNG (kontrak menang atas pembacaan longgar)
Dua verifier berbeda. Adjudikasi: **log-verifier benar.** SPEC §1 menetapkan kriteria falsifiable `batch1 n=65536 ≥ ~86 → H1 didukung`; batch1 mencapai **76.282** [74.896, 76.981] — **ambang tidak tercapai**. Yang benar:
- **Arah mekanisme H1 didukung**: batch1 [74.896,76.981] vs batchfp [71.972,72.561] **disjoint** (3 run masing-masing); batching menambah **+3.98 GFLOPS (~5.5%)**. `DISPATCHES_RECORDED=16` terverifikasi.
- **Besaran H1 tidak didukung**: efeknya kecil, bukan lompatan ke ~86+.
- **H2 juga tidak dinyatakan** (batchfp ≈ ramp [71.944,73.076] tumpang tindih → delta ramp-vs-batchfp TIDAK sah).
- **Verdict Q1: TIDAK TERVERIFIKASI** (arah konsisten H1; ambang & sel rujukan n-besar tak diukur).

### 8.2 Q2 — koreksi dua klaim
1. **"Datar terhadap bentuk kernel" SALAH untuk lsize.** Data parsial: lsize 64→128 menurunkan throughput **~40%** (disjoint, nyata). Efek **lsize dominan**; efek **ILP (K=1..8) tidak signifikan**. Klaim awal "kedataran di semua lsize×ILP" dibatalkan.
2. **OpenCL ~0.147 GFLOPS adalah jalur LOADER yang patologis, bukan properti hardware.** Kontrol penutup: gate penuh `FULLBUF_PASS ok=262144` (semua elemen benar, bukan subset) → bukan artefak gate; **CPU 1-thread = 4.958 GFLOPS vs OpenCL 0.1515 GFLOPS → CPU menang ~33x** pada jumlah FMA identik; Vulkan ~48x lebih cepat dari OpenCL pada kerja identik. Kesimpulan: jalur `libOpenCL.so → libCB` di konfigurasi ini gagal mencapai throughput compute GPU. Mekanisme (`maxcu=1`/`type=0x0`) **tersangka, belum dikonfirmasi**.

### 8.3 Worksweep — TERVERIFIKASI, menggantikan klaim "overhead tetap"
wall **skala linear** dengan kerja (iters 2000→200→20 = 7.07→0.71→0.075 s; n 10x turun = wall 10x turun); gflops ~0.147 datar. Klaim sementara gpu-runner "biaya tetap ~7 s/dispatch" **TERBANTAH** (ia melaporkan pembalikan ini sendiri).

### 8.4 Q3 — TIDAK TERVERIFIKASI (dihentikan; `q3-large-n.raw.txt` tak pernah ditulis; bukan FAIL).

### 8.6 Batas klaim ronde
Tidak ada klaim "maksimum hardware Adreno 616". Semua angka adalah karakterisasi workload+konfigurasi+harness pada device ini.

## 9. AMANDEMEN BERTANGGAL 2026-10-08 - 8.2 & 8.5 (append-only; teks 8 di atas TIDAK dihapus)

Konteks: setelah ronde ditutup, user melaporkan agent lain mencapai OpenCL ~90 GFLOPS dengan vec4 di environment yang sama. Investigasi lanjutan menemukan akar angka 0.147. 9 ini MENGGANTIKAN 8.2 butir 1 & 2 dan 8.5; 8.1 (Q1), 8.4 (Q3), 8.6 tetap berlaku.

### 9.1 Temuan terverifikasi: builtin fma() adalah jurang (menggantikan 8.2 butir 2)

Bukti: /home/han/adreno616-lab/oclmech.c, raw log tersimpan di gpu-r1/oclmech.raw.txt (re-run independen lead, semua gate C0==2000 PASS, clock 610 MHz, n=65536, iters=2000, reps=10):

| arm | operasi | GFLOPS |
|---|---|---|
| A | vec4 fma() | 0.150 |
| B | vec4 zv*xv+yv | 100.24 |
| C | vec4 mad() | 100.09 |
| D | vec4 fma() + -cl-fast-relaxed-math | 0.151 |
| E | skalar zv*xv+yv | 37.37 |
| F | skalar fma() | 0.770 |

Jadi 8.2 butir 2 SALAH sebagai sebab: yang gagal bukan jalur libOpenCL.so ke libCB, melainkan codegen kernel yang memakai builtin fma(). Angka 0.147/0.15 (ronde-5) dan plafon 0.94 lama SEMUANYA kernel ber-fma(). Pada lebar vec4, a*b+c / mad() menembus ~100 GFLOPS di harness yang sama. mad() cepat + -cl-fast-relaxed-math tidak menolong fma() -> konsisten dengan emulasi fma correctly-rounded (FFMA Adreno tidak correctly-rounded, sehingga driver menurunkannya ke jalur software).

### 9.2 Mekanisme = HIPOTESIS, belum dibuktikan

Tidak ada dump binary/disassembly yang diperiksa. ~600x terlalu besar untuk cerita devectorization (~4x), tapi tetap: nyatakan OBSERVASI (jurang fma() ~600x, tak bergantung lebar: skalar 0.770 vs 37.37) sebagai terverifikasi, dan MEKANISME emulasi sebagai hipotesis dengan dump binary sebagai cek (CL_PROGRAM_BINARY_SIZES 0x1165 dulu, baru 0x1166).

### 9.3 8.2 butir 1 (lsize dominan / ILP nol) IKUT DIBATALKAN

Efek lsize 64 ke 128 (~40%) diukur oleh oclgrid.c - yang MEMAKAI fma(). Maka kurva itu properti jalur emulasi, bukan bentuk kernel. Verdict "lsize dominan, ILP nol" TIDAK SAH. 8.3 "datar terhadap bentuk kernel" gugur untuk alasan yang sama. Grid ILP OpenCL di jalur cepat BELUM PERNAH dijalankan.

### 9.4 Ranking Vulkan vs OpenCL = UNRESOLVED

OpenCL ~100 GFLOPS diukur pada K=1 (satu akumulator); Vulkan ~112.7 diukur pada K=2 ILP. Itu bukan config yang sama. JANGAN menyatakan "Vulkan > OpenCL" atau sebaliknya dari data ini. Tes penentu (belum dijalankan): ubah satu token fma( menjadi z*x+y di oclgrid.c, jalankan ulang grid 12-sel lsize x ILP di jalur cepat.

### 9.5 8.5 dilonggarkan (menggantikan larangan mutlak)

Angka ~96-100 GFLOPS vec4 kini DIREPRODUKSI di harness yang masih ada di disk (oclmech.c, oclmech2.c, oclws_muladd), gate eksak PASS. Larangan berubah menjadi: jangan kutip GFLOPS tanpa (n, reps, bentuk kernel, status ramp, dan apakah kernel memakai builtin fma()). Entri memory lama (skalar ~0.86, vec4 ~96) tidak salah - hanya salah label bentuk kernel.

### 9.6 Dua kontrol yang belum ditutup

1. oclmech.c menggate hanya C0 (work-item 0), bukan seluruh buffer - kontrol buffer-penuh (FULLBUF_PASS ok=262144) ada hanya untuk arm fma() di oclfull.c. Perbandingan RELATIF antar-arm tetap aman, tapi jangan klaim verifikasi buffer-penuh untuk arm B/C/E.
2. Efek kontraksi otomatis a*b+c menjadi fma oleh compiler di jalur OpenCL ini BELUM diuji.

### 9.7 Yang TIDAK berubah oleh 9

- 8.1: Q1 tetap TIDAK TERVERIFIKASI (arah mekanisme saja; ambang batch1 >= ~86 tak tercapai).
- 8.4: Q3 tidak pernah dijalankan; q3-large-n.raw.txt tak ada.
- 8.6: tidak ada klaim "maksimum hardware Adreno 616".
- Verdict "jalur OpenCL patologis" sebagai pernyataan tentang LOADER/DRIVER/HARDWARE DIBATALKAN; yang patologis adalah CODEGEN fma() di jalur OpenCL.
