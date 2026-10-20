/* gpucl.c - bionic OpenCL probe+bench, dlopen-based (no CL headers). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>

typedef void* cl_platform_id;
typedef void* cl_device_id;
typedef void* cl_context;
typedef void* cl_command_queue;
typedef void* cl_program;
typedef void* cl_kernel;
typedef void* cl_mem;
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_ulong;
typedef unsigned long cl_bitfield;

#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_GPU (1u<<2)
#define CL_MEM_READ_WRITE (1u<<0)
#define CL_MEM_COPY_HOST_PTR (1u<<5)
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x101F
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
#define CL_PLATFORM_NAME 0x0902
#define CL_PROGRAM_BUILD_LOG 0x1183

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }

int main(int argc, char**argv){
    setvbuf(stdout,0,_IONBF,0);
    int iters = argc>1?atoi(argv[1]):200;
    int n = argc>2?atoi(argv[2]):(1<<20);

    void*h = dlopen("libOpenCL.so", RTLD_NOW);
    if(!h) h = dlopen("/vendor/lib64/libOpenCL.so", RTLD_NOW);
    if(!h){ fputs("ERR dlopen libOpenCL.so: ",stdout); puts(dlerror()); return 2; }
    puts("OK loaded libOpenCL.so");

    cl_int (*pGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
    cl_int (*pGetDeviceIDs)(cl_platform_id, cl_bitfield, cl_uint, cl_device_id*, cl_uint*);
    cl_int (*pGetDeviceInfo)(cl_device_id, cl_uint, size_t, void*, size_t*);
    cl_int (*pGetPlatformInfo)(cl_platform_id, cl_uint, size_t, void*, size_t*);
    cl_context (*pCreateContext)(const cl_context*, cl_uint, const cl_device_id*, void*, void*, cl_int*);
    cl_command_queue (*pCreateCommandQueue)(cl_context, cl_device_id, cl_bitfield, cl_int*);
    cl_program (*pCreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
    cl_int (*pBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*, void*, void*);
    cl_int (*pGetProgramBuildInfo)(cl_program, cl_device_id, cl_uint, size_t, void*, size_t*);
    cl_kernel (*pCreateKernel)(cl_program, const char*, cl_int*);
    cl_mem (*pCreateBuffer)(cl_context, cl_bitfield, size_t, void*, cl_int*);
    cl_int (*pSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
    cl_int (*pEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const void*, void*);
    cl_int (*pFinish)(cl_command_queue);

#define SYM(v,nm) do{ *(void**)(&v)=dlsym(h,nm); if(!(v)){ fputs("ERR dlsym ",stdout); puts(nm); return 2; } }while(0)
    SYM(pGetPlatformIDs,"clGetPlatformIDs");
    SYM(pGetDeviceIDs,"clGetDeviceIDs");
    SYM(pGetDeviceInfo,"clGetDeviceInfo");
    SYM(pGetPlatformInfo,"clGetPlatformInfo");
    SYM(pCreateContext,"clCreateContext");
    SYM(pCreateCommandQueue,"clCreateCommandQueue");
    SYM(pCreateProgramWithSource,"clCreateProgramWithSource");
    SYM(pBuildProgram,"clBuildProgram");
    SYM(pGetProgramBuildInfo,"clGetProgramBuildInfo");
    SYM(pCreateKernel,"clCreateKernel");
    SYM(pCreateBuffer,"clCreateBuffer");
    SYM(pSetKernelArg,"clSetKernelArg");
    SYM(pEnqueueNDRangeKernel,"clEnqueueNDRangeKernel");
    SYM(pFinish,"clFinish");

    cl_uint npl=0; cl_int e=pGetPlatformIDs(0,NULL,&npl);
    printf("clGetPlatformIDs -> %d platforms=%u\n", e, npl);
    if(e!=0||npl==0) return 3;
    cl_platform_id plats[8]; pGetPlatformIDs(npl,plats,NULL);
    char pname[256]={0}; pGetPlatformInfo(plats[0],CL_PLATFORM_NAME,sizeof(pname),pname,NULL);
    printf("Platform[0]: %s\n", pname);

    cl_device_id dev=NULL; cl_uint nd=0;
    e=pGetDeviceIDs(plats[0],CL_DEVICE_TYPE_GPU,1,&dev,&nd);
    printf("clGetDeviceIDs(GPU) -> %d devices=%u\n", e, nd);
    if(e!=0||nd==0) return 4;

    char dname[256]={0}; pGetDeviceInfo(dev,CL_DEVICE_NAME,sizeof(dname),dname,NULL);
    cl_uint cu=0; pGetDeviceInfo(dev,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cu),&cu,NULL);
    cl_ulong gmem=0; pGetDeviceInfo(dev,CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(gmem),&gmem,NULL);
    cl_uint mhz=0; pGetDeviceInfo(dev,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(mhz),&mhz,NULL);
    printf("Device: %s\n", dname);
    printf("  compute_units=%u max_clock=%u MHz global_mem=%lu MB\n", cu, mhz, (unsigned long)(gmem>>20));

    cl_int err;
    cl_context ctx=pCreateContext(NULL,1,&dev,NULL,NULL,&err);
    if(!ctx){ printf("ERR ctx %d\n",err); return 5; }
    cl_command_queue q=pCreateCommandQueue(ctx,dev,0,&err);
    if(!q){ printf("ERR queue %d\n",err); return 6; }

    const char*src =
      "__kernel void fma_bench(__global float4*a,__global float4*b,__global float4*c,int iters){"
      " int g=get_global_id(0); float4 x=a[g],y=b[g],z=c[g];"
      " for(int i=0;i<iters;i++){ z=fma(x,y,z); } c[g]=z; }";
    cl_program prog=pCreateProgramWithSource(ctx,1,&src,NULL,&err);
    e=pBuildProgram(prog,1,&dev,"-cl-fast-relaxed-math",NULL,NULL);
    if(e!=0){ char log[8192]={0}; pGetProgramBuildInfo(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,NULL); printf("ERR build %d\n%s\n",e,log); return 7; }
    puts("Kernel built OK");

    cl_kernel k=pCreateKernel(prog,"fma_bench",&err);
    if(!k){ printf("ERR kernel %d\n",err); return 8; }

    size_t bytes=(size_t)n*sizeof(float)*4;
    float*a=malloc(bytes),*b=malloc(bytes),*c=malloc(bytes);
    for(int i=0;i<n*4;i++){ a[i]=1.0001f; b[i]=1.0002f; c[i]=0.5f; }
    cl_mem ba=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,a,&err);
    cl_mem bb=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,b,&err);
    cl_mem bc=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,c,&err);

    int inner=1000;
    pSetKernelArg(k,0,sizeof(cl_mem),&ba);
    pSetKernelArg(k,1,sizeof(cl_mem),&bb);
    pSetKernelArg(k,2,sizeof(cl_mem),&bc);
    pSetKernelArg(k,3,sizeof(int),&inner);
    size_t gsz=n, lsz=64;
    pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL); pFinish(q);

    double t0=now_s();
    for(int it=0; it<iters; it++) pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL);
    pFinish(q);
    double t1=now_s();

    double total_fma=(double)n*4.0*inner*iters;
    double secs=t1-t0;
    puts("");
    puts("=== OpenCL FMA bench ===");
    printf("n=%d f4  inner=%d  iters=%d  wg=%d\n", n, inner, iters, n/64);
    printf("wall=%.4f s\n", secs);
    printf("FMA ops=%.3e\n", total_fma);
    printf("GFLOPS(fma=2flop)=%.2f\n", total_fma*2.0/secs/1e9);
    return 0;
}
