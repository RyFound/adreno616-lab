/* clver.c - query CL_DEVICE_VERSION / OPENCL_C_VERSION / EXTENSIONS via dlopen. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

typedef void* cl_platform_id;
typedef void* cl_device_id;
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_bitfield;

#define CL_DEVICE_TYPE_GPU (1u<<2)
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_VENDOR 0x102C
#define CL_DEVICE_VERSION 0x102F
#define CL_DRIVER_VERSION 0x102D
#define CL_DEVICE_OPENCL_C_VERSION 0x103D
#define CL_DEVICE_EXTENSIONS 0x1030
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
#define CL_PLATFORM_NAME 0x0902
#define CL_PLATFORM_VERSION 0x0901

static const char* S(cl_device_id d, cl_uint name, char* buf, size_t n,
                     cl_int(*fn)(cl_device_id,cl_uint,size_t,void*,size_t*)){
  buf[0]=0; fn(d,name,n,buf,NULL); return buf;
}
static const char* SP(cl_platform_id p, cl_uint name, char* buf, size_t n,
                      cl_int(*fn)(cl_platform_id,cl_uint,size_t,void*,size_t*)){
  buf[0]=0; fn(p,name,n,buf,NULL); return buf;
}

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  void*h=dlopen("libOpenCL.so",RTLD_NOW);
  if(!h) h=dlopen("/vendor/lib64/libOpenCL.so",RTLD_NOW);
  if(!h){ puts("no libOpenCL"); return 1; }

  cl_int(*pGetPlatformIDs)(cl_uint,cl_platform_id*,cl_uint*);
  cl_int(*pGetDeviceIDs)(cl_platform_id,cl_bitfield,cl_uint,cl_device_id*,cl_uint*);
  cl_int(*pGetDeviceInfo)(cl_device_id,cl_uint,size_t,void*,size_t*);
  cl_int(*pGetPlatformInfo)(cl_platform_id,cl_uint,size_t,void*,size_t*);
  *(void**)(&pGetPlatformIDs)=dlsym(h,"clGetPlatformIDs");
  *(void**)(&pGetDeviceIDs)=dlsym(h,"clGetDeviceIDs");
  *(void**)(&pGetDeviceInfo)=dlsym(h,"clGetDeviceInfo");
  *(void**)(&pGetPlatformInfo)=dlsym(h,"clGetPlatformInfo");
  if(!pGetPlatformIDs||!pGetDeviceIDs||!pGetDeviceInfo||!pGetPlatformInfo){ puts("missing sym"); return 1; }

  cl_platform_id p=NULL; cl_uint np=0;
  pGetPlatformIDs(1,&p,&np);
  if(np==0){ puts("no platform"); return 1; }

  char buf[8192];
  printf("platform.name    = %s", SP(p,CL_PLATFORM_NAME,buf,sizeof(buf),pGetPlatformInfo)); putchar(10);
  printf("platform.version = %s", SP(p,CL_PLATFORM_VERSION,buf,sizeof(buf),pGetPlatformInfo)); putchar(10);

  cl_device_id d=NULL; cl_uint nd=0;
  if(pGetDeviceIDs(p,CL_DEVICE_TYPE_GPU,1,&d,&nd)!=0||nd==0){ puts("no GPU device"); return 1; }

  printf("device.name      = %s", S(d,CL_DEVICE_NAME,buf,sizeof(buf),pGetDeviceInfo)); putchar(10);
  printf("device.vendor    = %s", S(d,CL_DEVICE_VENDOR,buf,sizeof(buf),pGetDeviceInfo)); putchar(10);
  printf("device.version   = %s   <<< VERSI OPENCL", S(d,CL_DEVICE_VERSION,buf,sizeof(buf),pGetDeviceInfo)); putchar(10);
  printf("driver.version   = %s", S(d,CL_DRIVER_VERSION,buf,sizeof(buf),pGetDeviceInfo)); putchar(10);
  printf("opencl.c.version = %s", S(d,CL_DEVICE_OPENCL_C_VERSION,buf,sizeof(buf),pGetDeviceInfo)); putchar(10);

  cl_uint cu=0; pGetDeviceInfo(d,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cu),&cu,NULL);
  printf("max_compute_units= %u", cu); putchar(10);
  cl_uint mhz=0; pGetDeviceInfo(d,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(mhz),&mhz,NULL);
  printf("max_clock_mhz    = %u", mhz); putchar(10);

  size_t got=0;
  pGetDeviceInfo(d,CL_DEVICE_EXTENSIONS,0,NULL,&got);
  printf("extensions bytes = %zu", got); putchar(10);
  if(got>0 && got<sizeof(buf)){
    pGetDeviceInfo(d,CL_DEVICE_EXTENSIONS,got,buf,NULL);
    printf("extensions: %s", buf); putchar(10);
  }
  return 0;
}
