/* oclprobe_host.c - can a bionic process in the DEFAULT linker namespace
 * (the same context as the Neuron server) dlopen the OpenCL stack?
 * Build: NDK clang -O2 -o oclprobe_host oclprobe_host.c -ldl
 * No \n inside string literals on purpose (write_file mangles backslash-n).
 */
#include <stdio.h>
#include <dlfcn.h>

int main(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    const char *paths[] = {
        "libOpenCL.so",
        "/vendor/lib64/libOpenCL.so",
        "libCB.so",
        "/vendor/lib64/libCB.so",
        "/vendor/lib64/libgsl.so",
        "libvulkan.so",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        void *h = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (h) {
            printf("OK   %-32s -> %p", paths[i], h);
            void *sym = dlsym(h, "clGetPlatformIDs");
            if (!sym) sym = dlsym(h, "clGetPlatformInfo");
            printf("  clGetPlatformIDs=%s", sym ? "yes" : "no");
            printf("\n");
        } else {
            const char *e = dlerror();
            printf("FAIL %-32s -> %s", paths[i], e ? e : "(null)");
            printf("\n");
        }
    }
    return 0;
}
