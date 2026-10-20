/* dspprobe.c - coba buka skel Hexagon lewat format URI fastrpc yang benar.
 * Pakai libcdsprpc.so (libadsprpc bisa hang). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>

typedef int (*fn_open64)(const char*, unsigned long long*);
typedef int (*fn_invoke64)(unsigned long long, unsigned int, void*, unsigned int, void*, unsigned int, void*, unsigned int);
typedef int (*fn_close64)(unsigned long long);

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  void*h=dlopen("libcdsprpc.so",RTLD_NOW);
  if(!h) h=dlopen("/vendor/lib64/libcdsprpc.so",RTLD_NOW);
  if(!h){ puts("dlopen libcdsprpc FAIL"); return 1; }
  puts("libcdsprpc OK");
  fn_open64 op=(fn_open64)dlsym(h,"remote_handle64_open");
  fn_close64 cl=(fn_close64)dlsym(h,"remote_handle64_close");
  if(!op){ puts("no remote_handle64_open"); return 1; }

  const char*uris[]={
    "file:///libfastcvadsp.so?fastcvadsp_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libfastcvadsp.so?fastcvadsp_skel_handle_invoke&_modver=1.0&_dom=adsp",
    "file:///libcamera_nn_skel.so?camera_nn_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libdspCV_skel.so?dspCV_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libapps_mem_heap.so?apps_mem_heap_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libVC1DecDsp_skel.so?VC1DecDsp_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libdepthmap_skel.so?depthmap_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    NULL};
  for(int i=0;uris[i];i++){
    unsigned long long hd=0; errno=0;
    int rc=op(uris[i],&hd);
    printf("rc=%-3d handle=0x%llx errno=%d  %s\n", rc, hd, errno, uris[i]);
    if(rc==0 && cl){ cl(hd); puts("   -> dibuka & ditutup OK"); }
  }
  return 0;
}
