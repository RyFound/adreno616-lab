# Desktop environment (XFCE) and GPU acceleration on this device — investigation results

2026-10-19. Target: make XFCE, running on Xtigervnc display `:1`, use the Adreno 616 GPU
instead of software rendering.

## Verdict

**Not reachable through the Mesa/glibc path on this setup.** GLX and EGL on `:1` always fall
back to `llvmpipe` (software). This is not an XFCE misconfiguration, not an Xtigervnc problem,
and not a missing-package problem.

## Evidence (measured, not assumed)

| Probe | Result |
|---|---|
| `glxinfo -B` (`DISPLAY=:1`) | `Device: llvmpipe (LLVM 21.1.8, 128 bits)`, **`Accelerated: no`** |
| `glxinfo` errors | `glx: failed to create dri3 screen` / `failed to load driver: msm_drm` |
| `MESA_LOADER_DRIVER_OVERRIDE=kgsl` | `failed to load driver: kgsl`, still llvmpipe |
| `MESA_LOADER_DRIVER_OVERRIDE=zink` | `Error: couldn't find RGB GLX visual or fbconfig` |
| `eglinfo` | `EGL driver name: kms_swrast`; every renderer reports llvmpipe |
| `GBM_BACKEND=msm eglinfo -B` | `failed to open msm: .../gbm/msm_gbm.so: No such file` |
| `/sys/class/drm/renderD128/device` | driver `msm_drm`, `OF_NAME=qcom,mdss_mdp` (display controller) |
| ioctl `DRM_VERSION` on `renderD128` | **OK** (1.2.0) — control: the node opens and DRM core is alive |
| ioctl `DRM_MSM_GET_PARAM` 16 B (`0xC0106440`) on `renderD128` | **EINVAL** (GPU_ID, GMEM_SIZE, CHIP_ID) |
| ioctl `DRM_MSM_GEM_NEW` 16 B (`0xC0106442`) on `renderD128` | **EINVAL** |
| MSM ioctl 16 B on `card0` | EACCES — consequence of a non-master uid, not evidence |
| `VK_ICD_FILENAMES=freedreno` + `vulkaninfo` | **`Failed to detect any valid GPUs`** / `ERROR_INITIALIZATION_FAILED` |
| `vulkaninfo`, default | `llvmpipe` only |
| `libdrm.so.2` symbols `msm_*` / `kgsl` | 0 (weak evidence — freedreno issues its own ioctls) |

**Methodology note (an important correction).** The first probe used a 24-byte
`struct drm_msm_param` from a newer Ubuntu header (`0xC0186440`), whereas this 4.9 kernel uses
a 16-byte struct. DRM core rejects a wrong-sized ioctl with EINVAL **before** the driver ever
sees the contents, so that first EINVAL was an artefact of the tool, not evidence about the
driver. The probe was repeated with the correct size, together with a `DRM_VERSION` control
that succeeds. The EINVAL on `GET_PARAM`/`GEM_NEW` is therefore meaningful: the node is alive,
but the MSM-specific ioctls are not recognised.

## Root cause

The kernel on this device (Qualcomm 4.9 downstream) renders through **KGSL**
(`/dev/kgsl-3d0`); `msm_drm` handles display and scanout only. Mesa freedreno (GL) and turnip
(Vulkan) require the **upstream MSM render UAPI**, which this kernel does not provide. The
control pair above establishes that directly: `DRM_VERSION` succeeds on the same node while
correctly-sized `GET_PARAM` and `GEM_NEW` both return EINVAL, and turnip finds zero devices.
Consequently:

- `msm_dri.so` exists but cannot open the device, hence `failed to load driver`
- renaming or symlinking the driver (`msm_drm_dri.so`) does not help — the failure is in the
  ioctl layer, not in file naming
- `zink` (GL on top of Vulkan) is equally dead, because no usable Vulkan device exists
- `libvulkan_freedreno.so` is present on disk but useless without that UAPI

## What can still use the GPU (separate, independently proven paths)

- **Bionic**: `libEGL_adreno.so` / `libGLESv2_adreno.so`, launched through
  `/apex/com.android.runtime/bin/linker64` (absolute path required,
  `LD_LIBRARY_PATH=/vendor/lib64:/system/lib64`).
- **Vulkan compute**: `dlopen("/vendor/lib64/hw/vulkan.adreno.so")` plus the C++ mangled
  `qglinternal` symbols. The Android loader finds zero devices, so the driver is called directly.
- **OpenCL**: `libOpenCL.so` (a shim) forwarding to `libCB.so`; `clinfo` reports Adreno 616.

So the GPU is usable for compute and offscreen rendering from a bionic process, but **not** for
an X11 compositor (xfwm4) or for glibc X11 applications.

## Actions taken

- `xfwm4 /general/use_compositing` set to **false** — this disables the software compositor,
  which consumed CPU with no benefit (shadows, transitions) in a VNC session.
- Wallpaper is handled by `setwall.py` (xfdesktop is disabled by `xfce-vnc`).
- The laptop viewer and DPMS handling are permanent (`~/bin/lap-vnc` plus an autostart entry).

## If GL on X11 is still wanted

The only realistic route is to run a **bionic** X server (rather than a glibc one) so that it
can load `libEGL_adreno.so`. That is a separate, higher-risk project and was not attempted here.
