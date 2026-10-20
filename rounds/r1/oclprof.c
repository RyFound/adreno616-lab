#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dlfcn.h>

typedef void* cl_platform_id;
typedef void* cl_device_id;
typedef void* cl_context;
typedef void* cl_command_queue;
typedef void* cl_program;
typedef void* cl_kernel;
typedef void* cl_mem;
typedef void* cl_event;
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_bitfield;

#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_MEM_READ_WRITE (1<<0)
#define CL_MEM_COPY_HOST_PTR (1<<5)
#define CL_TRUE 1
#define CL_QUEUE_PROFILING_ENABLE (1<<1)
#define CL_PROGRAM_BUILD_LOG 0x1183
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_TYPE 0x1000
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
#define CL_PROFILING_COMMAND_START 0x1280
#define CL_PROFILING_COMMAND_END   0x1281

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec*1e-9; }
static int read_freq(void){ FILE*f=fopen("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq","r"); if(!f) return -1; long v=0; if(fscanf(f,"%ld",&v)!=1) v=-1; fclose(f); return (int)v; }

typedef cl_int (*fn_GetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int (*fn_GetDeviceIDs)(cl_platform_id, unsigned long, cl_uint, cl_device_id*, cl_uint*);
typedef cl_int (*fn_GetDeviceInfo)(cl_device_id, cl_uint, size_t, void*, size_t*);
typedef cl_context (*fn_CreateContext)(const long*, cl_uint, const cl_device_id*, void*, void*, cl_int*);
typedef cl_command_queue (*fn_CreateCommandQueue)(cl_context, cl_device_id, cl_bitfield, cl_int*);
typedef cl_mem (*fn_CreateBuffer)(cl_context, cl_bitfield, size_t, void*, cl_int*);
typedef cl_int (*fn_EnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_uint, size_t, size_t, const void*, cl_uint, const void*, void*);
typedef cl_program (*fn_CreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
typedef cl_int (*fn_BuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*, void*, void*);
typedef cl_int (*fn_GetProgramBuildInfo)(cl_program, cl_device_id, cl_uint, size_t, void*, size_t*);
typedef cl_kernel (*fn_CreateKernel)(cl_program, const char*, cl_int*);
typedef cl_int (*fn_SetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
typedef cl_int (*fn_EnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const void*, cl_event*);
typedef cl_int (*fn_Finish)(cl_command_queue);
typedef cl_int (*fn_EnqueueReadBuffer)(cl_command_queue, cl_mem, cl_uint, size_t, size_t, void*, cl_uint, const void*, void*);
typedef cl_int (*fn_GetEventProfilingInfo)(cl_event, cl_uint, size_t, void*, size_t*);
typedef cl_int (*fn_ReleaseEvent)(cl_event);

int main(int argc, char**argv){
    setvbuf(stdout,0,_IONBF,0);
    uint32_t n=65536, iters=2000, R=10, L=64;
    if(argc>=4){ n=(uint32_t)strtoul(argv[1],0,0); iters=(uint32_t)strtoul(argv[2],0,0); R=(uint32_t)strtoul(argv[3],0,0); }
    if(argc>=5){ L=(uint32_t)strtoul(argv[4],0,0); }

    void*lib=dlopen("libOpenCL.so", RTLD_NOW|RTLD_LOCAL);
    if(!lib) lib=dlopen("/vendor/lib64/libOpenCL.so", RTLD_NOW|RTLD_LOCAL);
    if(!lib){ printf("dlopen libOpenCL fail: %s", dlerror()); puts(""); return 1; }

    fn_GetPlatformIDs pGetPlatformIDs=(fn_GetPlatformIDs)dlsym(lib,"clGetPlatformIDs");
    fn_GetDeviceIDs pGetDeviceIDs=(fn_GetDeviceIDs)dlsym(lib,"clGetDeviceIDs");
    fn_GetDeviceInfo pGetDeviceInfo=(fn_GetDeviceInfo)dlsym(lib,"clGetDeviceInfo");
    fn_CreateContext pCreateContext=(fn_CreateContext)dlsym(lib,"clCreateContext");
    fn_CreateCommandQueue pCreateCommandQueue=(fn_CreateCommandQueue)dlsym(lib,"clCreateCommandQueue");
    fn_CreateBuffer pCreateBuffer=(fn_CreateBuffer)dlsym(lib,"clCreateBuffer");
    fn_EnqueueWriteBuffer pEnqueueWriteBuffer=(fn_EnqueueWriteBuffer)dlsym(lib,"clEnqueueWriteBuffer");
    fn_CreateProgramWithSource pCreateProgramWithSource=(fn_CreateProgramWithSource)dlsym(lib,"clCreateProgramWithSource");
    fn_BuildProgram pBuildProgram=(fn_BuildProgram)dlsym(lib,"clBuildProgram");
    fn_GetProgramBuildInfo pGetProgramBuildInfo=(fn_GetProgramBuildInfo)dlsym(lib,"clGetProgramBuildInfo");
    fn_CreateKernel pCreateKernel=(fn_CreateKernel)dlsym(lib,"clCreateKernel");
    fn_SetKernelArg pSetKernelArg=(fn_SetKernelArg)dlsym(lib,"clSetKernelArg");
    fn_EnqueueNDRangeKernel pEnqueueNDRangeKernel=(fn_EnqueueNDRangeKernel)dlsym(lib,"clEnqueueNDRangeKernel");
    fn_Finish pFinish=(fn_Finish)dlsym(lib,"clFinish");
    fn_EnqueueReadBuffer pEnqueueReadBuffer=(fn_EnqueueReadBuffer)dlsym(lib,"clEnqueueReadBuffer");
    fn_GetEventProfilingInfo pGetEventProfilingInfo=(fn_GetEventProfilingInfo)dlsym(lib,"clGetEventProfilingInfo");
    fn_ReleaseEvent pReleaseEvent=(fn_ReleaseEvent)dlsym(lib,"clReleaseEvent");
    if(!pGetPlatformIDs||!pGetDeviceIDs||!pCreateContext||!pCreateCommandQueue||!pEnqueueNDRangeKernel||!pFinish){ puts("dlsym core fail"); return 1; }

    cl_platform_id plat=0; cl_uint np=0;
    if(pGetPlatformIDs(1,&plat,&np)!=CL_SUCCESS||np<1){ puts("no platform"); return 1; }
    cl_device_id dev=0; cl_uint nd=0;
    if(pGetDeviceIDs(plat,CL_DEVICE_TYPE_ALL,1,&dev,&nd)!=CL_SUCCESS||nd<1){ puts("no device"); return 1; }
    { char nm[256]; nm[0]=0; size_t z=0; if(pGetDeviceInfo) pGetDeviceInfo(dev,CL_DEVICE_NAME,sizeof(nm),nm,&z);
      cl_uint dt=0; z=0; if(pGetDeviceInfo) pGetDeviceInfo(dev,CL_DEVICE_TYPE,sizeof(dt),&dt,&z);
      cl_uint cu=0; z=0; if(pGetDeviceInfo) pGetDeviceInfo(dev,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cu),&cu,&z);
      cl_uint mf=0; z=0; if(pGetDeviceInfo) pGetDeviceInfo(dev,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(mf),&mf,&z);
      printf("CLDEV name=%s type=0x%x maxcu=%u maxmhz=%u nplat=%u ndev=%u", nm, dt, cu, mf, np, nd); puts("");
      printf("PROF_SUPPORTED=%d", pGetEventProfilingInfo?1:0); puts("");
    }
    cl_int err=0;
    cl_context ctx=pCreateContext(NULL,1,&dev,NULL,NULL,&err);
    if(err!=CL_SUCCESS||!ctx){ puts("ctx fail"); return 1; }
    cl_command_queue q=pCreateCommandQueue(ctx,dev,CL_QUEUE_PROFILING_ENABLE,&err);
    if(err!=CL_SUCCESS||!q){ puts("queue fail (profiling flag rejected)"); return 1; }

    size_t bytes=(size_t)n*16;
    float*hx=(float*)malloc(bytes), *hy=(float*)malloc(bytes), *hz=(float*)malloc(bytes);
    float*ho=(float*)malloc(bytes);
    for(size_t k=0;k<(size_t)n*4;k++){ hx[k]=1.0f; hy[k]=1.0f; hz[k]=0.0f; }
    cl_mem bx=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hx,&err);
    cl_mem by=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hy,&err);
    cl_mem bz=pCreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hz,&err);
    if(!bx||!by||!bz){ puts("buffer fail"); return 1; }

    char nl[2]; nl[0]=10; nl[1]=0;
    char src[4096]; int off=0;
    off+=snprintf(src+off,sizeof(src)-off,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  uint i=get_global_id(0);%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  float4 xv=x[i]; float4 yv=y[i]; float4 zv=z[i];%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  for(uint t=0;t<iters;t++){ zv=fma(zv,xv,yv); }%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  z[i]=zv;%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"}%s",nl);

    const char*sp=src; cl_program prog=pCreateProgramWithSource(ctx,1,&sp,NULL,&err);
    if(!prog){ puts("prog fail"); return 1; }
    err=pBuildProgram(prog,1,&dev,"",NULL,NULL);
    if(err!=CL_SUCCESS){ char log[8192]; size_t z2=0; if(pGetProgramBuildInfo) pGetProgramBuildInfo(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,&z2); printf("BUILD FAIL err=%d log=%s",err,log); puts(""); return 1; }
    cl_kernel k=pCreateKernel(prog,"gk",&err);
    if(!k){ puts("kernel fail"); return 1; }
    pSetKernelArg(k,0,sizeof(cl_mem),&bx);
    pSetKernelArg(k,1,sizeof(cl_mem),&by);
    pSetKernelArg(k,2,sizeof(cl_mem),&bz);
    pSetKernelArg(k,3,sizeof(cl_uint),&iters);

    size_t gsz=(size_t)n; size_t lsz=(size_t)L;
    if(gsz%lsz) gsz += (lsz-(gsz%lsz));
    double flops_rep=(double)n*4.0*(double)iters*2.0;

    // ---- Mode SINGLE: 1 dispatch + finish, event profiling ----
    for(int r=0;r<3;r++){
        for(size_t z2=0; z2<(size_t)n*4; z2++) hz[z2]=0.0f;
        pEnqueueWriteBuffer(q,bz,CL_TRUE,0,bytes,hz,0,NULL,NULL);
        cl_event ev=NULL; double t0=now_s();
        err=pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,&ev);
        if(err!=CL_SUCCESS){ printf("enqueue err=%d",err); puts(""); return 1; }
        pFinish(q);
        double t1=now_s();
        unsigned long kns=0; size_t ks=0;
        if(pGetEventProfilingInfo && ev){ pGetEventProfilingInfo(ev,CL_PROFILING_COMMAND_START,sizeof(kns),&kns,&ks); unsigned long ke=0; pGetEventProfilingInfo(ev,CL_PROFILING_COMMAND_END,sizeof(ke),&ke,&ks); kns=ke-kns; }
        printf("MODE=single run=%d n=%u iters=%u L=%u wall_s=%.6f gflops_wall=%.4f kernel_ns=%lu cur_freq=%d", r, n, iters, L, t1-t0, flops_rep/(t1-t0)/1e9, kns, read_freq()); puts("");
        if(ev && pReleaseEvent) pReleaseEvent(ev);
        float g0=0; pEnqueueReadBuffer(q,bz,CL_TRUE,0,16,ho,0,NULL,NULL); g0=ho[0];
        printf("GATE mode=single run=%d C0=%g expect=%g %s", r, (double)g0, (double)iters, (g0==(float)iters)?"PASS":"FAIL"); puts("");
        if(g0!=(float)iters){ puts("GATE_FAIL_EXIT"); return 1; }
    }

    // ---- Mode PERREP: R dispatch, each followed by finish ----
    {
        for(size_t z2=0; z2<(size_t)n*4; z2++) hz[z2]=0.0f;
        pEnqueueWriteBuffer(q,bz,CL_TRUE,0,bytes,hz,0,NULL,NULL);
        unsigned long kns_sum=0; double t0=now_s();
        for(uint32_t r=0;r<R;r++){
            cl_event ev=NULL;
            pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,&ev);
            pFinish(q);
            if(pGetEventProfilingInfo && ev){ unsigned long ks0=0,ke=0; size_t ks=0; pGetEventProfilingInfo(ev,CL_PROFILING_COMMAND_START,sizeof(ks0),&ks0,&ks); pGetEventProfilingInfo(ev,CL_PROFILING_COMMAND_END,sizeof(ke),&ke,&ks); kns_sum += (ke-ks0); }
            if(ev && pReleaseEvent) pReleaseEvent(ev);
        }
        double t1=now_s();
        printf("MODE=perrep R=%u n=%u iters=%u L=%u wall_s=%.6f gflops_wall=%.4f kernel_ns_sum=%lu cur_freq=%d", R, n, iters, L, t1-t0, flops_rep*R/(t1-t0)/1e9, kns_sum, read_freq()); puts("");
        float g0=0; pEnqueueReadBuffer(q,bz,CL_TRUE,0,16,ho,0,NULL,NULL); g0=ho[0];
        float expect=(float)iters*(float)R;
        printf("GATE mode=perrep C0=%g expect=%g %s", (double)g0, (double)expect, (g0==expect)?"PASS":"FAIL"); puts("");
        if(g0!=expect){ puts("GATE_FAIL_EXIT"); return 1; }
    }

    // ---- Mode BATCH: R dispatch, ONE finish ----
    {
        for(size_t z2=0; z2<(size_t)n*4; z2++) hz[z2]=0.0f;
        pEnqueueWriteBuffer(q,bz,CL_TRUE,0,bytes,hz,0,NULL,NULL);
        cl_event evs[64]; unsigned long kns_sum=0; double t0=now_s();
        for(uint32_t r=0;r<R;r++){
            cl_event ev=NULL;
            pEnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz,0,NULL,&ev);
            evs[r]=ev;
        }
        pFinish(q);
        double t1=now_s();
        for(uint32_t r=0;r<R;r++){
            if(pGetEventProfilingInfo && evs[r]){ unsigned long ks0=0,ke=0; size_t ks=0; pGetEventProfilingInfo(evs[r],CL_PROFILING_COMMAND_START,sizeof(ks0),&ks0,&ks); pGetEventProfilingInfo(evs[r],CL_PROFILING_COMMAND_END,sizeof(ke),&ke,&ks); kns_sum += (ke-ks0); }
            if(evs[r] && pReleaseEvent) pReleaseEvent(evs[r]);
        }
        printf("MODE=batch R=%u n=%u iters=%u L=%u wall_s=%.6f gflops_wall=%.4f kernel_ns_sum=%lu cur_freq=%d", R, n, iters, L, t1-t0, flops_rep*R/(t1-t0)/1e9, kns_sum, read_freq()); puts("");
        float g0=0; pEnqueueReadBuffer(q,bz,CL_TRUE,0,16,ho,0,NULL,NULL); g0=ho[0];
        float expect=(float)iters*(float)R;
        printf("GATE mode=batch C0=%g expect=%g %s", (double)g0, (double)expect, (g0==expect)?"PASS":"FAIL"); puts("");
        if(g0!=expect){ puts("GATE_FAIL_EXIT"); return 1; }
    }
    puts("DONE");
    return 0;
}
