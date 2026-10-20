# adreno616-lab

A measurement lab for the **Adreno 616** GPU in a Snapdragon 712 device, run on-device from an
Ubuntu chroot using bionic (Android) binaries. It contains Vulkan compute and OpenCL harnesses,
SoC probes (DSP, video decode, OpenGL ES), and CPU / RAM / storage benchmarks used as baselines.

> **Status:** research archive, not a library. Each round is a frozen experiment with its own
> documents and unedited raw logs. There is no API-stability promise; the value of the repo is the
> evidence, not a reusable SDK.

Every number here is a throughput **measured at a specific configuration** — never a hardware
ceiling. Every cell carries a validation gate and an unedited raw log; cells that fail the gate are
not reported at all.

---

## 1. Why this repository is unusual

The normal path — install a driver, use it — does not exist on this device, so every layer is
worked around:

| Obstacle | Reality on the device | What this repo does instead |
|---|---|---|
| The local shell is an Ubuntu chroot (glibc); the GPU needs bionic | `/system/bin` is absent inside the chroot | bionic binaries are launched through `/apex/com.android.runtime/bin/linker64` (absolute path required) |
| The Android Vulkan loader finds **0 physical devices** inside the chroot | no `icd.d/*.json`; `VK_ICD_FILENAMES` does not help | `dlopen("/vendor/lib64/hw/vulkan.adreno.so")` plus the C++ mangled symbols in namespace `qglinternal` |
| Mesa freedreno / turnip are unusable | the downstream 4.9 kernel exposes KGSL only, with no upstream MSM render UAPI | not used at all; glibc GL/EGL always falls back to llvmpipe |
| OpenCL | `/vendor/lib64/libOpenCL.so` is a shim, not an ICD | shim → `libCB.so`, compiler `libllvm-glnext.so` |

**Practical consequence:** every binary produced here is aarch64 bionic. It will not run on a normal
desktop, which is why build output is not committed (see §7).

---

## 2. Results

All cells below ran at GPU clock 610 MHz, with `cur_freq_after` recorded per run and the validation
gate passing.

### Vulkan compute — kernel shape

n=65536, `local_size` L=64, 500 ms warm-up, harness `vkopt8` (full-buffer gate plus `VkResult` per
rep). Values are the per-round medians: K=1, K=2, B5 and B4 from `rounds/r5`, K=3 and K=4 from
`rounds/r3`, K=8 from `rounds/r2`.

| Cell | Accumulators | FMA / iteration | GFLOPS |
|---|---:|---:|---:|
| K=1 | 1 | 1 | ~81.4 |
| **K=2** | 2 | 2 | **~111.0** |
| K=3 | 3 | 3 | ~84.6 |
| K=4 | 4 | 4 | ~86.3 |
| K=8 | 8 | 8 | ~86.0 |
| B5 | 4 | 2 | ~92.7 |
| B4 | 4 | 4 | ~86.1 |

**The peak at K=2 is sharp, not a plateau.** The K=2/K=1 ratio is 1.36–1.44 and it holds at every
`local_size` tried (L=32/64/128), so the effect is per-thread rather than per-workgroup.

The mechanism is **two independent FMAs per iteration (ILP)**, established by a one-variable
snowplow: adding live accumulators without adding FMAs does not raise throughput (75.4 vs ~112.4),
while 2 FMAs per iteration with the live-accumulator count held at 2 restores the full spike.

### B5 — the cost of a live range that crosses the loop

`r5` described the B5-versus-K=2 gap as "a codegen effect, mechanism UNKNOWN". `r5b` reduced it to a
single variable: identical math and identical epilogue, with only the **definition site** of `a2`
and `a3` moved from before the loop to after it (`OpStore` at lines 148/152 versus `OpLoopMerge` at
line 119).

| Cell | Live range across the loop | GFLOPS |
|---|---|---:|
| k2 | none | 111.40 |
| b5 | present | 92.14 |
| b5b (identical math, defined after the loop) | none | 104.94 |

Five interleaved rounds, non-overlapping ranges. Moving the definitions recovers **12.80 GFLOPS,
66% of the gap**. An `iters` sweep puts the entire penalty in the **slope** (+20.3% vs +6.5%), not
the intercept — so it is not prologue or epilogue cost.

### OpenCL — do not use builtin `fma()`

| Kernel (vec4) | GFLOPS |
|---|---:|
| `fma()` | 0.148 |
| `a*b+c` | 99.69 |
| `mad()` | 99.87 |

A **~675x** cliff. `mad()` (permitted to be non-correctly-rounded) is as fast as mul+add, and
`-cl-fast-relaxed-math` does not rescue `fma()`. The program binary for `fma()` is 2.02x larger
(5272 B vs 2616 B), so the compiler genuinely takes a different path. The explanation for *why*
(correctly-rounded emulation) remains a **hypothesis, not a measured mechanism**.

### CPU / RAM / storage baselines

| Suite | Result |
|---|---|
| NEON FMA, 8 threads | ~12–13.6 GFLOPS |
| NEON FMA, 1 thread | 3.02 GFLOPS |
| scalar | 0.91 GFLOPS |
| RAM read, 8 threads | ≥24.7 GB/s |
| RAM write, 8 threads | 17.0 GB/s |
| RAM copy, 8 threads | 8.0 GB/s payload |

Single-threaded RAM read at ~9–10 GB/s is **core-limited** (one load/store unit), not a bus limit.
The device does not expose a readable DRAM peak: the devfreq `cpubw` maximum of 6881 MB/s is a
Qualcomm QoS vote, not silicon peak.

---

## 3. Measurement methodology

These rules exist because each of them caught a wrong number at some point. A cell that violates any
of them is discarded, not averaged.

1. **Full-buffer gate.** After every timed run the whole output buffer is checked (`nbad`, `nzero`,
   `firstbad`, `lastbad`). A single-element gate is insufficient: long dispatches can execute
   *partially*, leaving a contiguous unwritten tail that a one-element check never sees.
2. **`VkResult` per submit and per wait.** An ignored failed submit makes `wall ≈ 0` and produces
   impossible throughput. Cells with `wall ≈ 0` are thrown away.
3. **Real warm-up.** Clock ramp is per-process and a cold process can read roughly 2x low, so every
   cell runs a real 500 ms dispatch warm-up and records the clock afterwards.
4. **One variable per comparison.** Attribution claims come from A/B pairs that differ in exactly
   one thing (FMA count, live-range position, local size).
5. **SPIR-V provenance.** Cells that form a curve are regenerated from a single template, then
   checked with `spirv-dis` (op counts, loop-body contents) so the curve is not a generator artefact.

---

## 4. What is explicitly not claimed

This section exists because benchmark readers routinely over-read results:

1. **118 GFLOPS is not a ceiling.** It is one point at (n≈262144–1048576, K=2, L=64, no `fma`).
   The instruction-mix and memory axes were never explored.
2. **Results depend on DVFS.** All reported cells use a real warm-up and record the resulting clock;
   an unwarmed measurement is not comparable.
3. **`local_size` is baked into SPIR-V.** Each L needs its own shader; it cannot be changed at
   runtime.
4. **Long dispatches can be partial.** At n=1M with K=16 the contiguous tail of the buffer was never
   written (~713,000 work-items' worth). Only the full-buffer gate catches this.
5. **"Matched K" is not "matched work".** OpenCL divides `iters` across accumulators; Vulkan
   replicates the loop. Every cross-runtime comparison here states the full configuration.
6. **Codegen-level mechanism is not measurable on this device.** There are no hardware counters via
   KGSL sysfs, no `fdperf`, and no `msm_kgsl.h` on disk. Claims stop at "isolated by a one-variable
   A/B" and are not promoted to "mechanism identified".

A follow-up line of work — reconstructing the Adreno shader compiler ABI from a proprietary blob —
is deliberately **not** part of this repository (see §8).

---

## 5. Repository layout

```
src/                                  shared probes and harnesses, used across rounds
  mem/       bench.c kernels.S build.sh   RAM + disk benchmark (hand-written NEON kernels)
  opencl/    clinfo.c clprobe*.c clver.c  OpenCL probes: platform, device, version, program binary
             gpucl.c oclmech*.c oclws*.c  OpenCL harnesses (fma vs mul+add, grid shape, workgroup)
  vulkan/    vkcomp.c gpufma.c            early Vulkan harnesses
  gles/      gles.c                       OpenGL ES probe
  soc/       socprobe.c dsp*.c            SoC and Hexagon DSP probes
  video/     vdec*.c vidprobe.c           H.264 decode probe via V4L2
  misc/      hello.c                      smoke test

rounds/rN/                            one directory per round, uniform shape
  RESULTS.md  SPEC.md  REVIEW.md  VERIFY.md   documents (r1 carries all four)
  run_*.sh  gen_*.py                        round scripts and shader generators
  src/                                      round-specific harnesses (vkopt8.c, oclgrid.c, ...)
  shaders/                                  .comp sources and .spv builds for that round
  logs/                                     raw logs for that round
  artifacts/                                OpenCL program binaries (not tracked)

  r1/   configuration space: batch-vs-fence, OpenCL lsize x ILP
  r2/   execution strategy: representation, compiler path, kernel shape
  r3/   discriminating the K=2 spike (L sweep) and the fma() path
  r4/   eliminating spike mechanisms and the dispatch limit
  r5/   live-register x independent-FMA matrix (B4/B5)
  r5b/  one-variable control for B5 (definition before vs after the loop)

shaders/   root-level example sources (fma.comp, fma8.comp)
docs/      write-ups  (see docs/gpu-de-xfce-findings.md)
logs/      captured probe output that does not belong to a single round
assets/    third-party test vectors (not tracked)
build/     build output, split per round: build/r1, build/r3, build/r4 (not tracked)
```

Round scripts resolve their own location (`cd "$(dirname "$0")"`), so a checkout works from any
path — no absolute path is baked in. From inside `rounds/rN/`, shared binaries are reached as
`../../build/rN/<name>` and shaders as `shaders/<name>.spv`.

---

## 6. Build and run

### Prerequisites

- An aarch64 Android NDK (the cells here used r30, `30.0.14904198`). In this lab it lives at
  `/opt/android/android-sdk/ndk/30.0.14904198`; adjust to your own installation.
- SPIR-V tools: `glslangValidator`, `spirv-val`, `spirv-dis`. `glslc` is not available here.
- A rooted device with Adreno 616 and `/vendor` mounted inside the chroot.

### RAM and storage benchmark (native host build)

```sh
./src/mem/build.sh          # -Os plus size flags; the HOT kernels stay at -O3 via attributes
./build/bench mem           # cache sweep 8 KB -> 256 MB, thread scaling, pointer chase
./build/bench disk ./bench.bin
```

### GPU harness (bionic, cross-compiled)

```sh
NDK="$ANDROID_NDK_HOME"     # or the explicit path above
CC="$NDK/toolchains/llvm/prebuilt/linux-arm64/bin/aarch64-linux-android33-clang"

mkdir -p build/r4
"$CC" -O2 -o build/r4/vkopt8 rounds/r4/src/vkopt8.c -ldl

# Run from the chroot. The binary path MUST be absolute.
LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 \
  /apex/com.android.runtime/bin/linker64 "$PWD/build/r4/vkopt8" 2000 65536 10 64 2 ramp
```

Traps that cost real time and are therefore documented:

- **The binary path must be absolute.** `linker64 ./x` fails with `expected absolute path`.
- **stdout is block-buffered**, which makes a working program look hung. Call
  `setvbuf(stdout, NULL, _IONBF, 0)` first.
- The Adreno driver is **`dlopen`ed directly**; its symbols are C++ mangled
  (`_ZN11qglinternal21vkGetInstanceProcAddrE...`), not plain `vkGetInstanceProcAddr`. Link with
  `-ldl`, never `-lvulkan`.

### SPIR-V

`.spv` files are not committed (§7). Regenerate them from `.comp`:

```sh
glslangValidator -V -o out.spv in.comp && spirv-val out.spv
```

Three rounds ship generators that emit every cell from one template (uniform provenance):
`rounds/r2/gen_vk_shaders.py`, `rounds/r3/gen_grid.py`, `rounds/r5/gen_r5.py`.

### Reproducibility caveats

Stated plainly rather than implied:

- Of the 55 `.spv` files in the original tree, **14 have no same-named `.comp`**. Those were produced
  by a generator or have since been deleted. Cells whose `.comp` is present can be rebuilt exactly;
  for the rest only the raw log survives.
- **`rounds/r2` tracks no `.comp` and no `.spv` at all.** Its six cells exist only as raw logs; to
  rebuild them, run `rounds/r2/gen_vk_shaders.py` and then compile the output.
- `rounds/r5` and `rounds/r5b` have no round-local harness source: both drive `build/r4/vkopt8`, as
  their `RESULTS.md` states.
- `rounds/r1/logs/q3-large-n.raw.txt` is absent because Q3 was never executed. This is recorded in
  `rounds/r1/RESULTS.md`, not a missing artefact.

---

## 7. Why binaries are not committed

`.gitignore` uses an **allowlist**, not a denylist: everything is ignored first, then only sources,
documents and raw logs are un-ignored.

The reason is concrete. The original tree accumulated **39 extensionless aarch64 ELF binaries** and
**55 `.spv` blobs**. A denylist of binary names goes stale the moment a new round adds `vkopt10`; an
allowlist cannot leak by omission. The trade-off is the reverse risk, and it is deliberate: a **new
file type is ignored until it is added to `.gitignore`**. If a file is missing from `git ls-files`,
check that list first.

Excluded on purpose: `*.spv` (regenerate from `.comp`), `*.bin` (OpenCL program binaries, regenerate
by running `oclbin`), `foreman.264` (third-party H.264 test vector with unclear licensing), and all
build output.

---

## 8. License and scope

Released under the **MIT License**; see [LICENSE](LICENSE).

Scope notes:

- All code here was written for this lab. **No proprietary blob, firmware image, or Qualcomm
  material is committed.** The vendor libraries the harnesses talk to are read from the device at
  runtime and are not redistributed.
- The reverse-engineering work on the Adreno shader compiler (disassembly, ABI reconstruction,
  behavioural specification) is **not** part of this repository and is kept separately.
- Findings that are not about the GPU are written up in [`docs/`](docs), for example
  [`docs/gpu-de-xfce-findings.md`](docs/gpu-de-xfce-findings.md) on why hardware GL/EGL is
  unreachable for an X11 desktop on this kernel.

If you run this on different hardware: every number here is bound to one Adreno 616 unit at
610 MHz. Do not transplant the figures to another SoC without re-measuring — DVFS behaviour, cache
sizes and bus width differ, and that mismatch is the most common way GPU benchmarks mislead.
