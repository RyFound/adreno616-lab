# adreno616-lab

Benchmark dan probe GPU untuk **Adreno 616** (Snapdragon 712) yang dijalankan langsung di
device, di dalam chroot Ubuntu, dengan biner bionic. Isinya: harness Vulkan compute, harness
OpenCL, probe SoC (DSP/video/camera), dan benchmark CPU/RAM/disk sebagai pembanding.

Setiap angka di repo ini adalah **throughput pada konfigurasi tertentu**, bukan plafon hardware.
Semua sel menyertakan gate validasi dan raw log mentah; klaim yang tidak lolos gate tidak dipakai.

---

## 1. Kenapa repo ini aneh

Jalur normal "install driver lalu pakai" tidak tersedia di device ini, jadi semuanya diakali:

| Masalah | Kenyataan di device | Yang dipakai di sini |
|---|---|---|
| Shell lokal adalah chroot Ubuntu (glibc), GPU butuh bionic | `/system/bin` tidak ada di chroot | biner bionic dijalankan via `/apex/com.android.runtime/bin/linker64` (path absolut wajib) |
| Loader Vulkan Android menemukan **0 physical device** di chroot | tidak ada `icd.d/*.json`; `VK_ICD_FILENAMES` tidak menolong | `dlopen("/vendor/lib64/hw/vulkan.adreno.so")` + simbol C++ mangled di namespace `qglinternal` |
| Mesa freedreno/turnip tidak bisa dipakai | kernel 4.9 downstream hanya punya KGSL, tanpa MSM render UAPI | tidak dipakai sama sekali; GL/EGL glibc selalu jatuh ke llvmpipe |
| OpenCL | `/vendor/lib64/libOpenCL.so` cuma shim | shim → `libCB.so`, compiler `libllvm-glnext.so` |

Konsekuensi praktis: **semua biner di repo ini adalah aarch64 bionic**, bukan glibc, dan tidak
berguna di mesin lain. Karena itu build output tidak di-commit (lihat §6).

---

## 2. Hasil utama

Semua angka dari `RESULTS.md` di ronde masing-masing, gate PASS, clock 610 MHz.

### Vulkan compute — bentuk kernel

n=65536, L=64, ramp 500 ms, harness `vkopt8` (gate full-buffer + `VkResult` per rep).

| Sel | Akumulator | FMA/iterasi | GFLOPS |
|---|---:|---:|---:|
| K=1 | 1 | 1 | ~81.4 |
| **K=2** | 2 | 2 | **~111.0** |
| K=3 | 3 | 3 | ~84.6 |
| K=4 | 4 | 4 | ~86.3 |
| K=8 | 8 | 8 | ~86.0 |
| B5 | 4 | 2 | ~92.7 |
| B4 | 4 | 4 | ~86.1 |

**Puncak tajam di K=2**, bukan plato. K=2/K=1 ≈ 1.36–1.44 dan spike itu bertahan di semua
local_size yang dicoba (L=32/64/128), jadi efeknya per-thread, bukan per-workgroup.

Mekanisme spike = **dua FMA independen per iterasi (ILP)**, dibuktikan dengan snowplow
satu-variabel: menambah akumulator hidup tanpa menambah FMA tidak menaikkan throughput
(75.4 vs ~112.4), sedangkan 2 FMA/iterasi dengan akumulator hidup tetap 2 mengembalikan spike penuh.

### B5 — penalti "nilai hidup melintasi loop"

`r5` menyebut selisih B5 vs K=2 sebagai "efek codegen, mekanisme UNKNOWN". `r5b` mengisolasinya
jadi satu variabel: math dan epilog identik, hanya **posisi definisi** `a2`/`a3` yang dipindah dari
sebelum loop ke sesudahnya (`OpStore` baris 148/152 vs `OpLoopMerge` baris 119).

| Sel | Live range melintasi loop | GFLOPS |
|---|---|---:|
| k2 | tidak ada | 111.40 |
| b5 | ada | 92.14 |
| b5b (math identik, definisi setelah loop) | tidak ada | 104.94 |

5 ronde interleaved, rentang tidak bertumpang tindih. Memindahkan definisi memulihkan **12.80 GFLOPS
(66% selisih)**. Dekomposisi `iters`-sweep menunjukkan seluruh penalti ada di **slope**
(+20.3% vs +6.5%), bukan di intercept — jadi bukan biaya prolog/epilog.

### OpenCL — jangan pakai `fma()`

| Kernel (vec4) | GFLOPS |
|---|---:|
| `fma()` | 0.148 |
| `a*b+c` | 99.69 |
| `mad()` | 99.87 |

Jurang **~675x**. `mad()` (boleh non-correct-rounding) secepat mul+add; `-cl-fast-relaxed-math`
tidak menolong `fma()`. Binary program `fma()` 2.02x lebih besar (5272 B vs 2616 B), jadi jalur
compiler-nya memang berbeda. Hipotesis mekanismenya (emulasi correctly-rounded) **belum dibuktikan**.

### CPU / RAM / disk (pembanding)

| Suite | Angka |
|---|---|
| NEON FMA 8-thread | ~12–13.6 GFLOPS |
| NEON FMA 1-thread | 3.02 GFLOPS |
| scalar | 0.91 GFLOPS |
| RAM read 8-thread | ≥24.7 GB/s |
| RAM write 8-thread | 17.0 GB/s |
| RAM copy 8-thread | 8.0 GB/s payload |

RAM single-thread read ~9–10 GB/s adalah **core-limited** (satu LSU), bukan batas bus. Device tidak
mengekspos peak DRAM yang bisa dibaca: devfreq `cpubw` max 6881 MB/s itu QoS vote, bukan silicon peak.

---

## 3. Yang TIDAK diklaim

Ini bagian yang sering dilewatkan orang saat membaca benchmark, jadi ditulis eksplisit:

1. **118 GFLOPS bukan ceiling.** Itu satu titik di (n≈262144–1048576, K=2, L=64, tanpa fma).
   Sumbu instruction-mix dan memori belum dijelajahi.
2. **Angka bergantung DVFS.** Clock ramp per-proses; proses dingin bisa membaca ~2x lebih rendah.
   Semua sel di sini memakai warm-up dispatch nyata 500 ms dan mencatat `cur_freq_after`.
3. **Local_size ter-bake di SPIR-V.** Setiap L butuh shader sendiri; tidak bisa diubah saat runtime.
4. **Dispatch panjang bisa parsial.** Pada n=1M K=16, ekor buffer kontigu tidak tertulis
   (~713.000 work-item tetap). Gate full-buffer yang menangkapnya; gate satu elemen tidak.
5. **"matched K" ≠ "matched work".** OpenCL membagi `iters`, Vulkan mereplikasi. Perbandingan
   cross-runtime di repo ini selalu menyebut konfigurasi lengkapnya.
6. **Mekanisme di level codegen tidak bisa diukur** di device ini: tidak ada hardware counter
   lewat sysfs KGSL, tidak ada `fdperf`, `msm_kgsl.h` tidak ada di disk. Klaim berhenti di
   "terisolasi oleh A/B satu variabel", tidak naik ke "mekanisme teridentifikasi".

Satu pekerjaan lanjutan (rekonstruksi ABI compiler shader Adreno lewat disassembly blob
proprietary) sengaja **tidak** ada di repo ini — lihat §7.

---

## 4. Struktur

```
src/
  mem/       bench.c kernels.S build.sh   benchmark RAM + disk (asm NEON tangan)
  opencl/    clinfo.c clprobe*.c clver.c  probe OpenCL: platform, device, versi, binary
             gpucl.c oclmech*.c oclws*.c  harness OpenCL (fma vs mul+add, grid, workgroup)
  vulkan/    vkcomp.c gpufma.c            harness Vulkan awal
  gles/      gles.c                       probe OpenGL ES
  soc/       socprobe.c dsp*.c            probe SoC dan DSP Hexagon
  video/     vdec*.c vidprobe.c           probe decode H.264 via V4L2
  misc/      hello.c                      smoke test

rounds/
  r1/   ruang konfigurasi: batch-vs-fence, OpenCL lsize-ILP   (SPEC/REVIEW/RESULTS/VERIFY)
  r2/   strategi eksekusi: representasi, compiler, bentuk kernel
  r3/   pembeda spike K=2 (L-sweep) + jalur fma()
  r4/   eliminasi mekanisme spike + batas dispatch
  r5/   matriks register-hidup x independent-FMA (B4/B5)
  r5b/  kontrol satu-variabel untuk B5 (definisi sebelum vs sesudah loop)

shaders/   .comp contoh di root (fma.comp, fma8.comp)
docs/      catatan temuan
logs/      output probe yang direkam
assets/    test vector pihak ketiga (tidak di-track)
build/     hasil build (tidak di-track)
```

Round run scripts resolve their own directory (cd dirname 0), so a checkout works
from any path - no absolute path is baked in.

Setiap ronde punya `RESULTS.md` (angka + adjudikasi) dan raw log mentah. `r1` juga punya
`SPEC.md`/`REVIEW.md`/`VERIFY.md` karena dijalankan sebagai ronde berkontrak.

---

## 5. Build & jalankan

### Prasyarat

```
NDK      /opt/android/android-sdk/ndk/30.0.14904198
SPIR-V   glslangValidator, spirv-val, spirv-dis   (glslc tidak ada di container ini)
Device   Android dengan Adreno 616, akses root, /vendor ter-mount di chroot
```

### Benchmark RAM/disk (host native)

```sh
./src/mem/build.sh  # -Os + flag size, kernel HOT di-O3 lewat __attribute__
./build/bench mem         # sweep cache 8 KB -> 256 MB, thread scaling, pointer-chase
./build/bench disk ./bench.bin
```

### Harness GPU (bionic, cross-compile)

```sh
CC=/opt/android/android-sdk/ndk/30.0.14904198/toolchains/llvm/prebuilt/linux-arm64/bin/aarch64-linux-android33-clang
$CC -O2 -o build/vkopt8 rounds/r4/vkopt8.c -ldl

# jalankan dari chroot; path biner HARUS absolut
LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 \
  /apex/com.android.runtime/bin/linker64 /path/absolut/vkopt8 2000 65536 10 64 2 ramp
```

Jebakan yang sudah memakan waktu dan karena itu dicatat:

- **Path biner wajib absolut.** `linker64 ./x` gagal `expected absolute path`.
- **stdout block-buffered** → program tampak hang. Panggil `setvbuf(stdout,0,_IONBF,0)`.
- Driver Adreno di-`dlopen` **langsung**; simbolnya C++ mangled (`_ZN11qglinternal21vkGetInstanceProcAddrE...`),
  bukan `vkGetInstanceProcAddr` polos. Link tanpa `-lvulkan`.

### SPIR-V

`.spv` tidak di-commit (lihat §6). Regenerasi dari `.comp`:

```sh
glslangValidator -V -o out.spv in.comp && spirv-val out.spv
```

Tiga ronde punya generator yang membuat semua selnya dari satu template (provenance seragam):
`rounds/r2/gen_vk_shaders.py`, `rounds/r3/gen_grid.py`, `rounds/r5/gen_r5.py`.

**Batasan yang perlu diketahui:** dari 55 `.spv` di tree asli, **14 tidak punya `.comp` senama** —
file itu dihasilkan generator atau sudah dihapus. Artinya tidak semua sel bisa di-rebuild persis
dari isi repo ini; sel yang `.comp`-nya ada bisa, sisanya hanya raw log-nya yang tersisa.

---

## 6. Kenapa binary tidak di-commit

`.gitignore` di repo ini memakai pola **allowlist**, bukan denylist: semuanya di-ignore dulu,
lalu hanya source/doc/log yang di-un-ignore.

Alasannya konkret: tree asli menumpuk **39 binary ELF tanpa ekstensi** dan **55 blob `.spv`**.
Denylist nama binary akan basi begitu ronde berikutnya menambah `vkopt10`; allowlist tidak bisa
bocor karena kelupaan. Konsekuensinya: **jenis file baru akan ter-ignore sampai ditambahkan ke
`.gitignore`** — kalau ada file yang hilang dari `git ls-files`, cek daftar itu dulu.

Yang sengaja tidak masuk: `*.spv` (regenerate dari `.comp`), `*.bin` (binary program OpenCL,
regenerate dengan menjalankan `oclbin`), `foreman.264` (test vector H.264 pihak ketiga dengan
lisensi tidak jelas), dan semua biner hasil build.

---

## 7. Lisensi & cakupan

Kode di sini ditulis sendiri. Tidak ada blob proprietary, firmware, atau materi Qualcomm yang
di-commit. Pekerjaan reverse-engineering terhadap blob compiler Adreno (disassembly, rekonstruksi
ABI, spesifikasi perilaku) **tidak** ada di repo ini dan disimpan terpisah.

Kalau kamu menjalankan ini di device lain: semua angka di sini terikat ke satu unit Adreno 616
dengan clock 610 MHz. Jangan pindahkan angkanya ke SoC lain tanpa mengukur ulang — DVFS,
ukuran cache, dan lebar bus berbeda, dan justru itu yang paling sering membuat benchmark
GPU menyesatkan.
