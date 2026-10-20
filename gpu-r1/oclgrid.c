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
typedef int cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_ulong;
typedef unsigned long cl_bitfield;
typedef unsigned long cl_device_type;

#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_MEM_READ_WRITE (1<<0)
#define CL_MEM_COPY_HOST_PTR (1<<5)
#define CL_TRUE 1
#define CL_PROGRAM_BUILD_LOG 0x1183
#define CL_QUEUE_PROFILING_ENABLE (1<<1)

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec*1e-9; }
static int read_freq(void){ FILE*f=fopen("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq","r"); if(!f) return -1; long v=0; if(fscanf(f,"%ld",&v)!=1) v=-1; fclose(f); return (int)v; }

typedef cl_int (*fn_GetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int (*fn_GetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
typedef cl_context (*fn_CreateContext)(const long*, cl_uint, const cl_device_id*, void*, void*, cl_int*);
typedef cl_command_queue (*fn_CreateCommandQueue)(cl_context, cl_device_id, cl_bitfield, cl_int*);
typedef cl_mem (*fn_CreateBuffer)(cl_context, cl_bitfield, size_t, void*, cl_int*);
typedef cl_int (*fn_EnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_uint, size_t, size_t, const void*, cl_uint, const void*, void*);
typedef cl_program (*fn_CreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
typedef cl_int (*fn_BuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*, void*, void*);
typedef cl_int (*fn_GetProgramBuildInfo)(cl_program, cl_device_id, cl_uint, size_t, void*, size_t*);
typedef cl_int (*fn_GetDeviceInfo)(cl_device_id, cl_uint, size_t, void*, size_t*);
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_TYPE 0x1000
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
typedef cl_kernel (*fn_CreateKernel)(cl_program, const char*, cl_int*);
typedef cl_int (*fn_SetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
typedef cl_int (*fn_EnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const void*, void*);
typedef cl_int (*fn_Finish)(cl_command_queue);
typedef cl_int (*fn_EnqueueReadBuffer)(cl_command_queue, cl_mem, cl_uint, size_t, size_t, void*, cl_uint, const void*, void*);
typedef cl_int (*fn_ReleaseMemObject)(cl_mem);
typedef cl_int (*fn_ReleaseKernel)(cl_kernel);
typedef cl_int (*fn_ReleaseProgram)(cl_program);
typedef cl_int (*fn_ReleaseContext)(cl_context);
typedef cl_int (*fn_ReleaseCommandQueue)(cl_command_queue);

typedef struct {
    fn_GetPlatformIDs GetPlatformIDs;
    fn_GetDeviceIDs GetDeviceIDs;
    fn_CreateContext CreateContext;
    fn_CreateCommandQueue CreateCommandQueue;
    fn_CreateBuffer CreateBuffer;
    fn_EnqueueWriteBuffer EnqueueWriteBuffer;
    fn_CreateProgramWithSource CreateProgramWithSource;
    fn_BuildProgram BuildProgram;
    fn_GetProgramBuildInfo GetProgramBuildInfo;
    fn_GetDeviceInfo GetDeviceInfo;
    fn_CreateKernel CreateKernel;
    fn_SetKernelArg SetKernelArg;
    fn_EnqueueNDRangeKernel EnqueueNDRangeKernel;
    fn_Finish Finish;
    fn_EnqueueReadBuffer EnqueueReadBuffer;
} CLAPI;

static int build_kernel_src(char*buf, size_t cap, int K, const char*nl){
    int off=0;
    off+=snprintf(buf+off,cap-off,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    off+=snprintf(buf+off,cap-off,"  uint i = get_global_id(0);%s",nl);
    off+=snprintf(buf+off,cap-off,"  float4 xv = x[i]; float4 yv = y[i]; float4 zv = z[i];%s",nl);
    for(int j=0;j<K;j++) off+=snprintf(buf+off,cap-off,"  float4 a%d = zv;%s",j,nl);
    off+=snprintf(buf+off,cap-off,"  uint inner = iters / %d;%s",K,nl);
    off+=snprintf(buf+off,cap-off,"  for(uint t=0;t<inner;t++){%s",nl);
    for(int j=0;j<K;j++) off+=snprintf(buf+off,cap-off,"    a%d = fma(a%d, xv, yv);%s",j,j,nl);
    off+=snprintf(buf+off,cap-off,"  }%s",nl);
    off+=snprintf(buf+off,cap-off,"  float4 r = ");
    for(int j=0;j<K;j++){ off+=snprintf(buf+off,cap-off,"%s a%d", (j==0)?"":"+", j); }
    off+=snprintf(buf+off,cap-off,";%s",nl);
    off+=snprintf(buf+off,cap-off,"  z[i] = r;%s",nl);
    off+=snprintf(buf+off,cap-off,"}%s",nl);
    return off;
}

int main(int argc, char**argv){
    setvbuf(stdout,0,_IONBF,0);
    if(argc<4){ puts("usage: oclgrid <n> <iters> <reps>"); return 2; }
    uint32_t n=(uint32_t)strtoul(argv[1],0,0);
    uint32_t iters=(uint32_t)strtoul(argv[2],0,0);
    uint32_t reps=(uint32_t)strtoul(argv[3],0,0);
    int lsize_filter=0; { const char*e=getenv("OCL_LSIZE"); if(e) lsize_filter=atoi(e); }

    void*lib=dlopen("libOpenCL.so", RTLD_NOW|RTLD_LOCAL);
    if(!lib) lib=dlopen("/vendor/lib64/libOpenCL.so", RTLD_NOW|RTLD_LOCAL);
    if(!lib){ printf("dlopen libOpenCL fail: %s", dlerror()); puts(""); return 1; }

    CLAPI c; memset(&c,0,sizeof(c));
    #define LD(field,name) c.field=(fn_##field)dlsym(lib,name); if(!c.field){ printf("dlsym %s fail",name); puts(""); return 1; }
    LD(GetPlatformIDs,"clGetPlatformIDs");
    LD(GetDeviceIDs,"clGetDeviceIDs");
    LD(CreateContext,"clCreateContext");
    LD(CreateCommandQueue,"clCreateCommandQueue");
    LD(CreateBuffer,"clCreateBuffer");
    LD(EnqueueWriteBuffer,"clEnqueueWriteBuffer");
    LD(CreateProgramWithSource,"clCreateProgramWithSource");
    LD(BuildProgram,"clBuildProgram");
    LD(GetProgramBuildInfo,"clGetProgramBuildInfo");
    LD(GetDeviceInfo,"clGetDeviceInfo");
    LD(CreateKernel,"clCreateKernel");
    LD(SetKernelArg,"clSetKernelArg");
    LD(EnqueueNDRangeKernel,"clEnqueueNDRangeKernel");
    LD(Finish,"clFinish");
    LD(EnqueueReadBuffer,"clEnqueueReadBuffer");

    cl_platform_id plat=0; cl_uint np=0;
    if(c.GetPlatformIDs(1,&plat,&np)!=CL_SUCCESS || np<1){ puts("no platform"); return 1; }
    cl_device_id dev=0; cl_uint nd=0;
    if(c.GetDeviceIDs(plat,CL_DEVICE_TYPE_ALL,1,&dev,&nd)!=CL_SUCCESS || nd<1){ puts("no device"); return 1; }
    { char nm[256]; nm[0]=0; size_t ns=0; c.GetDeviceInfo(dev,CL_DEVICE_NAME,sizeof(nm),nm,&ns);
      cl_uint dt=0; size_t ds=0; c.GetDeviceInfo(dev,CL_DEVICE_TYPE,sizeof(dt),&dt,&ds);
      cl_uint cu=0; size_t cs=0; c.GetDeviceInfo(dev,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cu),&cu,&cs);
      cl_uint mf=0; size_t fs=0; c.GetDeviceInfo(dev,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(mf),&mf,&fs);
      printf("CLDEV name=%s type=0x%x maxcu=%u maxmhz=%u nplat=%u ndev=%u", nm, dt, cu, mf, np, nd); puts(""); }
    cl_int err=0;
    cl_context ctx=c.CreateContext(NULL,1,&dev,NULL,NULL,&err);
    if(err!=CL_SUCCESS || !ctx){ puts("ctx fail"); return 1; }
    cl_command_queue q=c.CreateCommandQueue(ctx,dev,0,&err);
    if(err!=CL_SUCCESS || !q){ puts("queue fail"); return 1; }

    size_t bytes=(size_t)n*16;
    void* hostx=malloc(bytes), *hosty=malloc(bytes), *hostz=malloc(bytes), *hostout=malloc(bytes);
    float*fx=(float*)hostx,*fy=(float*)hosty,*fz=(float*)hostz;
    for(size_t k=0;k<(size_t)n*4;k++){ fx[k]=1.0f; fy[k]=1.0f; fz[k]=0.0f; }
    cl_mem bx=c.CreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hostx,&err);
    cl_mem by=c.CreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hosty,&err);
    cl_mem bz=c.CreateBuffer(ctx,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,bytes,hostz,&err);
    if(!bx||!by||!bz){ puts("buffer fail"); return 1; }

    char nl[2]; nl[0]=10; nl[1]=0;

    int lsz[3]={64,128,256};
    int ks[4]={1,2,4,8};
    int runidx=0;

    for(int li=0;li<3;li++){
        if(lsize_filter && lsz[li]!=lsize_filter) continue;
        for(int ki=0;ki<4;ki++){
            int L=lsz[li]; int K=ks[ki];
            char cell[32]; snprintf(cell,sizeof(cell),"l%da%d",L,K);

            char src[8192]; build_kernel_src(src,sizeof(src),K,nl);
            const char*sp=src; cl_program prog=c.CreateProgramWithSource(ctx,1,&sp,NULL,&err);
            if(!prog){ printf("prog fail cell=%s",cell); puts(""); return 1; }
            err=c.BuildProgram(prog,1,&dev,"",NULL,NULL);
            if(err!=CL_SUCCESS){
                char log[8192]; size_t lsz2=0; c.GetProgramBuildInfo(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,&lsz2);
                printf("BUILD FAIL cell=%s err=%d log=%s",cell,err,log); puts(""); return 1;
            }
            cl_kernel k=c.CreateKernel(prog,"gk",&err);
            if(!k){ printf("kernel fail cell=%s",cell); puts(""); return 1; }

            // reset z to zeros before gate so z0 is exact (bz is reused across cells)
            for(size_t zk=0; zk<(size_t)n*4; zk++) ((float*)hostz)[zk]=0.0f;
            c.EnqueueWriteBuffer(q,bz,CL_TRUE,0,bytes,hostz,0,NULL,NULL);
            c.SetKernelArg(k,0,sizeof(cl_mem),&bx);
            c.SetKernelArg(k,1,sizeof(cl_mem),&by);
            c.SetKernelArg(k,2,sizeof(cl_mem),&bz);
            c.SetKernelArg(k,3,sizeof(cl_uint),&iters);

            size_t gsz=(size_t)n;
            size_t lsz_s=(size_t)L;
            size_t rem=gsz % lsz_s; if(rem) gsz += (lsz_s-rem);

            err=c.EnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz_s,0,NULL,NULL);
            if(err!=CL_SUCCESS){ printf("enqueue fail cell=%s err=%d",cell,err); puts(""); return 1; }
            c.Finish(q);
            c.EnqueueReadBuffer(q,bz,CL_TRUE,0,16,hostout,0,NULL,NULL);
            float g0=((float*)hostout)[0];
            float expect=(float)iters;
            printf("GATE cell=%s C0=%g expect=%g %s",cell,(double)g0,(double)expect,(g0==expect)?"PASS":"FAIL"); puts("");
            if(g0!=expect){ printf("GATE_FAIL_EXIT cell=%s",cell); puts(""); return 1; }

            double flops_rep=(double)n*4.0*(double)iters*2.0;
            for(uint32_t r=0;r<reps;r++){
                double t0=now_s();
                c.EnqueueNDRangeKernel(q,k,1,NULL,&gsz,&lsz_s,0,NULL,NULL);
                c.Finish(q);
                double t1=now_s();
                double wall=t1-t0;
                double gf=flops_rep/wall/1e9;
                printf("RESULT idx=%d cell=%s n=%u iters=%u reps=%u L=%d K=%d wall_s=%.6f gflops_wall=%.3f cur_freq=%d",runidx,cell,n,iters,reps,L,K,wall,gf,read_freq());
                puts("");
                runidx++;
            }
            fn_ReleaseKernel rk=(fn_ReleaseKernel)dlsym(lib,"clReleaseKernel"); if(rk) rk(k);
            fn_ReleaseProgram rp=(fn_ReleaseProgram)dlsym(lib,"clReleaseProgram"); if(rp) rp(prog);
        }
    }
    fn_ReleaseMemObject rm=(fn_ReleaseMemObject)dlsym(lib,"clReleaseMemObject"); if(rm){ rm(bx); rm(by); rm(bz); }
    fn_ReleaseCommandQueue rq=(fn_ReleaseCommandQueue)dlsym(lib,"clReleaseCommandQueue"); if(rq) rq(q);
    fn_ReleaseContext rc=(fn_ReleaseContext)dlsym(lib,"clReleaseContext"); if(rc) rc(ctx);
    puts("DONE");
    return 0;
}
