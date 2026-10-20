/* dsp3.c - buka handle skel DSP dengan nama simbol invoke yang BENAR (dibaca dari ELF). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>

typedef int (*fn_open64)(const char*, unsigned long long*);
typedef int (*fn_close64)(unsigned long long);

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  void*h=dlopen("libcdsprpc.so",RTLD_NOW);
  if(!h){ puts("no libcdsprpc"); return 1; }
  fn_open64 op=(fn_open64)dlsym(h,"remote_handle64_open");
  fn_close64 cl=(fn_close64)dlsym(h,"remote_handle64_close");
  if(!op){ puts("no open"); return 1; }

  const char*uris[]={
    "file:///libdepthmap_skel.so?depthmap_skel_invoke&_modver=1.0&_dom=cdsp",
    "file:///libarcsoft_hdrplus_hvx_skel.so?arcsoft_hdrplus_hvx_skel_invoke&_modver=1.0&_dom=cdsp",
    "file:///libdepthmap_skel.so?depthmap_skel_invoke&_modver=1.0",
    "file:///libarcsoft_hdrplus_hvx_skel.so?arcsoft_hdrplus_hvx_skel_invoke&_modver=1.0",
    NULL};
  for(int i=0;uris[i];i++){
    unsigned long long hd=0; errno=0;
    int rc=op(uris[i],&hd);
    printf("open rc=%-3d handle=0x%llx  %s\n",rc,hd,uris[i]);
    if(rc==0){ puts("   >>> HANDLE DSP TERBUKA DARI CHROOT <<<"); if(cl){ int cr=cl(hd); printf("   close rc=%d\n",cr);} }
  }
  return 0;
}
