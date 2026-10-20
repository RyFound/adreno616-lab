/* dsp2.c - aktifkan unsigned PD lalu buka skel. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>

/* struct RemoteRpcSessionControlRequest (fastrpc.h) */
typedef int (*fn_rsc)(void*, int);
typedef int (*fn_open64)(const char*, unsigned long long*);
typedef int (*fn_close64)(unsigned long long);

/* FASTRPC_CONTROL_DSPRPC = 0x201 ; UNSPD = FASTRPC_CONTROL_UNSIGNED_MODULE? */
#ifndef FASTRPC_CONTROL_DSPRPC
#define FASTRPC_CONTROL_DSPRPC 0x201
#endif

struct rrsc {
  unsigned int size;
  unsigned int request;
  int domain;
  int enable;
  void* rsp;
  unsigned int rsp_len;
};

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  void*h=dlopen("libcdsprpc.so",RTLD_NOW);
  if(!h){ puts("no libcdsprpc"); return 1; }
  fn_rsc rsc=(fn_rsc)dlsym(h,"remote_session_control");
  fn_open64 op=(fn_open64)dlsym(h,"remote_handle64_open");
  fn_close64 cl=(fn_close64)dlsym(h,"remote_handle64_close");
  printf("rsc=%p open=%p close=%p\n",(void*)rsc,(void*)op,(void*)cl);

  if(rsc){
    /* coba beberapa request id yang lazim untuk unsigned PD / reserve session */
    int reqs[]={0x201, 0x202, 0x203, 1, 2};
    for(int i=0;i<5;i++){
      struct rrsc r; memset(&r,0,sizeof(r));
      r.size=sizeof(r); r.request=(unsigned)reqs[i]; r.domain=3; r.enable=1;
      errno=0; int rc=rsc(&r, sizeof(r));
      printf("  session_control req=0x%x domain=3 enable=1 -> rc=%d errno=%d\n",reqs[i],rc,errno);
    }
  }

  const char*uris[]={
    "file:///libfastcvadsp.so?fastcvadsp_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libcamera_nn_skel.so?camera_nn_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libapps_mem_heap.so?apps_mem_heap_skel_handle_invoke&_modver=1.0&_dom=cdsp",
    "file:///libfastcvadsp.so?fastcvadsp_skel_handle_invoke",
    NULL};
  for(int i=0;uris[i];i++){
    unsigned long long hd=0; errno=0;
    int rc=op(uris[i],&hd);
    printf("open rc=%-3d handle=0x%llx  %s\n",rc,hd,uris[i]);
    if(rc==0&&cl) cl(hd);
  }
  return 0;
}
