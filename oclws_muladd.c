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
#define CL_PROFILING_COMMAND_START 0x1280
#define CL_PROFILING_COMMAND_END   0x1281
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
typedef cl_int (*f_gepi)(cl_event,cl_uint,size_t,void*,size_t*);
typedef cl_int (*f_re)(cl_event);
int main(int argc,char**argv){
    setvbuf(stdout,0,_IONBF,0);
    uint32_t n=65536, iters=2000, L=64;
    if(argc>=3){ n=(uint32_t)strtoul(argv[1],0,0); iters=(uint32_t)strtoul(argv[2],0,0); }
    void*lib=dlopen("libOpenCL.so",RTLD_NOW|RTLD_LOCAL); if(!lib) lib=dlopen("/vendor/lib64/libOpenCL.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){ puts("dlopen fail"); return 1; }
    f_gpi gpi=(f_gpi)dlsym(lib,"clGetPlatformIDs"); f_gdi gdi=(f_gdi)dlsym(lib,"clGetDeviceIDs");
    f_cc cc=(f_cc)dlsym(lib,"clCreateContext"); f_ccq ccq=(f_ccq)dlsym(lib,"clCreateCommandQueue");
    f_cb cb=(f_cb)dlsym(lib,"clCreateBuffer"); f_ewb ewb=(f_ewb)dlsym(lib,"clEnqueueWriteBuffer");
    f_cp cp=(f_cp)dlsym(lib,"clCreateProgramWithSource"); f_bp bp=(f_bp)dlsym(lib,"clBuildProgram");
    f_gpbi gpbi=(f_gpbi)dlsym(lib,"clGetProgramBuildInfo"); f_ck ck=(f_ck)dlsym(lib,"clCreateKernel");
    f_ska ska=(f_ska)dlsym(lib,"clSetKernelArg"); f_enk enk=(f_enk)dlsym(lib,"clEnqueueNDRangeKernel");
    f_fin fin=(f_fin)dlsym(lib,"clFinish"); f_erb erb=(f_erb)dlsym(lib,"clEnqueueReadBuffer");
    f_gepi gepi=(f_gepi)dlsym(lib,"clGetEventProfilingInfo"); f_re re=(f_re)dlsym(lib,"clReleaseEvent");
    cl_platform_id plat=0; cl_uint np=0; gpi(1,&plat,&np);
    cl_device_id dev=0; cl_uint nd=0; gdi(plat,CL_DEVICE_TYPE_ALL,1,&dev,&nd);
    cl_int err=0; cl_context ctx=cc(NULL,1,&dev,NULL,NULL,&err);
    cl_command_queue q=ccq(ctx,dev,CL_QUEUE_PROFILING_ENABLE,&err);
    if(err!=CL_SUCCESS){ puts("queue prof fail"); return 1; }
    size_t bytes=(size_t)n*16;
    float*hx=(float*)malloc(bytes),*hy=(float*)malloc(bytes),*hz=(float*)malloc(bytes),*ho=(float*)malloc(bytes);
    for(size_t k=0;k<(size_t)n*4;k++){hx[k]=1.0f;hy[k]=1.0f;hz[k]=0.0f;}
    cl_mem bx=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hx,&err);
    cl_mem by=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hy,&err);
    cl_mem bz=cb(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hz,&err);
    char nl[2]; nl[0]=10; nl[1]=0;
    char src[4096]; int off=0;
    off+=snprintf(src+off,sizeof(src)-off,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  uint i=get_global_id(0);%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  float4 xv=x[i]; float4 yv=y[i]; float4 zv=z[i];%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  for(uint t=0;t<iters;t++){ zv=zv*xv+yv; }%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"  z[i]=zv;%s",nl);
    off+=snprintf(src+off,sizeof(src)-off,"}%s",nl);
    const char*sp=src; cl_program prog=cp(ctx,1,&sp,NULL,&err);
    err=bp(prog,1,&dev,"",NULL,NULL);
    if(err!=CL_SUCCESS){ char log[8192]; size_t z=0; gpbi(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,&z); printf("BUILD FAIL %s",log); puts(""); return 1; }
    cl_kernel k=ck(prog,"gk",&err);
    ska(k,0,sizeof(cl_mem),&bx); ska(k,1,sizeof(cl_mem),&by); ska(k,2,sizeof(cl_mem),&bz); ska(k,3,sizeof(cl_uint),&iters);
    size_t gsz=(size_t)n; size_t lsz=(size_t)L; if(gsz%lsz) gsz+=(lsz-(gsz%lsz));
    double flops=(double)n*4.0*(double)iters*2.0;
    for(int r=0;r<3;r++){
        for(size_t z=0;z<(size_t)n*4;z++) hz[z]=0.0f;
        ewb(q,bz,CL_TRUE,0,bytes,hz,0,NULL,NULL);
        cl_event ev=NULL; double t0=now_s();
        enk(q,k,1,NULL,&gsz,&lsz,0,NULL,&ev);
        fin(q);
        double t1=now_s();
        unsigned long kns=0; size_t ks=0;
        if(gepi&&ev){ unsigned long a=0,b=0; gepi(ev,CL_PROFILING_COMMAND_START,sizeof(a),&a,&ks); gepi(ev,CL_PROFILING_COMMAND_END,sizeof(b),&b,&ks); kns=b-a; }
        erb(q,bz,CL_TRUE,0,16,ho,0,NULL,NULL); float g0=ho[0];
        printf("WS run=%d n=%u iters=%u wall_s=%.6f gflops=%.5f kernel_ns=%lu cur_freq=%d",r,n,iters,t1-t0,flops/(t1-t0)/1e9,kns,read_freq()); puts("");
        printf("GATE run=%d C0=%g expect=%g %s",r,(double)g0,(double)iters,(g0==(float)iters)?"PASS":"FAIL"); puts("");
        if(g0!=(float)iters){ puts("GATE_FAIL_EXIT"); return 1; }
        if(ev&&re) re(ev);
    }
    puts("DONE");
    return 0;
}
