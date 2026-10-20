/* oclmech2.c - closing arms for the fma() cliff table, scalar width.
 * Question: is scalar fma() slow for the SAME reason as vec4 (correctly-rounded emulation)?
 * If mad() is fast while fma() is slow, mechanism is confirmed at scalar width too.
 * Arms (gate-exact C0==iters, same process/queue/device):
 *   E2 scalar  zv*xv+yv   opts=""                    -> expect ~37
 *   F2 scalar  fma()      opts=""                    -> expect ~0.77
 *   G  scalar  mad()      opts=""                    -> fast => emulation confirmed
 *   H  scalar  mad()      opts="-cl-fast-relaxed-math"
 * Build: aarch64-linux-android33-clang -O2 -o oclmech2 oclmech2.c -ldl
 * No backslash-n inside string literals (write_file corrupts it); build source via nl[].
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dlfcn.h>

typedef void* cl_platform_id; typedef void* cl_device_id; typedef void* cl_context;
typedef void* cl_command_queue; typedef void* cl_program; typedef void* cl_kernel;
typedef void* cl_mem; typedef void* cl_event; typedef int cl_int; typedef unsigned int cl_uint;
typedef unsigned long cl_bitfield;
#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_MEM_READ_WRITE (1<<0)
#define CL_MEM_COPY_HOST_PTR (1<<5)
#define CL_TRUE 1
#define CL_QUEUE_PROFILING_ENABLE (1<<1)
#define CL_PROGRAM_BUILD_LOG 0x1183

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }
static int read_freq(void){ FILE*f=fopen("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq","r"); if(!f) return -1; long v=0; if(fscanf(f,"%ld",&v)!=1)v=-1; fclose(f); return (int)v; }

typedef cl_int (*f_gpi)(cl_uint,cl_platform_id*,cl_uint*);
typedef cl_int (*f_gdi)(cl_platform_id,unsigned long,cl_uint,cl_device_id*,cl_uint*);
typedef cl_context (*f_cc)(const long*,cl_uint,const cl_device_id*,void*,void*,cl_int*);
typedef cl_command_queue (*f_ccq)(cl_context,cl_device_id,cl_bitfield,cl_int*);
typedef cl_mem (*f_cb)(cl_context,cl_bitfield,size_t,void*,cl_int*);
typedef cl_int (*f_ewb)(cl_command_queue,cl_mem,cl_uint,size_t,size_t,const void*,cl_uint,const void*,void*);
typedef cl_program (*f_cp)(cl_context,cl_uint,const char**,const size_t*,cl_int*);
typedef cl_int (*f_bp)(cl_program,cl_uint,const cl_device_id*,const char*,void*,void*);
typedef cl_int (*f_gpbi)(cl_program,cl_device_id,cl_uint,size_t,void*,size_t*);
typedef cl_kernel (*f_ck)(cl_program,const char*,cl_int*);
typedef cl_int (*f_ska)(cl_kernel,cl_uint,size_t,const void*);
typedef cl_int (*f_enk)(cl_command_queue,cl_kernel,cl_uint,const size_t*,const size_t*,const size_t*,cl_uint,const void*,cl_event*);
typedef cl_int (*f_fin)(cl_command_queue);
typedef cl_int (*f_erb)(cl_command_queue,cl_mem,cl_uint,size_t,size_t,void*,cl_uint,const void*,void*);

static f_cp cp; static f_bp bp; static f_gpbi gpbi; static f_ck ck;
static f_ska ska; static f_enk enk; static f_fin fin; static f_erb erb; static f_ewb ewb;
static cl_device_id dev; static cl_context ctx;

static cl_kernel mk(const char*src,const char*opts,const char*tag){
    cl_int err=0; const char*sp=src;
    cl_program prog=cp(ctx,1,&sp,NULL,&err);
    err=bp(prog,1,&dev,opts,NULL,NULL);
    if(err!=CL_SUCCESS){ char log[8192]; size_t z=0; gpbi(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,&z);
        printf("%s BUILD_FAIL rc=%d %s",tag,err,log); puts(""); return 0; }
    cl_kernel k=ck(prog,"gk",&err);
    if(!k){ printf("%s KERNEL_FAIL rc=%d",tag,err); puts(""); return 0; }
    return k;
}

#define NARM 4
int main(void){
    setvbuf(stdout,0,_IONBF,0);
    uint32_t n=65536, iters=2000; size_t L=64;
    void*lib=dlopen("libOpenCL.so",RTLD_NOW|RTLD_LOCAL); if(!lib) lib=dlopen("/vendor/lib64/libOpenCL.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){ puts("dlopen fail"); return 1; }
    f_gpi gpi=(f_gpi)dlsym(lib,"clGetPlatformIDs"); f_gdi gdi=(f_gdi)dlsym(lib,"clGetDeviceIDs");
    f_cc cc=(f_cc)dlsym(lib,"clCreateContext"); f_ccq ccq=(f_ccq)dlsym(lib,"clCreateCommandQueue");
    f_cb cb=(f_cb)dlsym(lib,"clCreateBuffer");
    ewb=(f_ewb)dlsym(lib,"clEnqueueWriteBuffer");
    cp=(f_cp)dlsym(lib,"clCreateProgramWithSource"); bp=(f_bp)dlsym(lib,"clBuildProgram");
    gpbi=(f_gpbi)dlsym(lib,"clGetProgramBuildInfo"); ck=(f_ck)dlsym(lib,"clCreateKernel");
    ska=(f_ska)dlsym(lib,"clSetKernelArg"); enk=(f_enk)dlsym(lib,"clEnqueueNDRangeKernel");
    fin=(f_fin)dlsym(lib,"clFinish"); erb=(f_erb)dlsym(lib,"clEnqueueReadBuffer");
    cl_platform_id plat=0; cl_uint np=0; gpi(1,&plat,&np);
    cl_uint nd=0; gdi(plat,CL_DEVICE_TYPE_ALL,1,&dev,&nd);
    cl_int err=0; ctx=cc(NULL,1,&dev,NULL,NULL,&err);
    cl_command_queue q=ccq(ctx,dev,CL_QUEUE_PROFILING_ENABLE,&err);
    if(err!=CL_SUCCESS){ puts("queue fail"); return 1; }

    size_t bytes=(size_t)n*4;   /* SCALAR: one float per work-item */
    float*hx=malloc(bytes),*hy=malloc(bytes),*hz=malloc(bytes),*ho=malloc(bytes);
    for(size_t k=0;k<(size_t)n;k++){hx[k]=1.0f;hy[k]=1.0f;hz[k]=0.0f;}
    cl_mem bx=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hx,&err);
    cl_mem by=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hy,&err);
    cl_mem bz=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hz,&err);

    size_t gsz=(size_t)n, lsz=L;
    double flops=(double)n*1.0*(double)iters*2.0;  /* 1 fma per work-item per iter */
    char nl[2]; nl[0]=10; nl[1]=0;
    char src[NARM][2048];
    int o;

    /* E2: scalar muladd */
    o=0;
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"__kernel void gk(__global float* x, __global float* y, __global float* z, uint iters){%s",nl);
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"  float xv=x[i]; float yv=y[i]; float zv=z[i];%s",nl);
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"  for(uint t=0;t<iters;t++){ zv=zv*xv+yv; }%s",nl);
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src[0]+o,sizeof(src[0])-o,"}%s",nl);
    /* F2: scalar fma */
    o=0;
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"__kernel void gk(__global float* x, __global float* y, __global float* z, uint iters){%s",nl);
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"  float xv=x[i]; float yv=y[i]; float zv=z[i];%s",nl);
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"  for(uint t=0;t<iters;t++){ zv=fma(zv,xv,yv); }%s",nl);
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src[1]+o,sizeof(src[1])-o,"}%s",nl);
    /* G: scalar mad */
    o=0;
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"__kernel void gk(__global float* x, __global float* y, __global float* z, uint iters){%s",nl);
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"  float xv=x[i]; float yv=y[i]; float zv=z[i];%s",nl);
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"  for(uint t=0;t<iters;t++){ zv=mad(zv,xv,yv); }%s",nl);
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src[2]+o,sizeof(src[2])-o,"}%s",nl);
    /* H: scalar mad + fast-relaxed-math */
    o=0;
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"__kernel void gk(__global float* x, __global float* y, __global float* z, uint iters){%s",nl);
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"  float xv=x[i]; float yv=y[i]; float zv=z[i];%s",nl);
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"  for(uint t=0;t<iters;t++){ zv=mad(zv,xv,yv); }%s",nl);
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src[3]+o,sizeof(src[3])-o,"}%s",nl);

    const char*tag[NARM]={"E2_scalar_muladd","F2_scalar_fma","G_scalar_mad","H_scalar_mad_fast"};
    const char*opts[NARM]={"","","","-cl-fast-relaxed-math"};

    for(int v=0;v<NARM;v++){
        cl_kernel k=mk(src[v],opts[v],tag[v]);
        if(!k) continue;
        ska(k,0,sizeof(cl_mem),&bx); ska(k,1,sizeof(cl_mem),&by); ska(k,2,sizeof(cl_mem),&bz);
        ska(k,3,sizeof(cl_uint),&iters);
        for(size_t z=0;z<(size_t)n;z++) hz[z]=0.0f;
        ewb(q,bz,CL_TRUE,0,bytes,hz,0,NULL,NULL);
        enk(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL); fin(q);
        erb(q,bz,CL_TRUE,0,16,ho,0,NULL,NULL);
        double expect=(double)iters;
        printf("%s GATE C0=%g expect=%g %s",tag[v],(double)ho[0],expect,((double)ho[0]==expect)?"PASS":"FAIL"); puts("");
        if((double)ho[0]!=expect){ puts("GATE_FAIL"); continue; }
        double t0=now_s(); for(int r=0;r<10;r++) enk(q,k,1,NULL,&gsz,&lsz,0,NULL,NULL); fin(q); double t1=now_s();
        printf("%s opts=\"%s\" reps=10 wall_s=%.6f gflops=%.3f freq=%d",tag[v],opts[v],t1-t0,10.0*flops/(t1-t0)/1e9,read_freq()); puts("");
    }
    puts("DONE");
    return 0;
}
