# DE/XFCE + akselerasi GPU di device (chroot glibc) — hasil investigasi

2026-10-19. Target: XFCE yang berjalan di Xtigervnc `:1` memakai GPU (Adreno 616),
bukan software rendering.

## Verdict

**Tidak bisa dicapai lewat jalur Mesa/glibc di setup ini.** GLX dan EGL di `:1`
selalu jatuh ke `llvmpipe` (software). Bukan salah konfigurasi XFCE, bukan salah
Xtigervnc, dan bukan karena paket kurang.

## Bukti (semua diukur, bukan asumsi)

| Uji | Hasil |
|---|---|
| `glxinfo -B` (DISPLAY=:1) | `Device: llvmpipe (LLVM 21.1.8, 128 bits)`, **`Accelerated: no`** |
| `glxinfo` error | `glx: failed to create dri3 screen` / `failed to load driver: msm_drm` |
| `MESA_LOADER_DRIVER_OVERRIDE=kgsl` | `failed to load driver: kgsl`, tetap llvmpipe |
| `MESA_LOADER_DRIVER_OVERRIDE=zink` | `Error: couldn't find RGB GLX visual or fbconfig` |
| `eglinfo` | `EGL driver name: kms_swrast`; semua renderer llvmpipe |
| `GBM_BACKEND=msm eglinfo -B` | `failed to open msm: .../gbm/msm_gbm.so: No such file` |
| `/sys/class/drm/renderD128/device` | driver `msm_drm`, `OF_NAME=qcom,mdss_mdp` (display controller) |
| ioctl `DRM_VERSION` di `renderD128` | **OK** (1.2.0) — kontrol: node terbuka & DRM core hidup |
| ioctl `DRM_MSM_GET_PARAM` 16B (`0xC0106440`) di `renderD128` | **EINVAL** (GPU_ID, GMEM_SIZE, CHIP_ID) |
| ioctl `DRM_MSM_GEM_NEW` 16B (`0xC0106442`) di `renderD128` | **EINVAL** |
| ioctl MSM 16B di `card0` | EACCES — konsekuensi uid non-master, bukan bukti |
| `VK_ICD_FILENAMES=freedreno` + `vulkaninfo` | **`Failed to detect any valid GPUs`** / `ERROR_INITIALIZATION_FAILED` |
| `vulkaninfo` default | hanya `llvmpipe` |
| `libdrm.so.2` simbol `msm_*` / `kgsl` | 0 (bukti lemah — freedreno kirim ioctl sendiri) |

**Catatan metodologi (koreksi penting):** probe pertama memakai `struct drm_msm_param`
24-byte dari header Ubuntu baru (`0xC0186440`), padahal kernel 4.9 memakai struct 16-byte.
DRM core menolak ukuran yang salah dengan EINVAL **sebelum** driver melihat isinya — jadi
EINVAL pertama itu artefak alat, bukan bukti. Probe diulang dengan ukuran yang benar,
plus kontrol `DRM_VERSION` yang sukses, sehingga EINVAL pada `GET_PARAM`/`GEM_NEW`
sekarang bermakna: node-nya hidup, tapi ioctl spesifik MSM tidak dikenali.

## Akar masalah

Kernel device ini (4.9 downstream Qualcomm) merender lewat **KGSL** (`/dev/kgsl-3d0`),
sedangkan `msm_drm` hanya untuk display/scanout. Mesa freedreno (GL) dan turnip
(Vulkan) membutuhkan **upstream MSM render UAPI**, yang tidak ada di kernel ini —
dibuktikan dengan kontrol: `DRM_VERSION` sukses di node yang sama sementara
`GET_PARAM` dan `GEM_NEW` berukuran benar sama-sama EINVAL, dan turnip menemukan
nol device. Karena itu:

- `msm_dri.so` ada tapi tidak bisa membuka device → `failed to load driver`
- symlink nama driver (`msm_drm_dri.so`) tidak menolong — kegagalannya di ioctl, bukan nama file
- `zink` (GL di atas Vulkan) juga mati karena tidak ada Vulkan device yang bisa dipakai
- `libvulkan_freedreno.so` ada di disk tapi tidak berguna tanpa UAPI tersebut

## Yang MASIH bisa memakai GPU (jalur lain, sudah terbukti terpisah)

- **Bionic**: `libEGL_adreno.so` / `libGLESv2_adreno.so` dijalankan via
  `/apex/com.android.runtime/bin/linker64` (path absolut, `LD_LIBRARY_PATH=/vendor/lib64:/system/lib64`).
- **Vulkan compute**: `dlopen("/vendor/lib64/hw/vulkan.adreno.so")` + simbol C++ mangled
  `qglinternal` (loader Android menemukan 0 device, jadi driver dipanggil langsung).
- **OpenCL**: `libOpenCL.so` (shim) → `libCB.so`, `clinfo` mengonfirmasi Adreno 616.

Artinya GPU bisa dipakai untuk komputasi/render offscreen dari proses bionic,
tapi **tidak** untuk compositor X11 glibc (xfwm4) maupun aplikasi X glibc.

## Tindakan yang diambil

- `xfwm4 /general/use_compositing` diset **false** — mematikan compositor software
  yang membebani CPU tanpa manfaat (shadow/transisi) di sesi VNC.
- Wallpaper sudah ditangani `setwall.py` (xfdesktop dimatikan oleh `xfce-vnc`).
- Laptop viewer + DPMS sudah permanen (`~/bin/lap-vnc` + autostart).

## Kalau tetap ingin GL di X11

Satu-satunya jalan realistis adalah menjalankan X server **bionic** (bukan glibc)
sehingga bisa memuat `libEGL_adreno.so`. Itu proyek terpisah dan berisiko; tidak
dilakukan di sini.
