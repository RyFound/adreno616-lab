/* gpufma.c - OpenCL FMA throughput bench with ILP (independent accumulators).
 * Build bionic: NDK clang -O2 -o gpufma gpufma.c -ldl
 * Run: LD_LIBRARY_PATH=... /apex/com.android.runtime/bin/linker64 /home/han/adreno616-lab/gpufma [iters] [n] [acc]
 *
 * NOTE: source has NO literal backslash-n inside C strings (write_file corrupts them);
 * all newlines are emitted via puts()/printf with explicit \n escapes only in format args
 * that we keep to a single short line each. */
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
    int iters = argc>1?atoi(argv[1]):2000;
    int n = argc>2?atoi(argv[2]):(1<<16);
    int acc = argc>3?atoi(argv[3]):8;

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
    if(e!=0||npl==0){ printf("ERR platforms e=%d n=%u\n",e,npl); return 3; }
    cl_platform_id plats[8]; pGetPlatformIDs(npl,plats,NULL);
    char pname[256]={0}; pGetPlatformInfo(plats[0],CL_PLATFORM_NAME,sizeof(pname),pname,NULL);

    cl_device_id dev=NULL; cl_uint nd=0;
    e=pGetDeviceIDs(plats[0],CL_DEVICE_TYPE_GPU,1,&dev,&nd);
    if(e!=0||nd==0){ printf("ERR devices e=%d n=%u\n",e,nd); return 4; }
    char dname[256]={0}; pGetDeviceInfo(dev,CL_DEVICE_NAME,sizeof(dname),dname,NULL);
    cl_uint cu=0; pGetDeviceInfo(dev,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cu),&cu,NULL);
    cl_ulong gmem=0; pGetDeviceInfo(dev,CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(gmem),&gmem,NULL);
    cl_uint mhz=0; pGetDeviceInfo(dev,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(mhz),&mhz,NULL);

    cl_int err;
    cl_context ctx=pCreateContext(NULL,1,&dev,NULL,NULL,&err);
    cl_command_queue q=pCreateCommandQueue(ctx,dev,0,&err);

    /* Build kernel source dynamically so ACC (accumulator count) is a literal. */
    char src[4096]; int off=0;
    off += snprintf(src+off, sizeof(src)-off, "__kernel void fmb(__global float4*a,__global float4*b,__global float4*c,int iters){");
    off += snprintf(src+off, sizeof(src)-off, " int g=get_global_id(0); float4 x=a[g], y=b[g];");
    for(int i=0;i<acc;i++) off += snprintf(src+off, sizeof(src)-off, " float4 z%d=c[g];", i);
    off += snprintf(src+off, sizeof(src)-off, " for(int i=0;i<iters;i++){");
    for(int i=0;i<acc;i++) off += snprintf(src+off, sizeof(src)-off, " z%d=fma(x,y,z%d);", i,i);
    off += snprintf(src+off, sizeof(src)-off, " }");
    off += snprintf(src+off, sizeof(src)-off, " float4 s=z0;");
    for(int i=1;i<acc;i++) off += snprintf(src+off, sizeof(src)-off, " s+=z%d;", i);
    off += snprintf(src+off, sizeof(src)-off, " c[g]=s; }");

    const char*sp=src;
    cl_program prog=pCreateProgramWithSource(ctx,1,&sp,NULL,&err);
    e=pBuildProgram(prog,1,&dev,"-cl-fast-relaxed-math",NULL,NULL);
    if(e!=0){ char log[8192]={0}; pGetProgramBuildInfo(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,NULL); printf("ERR build %d\n%s\n",e,log); return 7; }
    cl_kernel k=pCreateKernel(prog,"fmb",&err);

    size_t bytes=(size_t)n*sizeof(float)*4;
    float*a=malloc(bytes),*b=malloc(bytes),*c=malloc(bytes);
    for(int i=0;i<n*4;i++){ a[i]=1.0001f; b[i]=1.0002f; c[i]=0.5f; }
    cl_mem ba=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,a,&err);
    cl_mem bb=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,b,&err);
    cl_mem bc=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,c,&err);

    pSetKernelArg(k,0,sizeof(cl_mem),&ba);
    pSetKernelArg(k,1,sizeof(cl_mem),&bb);
    pSetKernelArg(k,2,sizeof(cl_mem),&bc);
    pSetKernelArg(k,3,sizeof(int),&iters);
    size_t gsz=n, lsz=64;
    pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL); pFinish(q); /* warmup */

    int reps = argc>4?atoi(argv[4]):10;
    double t0=now_s();
    for(int r=0;r<reps;r++) pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL);
    pFinish(q);
    double t1=now_s();

    double total_fma=(double)n*4.0*(double)acc*(double)iters*(double)reps;
    double secs=t1-t0;
    puts("");
    puts("=== OpenCL FMA throughput (ILP kernel) ===");
    printf("platform=%s\n", pname);
    printf("device=%s cu=%u clk=%uMHz gmem=%luMB\n", dname, cu, mhz, (unsigned long)(gmem>>20));
    printf("n=%d f4 acc=%d iters=%d reps=%d wg=%d lsz=%d\n", n, acc, iters, reps, n/64, (int)lsz);
    printf("wall=%.4f s\n", secs);
    printf("FMA=%.3e  GFLOPS(fma=2flop)=%.2f\n", total_fma, total_fma*2.0/secs/1e9);
    return 0;
}
