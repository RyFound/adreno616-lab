/* clinfo.c - OpenCL device info probe (bionic, dlopen-based). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
typedef void* cl_platform_id;
typedef void* cl_device_id;
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_bitfield;
typedef unsigned long cl_ulong;
#define CL_DEVICE_TYPE_GPU (1u<<2)
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_VENDOR 0x102C
#define CL_DEVICE_VERSION 0x102F
#define CL_DRIVER_VERSION 0x102D
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
#define CL_DEVICE_TYPE 0x1000
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x101F
#define CL_DEVICE_MAX_WORK_GROUP_SIZE 0x1004
#define CL_PLATFORM_NAME 0x0902
#define CL_PLATFORM_VENDOR 0x0903
#define CL_PLATFORM_VERSION 0x0901
static void getstr(void*fp,cl_device_id d,cl_uint p,const char*tag){ char b[256]={0}; ((cl_int(*)(cl_device_id,cl_uint,size_t,void*,size_t*))fp)(d,p,sizeof(b),b,NULL); printf("  %-22s = %s\n",tag,b); }
static void getu32(void*fp,cl_device_id d,cl_uint p,const char*tag){ cl_uint v=0; ((cl_int(*)(cl_device_id,cl_uint,size_t,void*,size_t*))fp)(d,p,sizeof(v),&v,NULL); printf("  %-22s = %u\n",tag,v); }
static void getu64(void*fp,cl_device_id d,cl_uint p,const char*tag){ cl_ulong v=0; ((cl_int(*)(cl_device_id,cl_uint,size_t,void*,size_t*))fp)(d,p,sizeof(v),&v,NULL); printf("  %-22s = %lu\n",tag,(unsigned long)v); }
int main(void){ setvbuf(stdout,0,_IONBF,0);
 void*h=dlopen("libOpenCL.so",RTLD_NOW); if(!h){puts("no OpenCL");return 1;}
 cl_int(*pGP)(cl_uint,cl_platform_id*,cl_uint*)=dlsym(h,"clGetPlatformIDs");
 cl_int(*pGD)(cl_platform_id,cl_bitfield,cl_uint,cl_device_id*,cl_uint*)=dlsym(h,"clGetDeviceIDs");
 void*pDI=dlsym(h,"clGetDeviceInfo");
 void*pPI=dlsym(h,"clGetPlatformInfo");
 cl_uint npl=0; pGP(0,NULL,&npl); printf("platforms=%u\n",npl);
 cl_platform_id p[8]; pGP(npl,p,NULL);
 for(cl_uint i=0;i<npl;i++){ char b[256]={0};
   ((cl_int(*)(cl_platform_id,cl_uint,size_t,void*,size_t*))pPI)(p[i],CL_PLATFORM_NAME,sizeof(b),b,NULL); printf("platform[%u] name=%s\n",i,b);
   ((cl_int(*)(cl_platform_id,cl_uint,size_t,void*,size_t*))pPI)(p[i],CL_PLATFORM_VENDOR,sizeof(b),b,NULL); printf("  vendor=%s\n",b);
   ((cl_int(*)(cl_platform_id,cl_uint,size_t,void*,size_t*))pPI)(p[i],CL_PLATFORM_VERSION,sizeof(b),b,NULL); printf("  version=%s\n",b);
   cl_uint nd=0; cl_device_id d[8]; cl_int e=pGD(p[i],CL_DEVICE_TYPE_GPU,8,d,&nd);
   printf("  GPU devices=%u (e=%d)\n",nd,e);
   for(cl_uint j=0;j<nd;j++){ getu32(pDI,d[j],CL_DEVICE_TYPE,"type"); getstr(pDI,d[j],CL_DEVICE_NAME,"name"); getstr(pDI,d[j],CL_DEVICE_VENDOR,"vendor"); getstr(pDI,d[j],CL_DEVICE_VERSION,"version"); getstr(pDI,d[j],CL_DRIVER_VERSION,"driver"); getu32(pDI,d[j],CL_DEVICE_MAX_COMPUTE_UNITS,"compute_units"); getu32(pDI,d[j],CL_DEVICE_MAX_CLOCK_FREQUENCY,"max_clock_mhz"); getu32(pDI,d[j],CL_DEVICE_MAX_WORK_GROUP_SIZE,"max_wg"); getu64(pDI,d[j],CL_DEVICE_GLOBAL_MEM_SIZE,"global_mem"); }
 }
 return 0; }
