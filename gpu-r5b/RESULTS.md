# gpu-r5b — kontrol satu-variabel untuk B5: live range melintasi loop

2026-10-19. Adreno 616, L=64, n=65536, iters=2000, reps=10, ramp=500, R=1,
harness `vkopt8` (gate full-buffer + VkResult per rep), clock 610 MHz di semua run.

## Pertanyaan

r5 menyimpulkan B5 (~92.7) < K=2 (~111.0) dan menyebut selisihnya "efek codegen,
mekanisme UNKNOWN". Apakah itu benar-benar batas pengetahuan, atau ada satu variabel
yang belum diisolasi?

## Sel

| Tag | Isi | Loop body | Live range melintasi loop |
|---|---|---|---|
| `k2` | 2 acc, 2 FMA | 2 FMul + 2 FAdd, store a0,a1 | tidak ada |
| `b5` | 2 acc + `a2=yv`, `a3=a2+yv` didefinisikan **sebelum** loop | identik k2 | **ada** (a2, a3) |
| `b5b` | math & epilog **identik** b5, `a2`/`a3` didefinisikan **setelah** loop | identik k2 | **tidak ada** |

Ketiga sel lulus gate eksak (`nbad=0`, 15/15 run), jadi output ketiganya identik secara
numerik. `b5b.spv` lolos `spirv-val`; `OpStore %a2`/`%a3` berada di baris 148/152,
`OpLoopMerge` di baris 119 — yaitu sesudah loop, bukan sebelumnya.

**b5 vs b5b adalah satu variabel: ada/tidaknya nilai yang hidup melintasi loop.**
Bukan jumlah FMA, bukan jumlah op, bukan kerja loop, bukan epilog.

## Hasil (interleaved, 5 ronde, urutan k2/b5/b5b per ronde)

| Tag | 5 pengukuran (GFLOPS) | mean | min–max |
|---|---|---:|---|
| k2 | 111.920 110.743 111.038 112.443 110.831 | 111.40 | 110.74–112.44 |
| b5 | 93.558 91.669 90.111 93.586 91.777 | 92.14 | 90.11–93.59 |
| b5b | 104.390 104.764 105.546 104.574 105.421 | 104.94 | 104.39–105.55 |

Rentang ketiga sel tidak bertumpang tindih pada ronde mana pun.

## Dekomposisi selisih B5

- k2 − b5 = **19.26 GFLOPS**
- b5b − b5 = **12.80 GFLOPS** → 66% dari kehilangan B5 berasal dari live range melintasi loop
- k2 − b5b = **6.46 GFLOPS** (5.8%) → sisa, **belum teratribusi**

## Verdict

1. **Bukan bug pengukuran.** Gate eksak, FLOP count benar, interleaved, spread sempit,
   clock terkunci.
2. **Ada lubang logika di interpretasi r5**, bukan di datanya: r5 berhenti di
   "codegen, mekanisme UNKNOWN" padahal satu kontrol satu-variabel memisahkannya.
3. **Mekanisme B5 sekarang teridentifikasi:** biayanya bukan "jumlah register hidup"
   melainkan **rentang hidup yang melewati loop**. Menghapus crossing itu memulihkan
   ~2/3 throughput tanpa mengubah satu pun operasi loop maupun hasil.
4. **B5 tidak lagi boleh disebut mekanisme UNKNOWN.** Frasa yang benar:
   *B5 membayar ~12.8 GFLOPS karena dua nilai dijaga hidup melintasi loop.*
5. **Yang tetap UNKNOWN:** sisa 5.8% (b5b vs k2). Kandidat yang belum dipisah:
   kerja epilog tambahan vs codegen/regalloc di scope fungsi.
6. **Yang tidak berubah:** kurva K. K=3/K=4 tidak punya nilai loop-invariant — semua
   accumulator benar-benar di-update di dalam loop — jadi penurunan K=3 (~84) tetap
   fenomena tersendiri dan tidak dijelaskan oleh hasil ini.

## Batas

Semua angka terikat konfigurasi di atas. B5/b5b/k2 diukur di proses terpisah,
bergantian, 5 ronde. Perbandingan b5b vs k2 tidak sepenuhnya satu-variabel (b5b punya
2 FAdd epilog lebih banyak); angka 5.8% karena itu disebut residual, bukan efek terukur.

## Diskriminator berikutnya (belum dijalankan)

`b5c`: seperti b5b tetapi dengan 4 konstanta epilog-only tambahan. Jika tetap ~104.9,
residual bukan biaya kerja epilog. Jika turun, epilog memang berkontribusi.

## ITERS SWEEP (2026-10-19) — dekomposisi slope/intercept

45/45 gate PASS. k2/b5/b5b x iters{250..4000} x 3 ronde. Fit `wall_ms = a + b*iters`
(least squares pada iters 1000/2000/4000, median per titik).

| tag | slope ms/1000it | vs k2 | intercept ms |
|---|---:|---:|---:|
| k2 | 8.932 | — | 0.995 |
| b5 | 10.743 | +20.3% | 1.090 |
| b5b | 9.515 | +6.5% | 1.043 |

**Intercept ~sama** (0.995/1.090/1.043 ms, selisih <=0.1 ms = 0.5% dari run 2000it).
**Seluruh penalti ada di slope** (steady-state per-iterasi).

Dekomposisi slope: b5-k2 = 1.811 ms/1000it; b5b-k2 = 0.583; jadi b5->b5b memulihkan
1.228/1.811 = **67.8%** dari penalti b5. Konsisten dengan dekomposisi GFLOPS (66%).

### Koreksi yang dipaksa oleh sweep ini

1. Residual 5.8% (b5b vs k2) **tidak mungkin kerja epilog**. Epilog dieksekusi sekali per
work-item, jadi biayanya iters-independent -> harus muncul di intercept, bukan slope.
Intercept ketiga sel sama. Maka residual juga berada **di dalam loop body** (codegen),
bukan di aritmetika epilog.
2. Konsekuensinya **b5c sebagai rancangan awal bukan eksperimen**: menambah konstanta
epilog-only hanya akan menggeser intercept. Rancangan itu dibatalkan.
3. Tiga sel dengan loop body **identik di SPIR-V** menghasilkan tiga slope berbeda
(8.93 / 10.74 / 9.52). Jadi "loop body identik di SPIR-V" tidak berarti "loop body
identik di machine code": apa pun yang ada di fungsi lain ikut mengubah loop.

### Yang positif terbukti

- Tidak ada komponen biaya launch/setup: intercept sama.
- Penalti B5 adalah fenomena **steady-state loop codegen**, bukan biaya sekali-jalan.
- Nilai yang hidup melintasi loop menyumbang ~2/3; ~1/3 tersisa bahkan setelah nilainya
didefinisikan di luar loop (b5b) — konsisten dengan register allocation per-fungsi.

### Batas

Slope fit dari 3 titik (1000/2000/4000); titik 250/500 punya spread besar (b5b it=250
32%) dan tidak dipakai untuk fit. Intercept pada iters kecil tidak dapat dipisahkan dari
ramp/wave-start, jadi "intercept sama" hanya klaim pada resolusi ~0.1 ms.
