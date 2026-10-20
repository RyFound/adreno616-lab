# gpu-r5 — matriks register-hidup x independent-FMA (B4/B5)

2026-10-19. Adreno 616, clock 610 MHz (`cur_freq_after=610000000` di semua sel), chroot,
harness `vkopt8` (gate FULL-BUFFER `nbad`/`nzero`/`firstbad`/`lastbad` + VkResult submit/wait
per rep). Raw: `r5_b4b5.raw.txt`. Generator: `gen_r5.py` (satu template untuk keempat sel,
provenance seragam). Compile: `glslangValidator -V`, semua lolos `spirv-val`.

Konfigurasi: L=64, n=65536, iters=2000, reps=10, ramp=500, R=1. 3 invokasi per sel (r1..r3).

## Tujuan

Memisahkan, pada ACC=4, kontribusi jumlah nilai hidup tambahan dari jumlah rantai FMA
independen, dan menguji apakah K=2 (2 acc/2 FMA) sudah menyaturasi execution resource.

Matriks: 1acc/1FMA (baseline), 2acc/1FMA (B2, liveness), 2acc/2FMA (K=2, ILP),
4acc/2FMA (B5), 4acc/4FMA (B4).

## Provenance SPIR-V (bukti satu template)

| Sel | ACC | FMAS | OpFMul | OpFAdd |
|---|---:|---:|---:|---:|
| r5_k1 | 1 | 1 | 1 | 1 |
| r5_k2 | 2 | 2 | 2 | 3 |
| r5_b5 | 4 | 2 | 2 | 6 |
| r5_b4 | 4 | 4 | 4 | 7 |

OpFMul = FMAS, konsisten dengan `fadd = 2K-1` untuk sel kernel-K. Tidak ada OpExtInst Fma.

## Hasil

| Sel | ACC | FMA/iter | GFLOPS (3 invokasi) | mid | gate |
|---|---:|---:|---|---:|---|
| r5_k1 | 1 | 1 | 81.019 / 81.884 / 81.350 | ~81.4 | PASS |
| r5_k2 | 2 | 2 | 111.445 / 109.859 / 111.050 | ~111.0 | PASS |
| r5_b5 | 4 | 2 | 92.297 / 92.796 / 93.046 | ~92.7 | PASS |
| r5_b4 | 4 | 4 | 86.128 / 85.994 / 86.192 | ~86.1 | PASS |

Semua 12 gate `nbad=0`; tidak ada FAIL; tidak ada `submit`/`wait` != 0; tidak ada wall≈0.

## Dua poros satu-variabel

1. **Fixed 2 FMA, ACC 2 -> 4:** ~111.0 (K=2) -> ~92.7 (B5).
2. **Fixed ACC=4, FMA 2 -> 4:** ~92.7 (B5) -> ~86.1 (B4).

## AMANDEMEN (2026-10-19): koreksi interpretasi B5

**Bukti baru dari disassembly SPIR-V.** Blok loop `r5_b5` dibandingkan dengan `r5_k2`:

- Loop body B5 identik strukturnya dengan K=2: 2 `OpLoad xv`, 2 `OpLoad yv`,
  2 `OpFMul`, 2 `OpFAdd`, 2 `OpStore` (a0, a1). Tidak ada operasi tambahan di dalam loop.
- `a2` (inisialisasi `yv`) dan `a3` (`a2 + yv`) adalah **loop-invariant**; keduanya hanya
  dipakai di epilog (`+a2+a3-3.0`).

Konsekuensi yang harus dikoreksi dari draf pertama dokumen ini:

1. `a2`/`a3` **bukan accumulator**. Mereka adalah **register hidup tambahan** yang nilainya
   harus dipertahankan melintasi loop. Istilah "biaya accumulator" untuk B5 **dicabut**.
2. Selisih 111.0 -> 92.7 **tidak mungkin** berasal dari kerja loop (identik). Ia harus berasal
   dari **codegen**: alokasi register / scheduling / occupancy yang berbeda karena adanya nilai
   hidup tambahan. **Mekanisme persisnya UNKNOWN** — tidak ada pengukuran register count atau
   occupancy di r5.
3. Klaim "biaya accumulator hidup nyata dan terukur" diturunkan menjadi: *nilai hidup tambahan
   berkorelasi dengan throughput lebih rendah; mekanismenya belum diverifikasi.*

Catatan: hal yang sama berlaku untuk B2 di r4 — kontrol "liveness" itu adalah *hint* ke compiler,
bukan jaminan register pressure naik. Kesimpulan E8 (ILP, bukan jumlah accumulator) tetap utuh,
karena snowflake-nya ada di B2 vs B3 (1 FMA vs 2 FMA dengan acc hidup sama), bukan di B5.

## Adjudikasi prediksi (termasuk cabang keempat)

| Prediksi yang dinyatakan sebelum run | Hasil | Status |
|---|---|---|
| 4acc+2FMA ≈ 2acc+2FMA | 92.7 vs 111.0 | **TIDAK terpenuhi** |
| 4acc+4FMA ≈ 2acc+2FMA | 86.1 vs 111.0 | **TIDAK terpenuhi** |
| 4acc+4FMA > 2acc+2FMA | 86.1 < 111.0 | **TIDAK terpenuhi** |
| **B4 < B3 -> kandidat pressure/occupancy, serupa band K=4, dilaporkan sebagai regresi** | 86.1 vs 111.0, dan 86.1 ≈ band K=4 (86.3 di E7) | **TERPENUHI — ini cabang yang kena** |

Kesimpulan adjudikasi: yang terjadi bukan "saturasi datar", melainkan **puncak di 2 rantai**
diikuti penurunan. Empat rantai lebih buruk daripada dua, jadi r5 **tidak dapat** membedakan
"resource jenuh" dari "spill/pressure". Frasa yang dipakai: **puncak di 2 rantai; di atasnya
turun; mekanisme belum terselesaikan** — bukan "saturasi terbantah".

## Kendala urutan yang harus dijelaskan mekanisme apa pun

B5 (~92.7) **lebih tinggi** dari K=3 (~84.3) dan K=4 (~86.3). Artinya 2 rantai + 2 register hidup
tambahan mengalahkan 3 rantai. Ini melemahkan penjelasan "biaya jumlah register murni" dan
mendukung "jumlah rantai dominan, puncak di 2" — tetapi tetap menyisakan mekanisme tak terjelaskan.

## Konsistensi internal

B4 (~86.1) jatuh di band K=4 kurva E7 (~86.3), dan struktur SPIR-V-nya (4 FMul, 7 FAdd) persis
baris K=4 di E5. Karena itu **B4 pada dasarnya replikasi K=4**, bukan mekanisme baru: nilainya
adalah kontrol provenance. Informasi baru r5 praktis ada di **B5** dan di konfirmasi tersebut.

## Status klaim

Terbukti (r1-r5):
- E8 memisahkan "2 accumulator hidup" dari "2 independent FMA"; B2 tanpa spike, B3 spike kembali.
- Puncak tajam di 2 rantai FMA independen; menambah kerja independen (B4) maupun register hidup
  tambahan (B5) sama-sama menurunkan throughput di bawah K=2.
- B5 > K=3 dan B5 > K=4 (urutan yang harus dijelaskan mekanisme apa pun).
- Struktur SPIR-V linier terhadap K; spike tidak hidup di SPIR-V.
- B5 loop body identik dengan K=2; seluruh selisih op berada di epilog.

Belum terbukti / UNKNOWN:
- Mekanisme penurunan di B5 (111.0 -> 92.7) dan di B4. Kandidat: register pressure, occupancy,
  instruction scheduling. Tidak ada bukti register count atau occupancy di r5.
- Mekanisme penurunan K=3 (~84) dan bentuk non-monoton K=16 > K=3.
- Mekanisme jurang fma() OpenCL (~0.15 vs ~100 GFLOPS). Tetap UNKNOWN.
- Penyebab pemotongan dispatch panjang: "time budget ~2 s" masih hipotesis; yang terbukti hanya
  GPU tidak menulis seluruh output (firstbad ≈ 2.85M float, ~713K work-item selesai, ~2 s estimasi).

## Batas

Semua angka terikat konfigurasi (L=64, n=65536, iters=2000, reps=10, ramp=500, R=1).
~111.0 bukan plafon hardware; 117.9 (n=1M K=2) tetap best observed lintas ronde.
Selisih B5 vs K=2 tidak boleh diatribusikan ke loop work — loop-nya identik.
