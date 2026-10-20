/* oclbin.c - E1: does CL_PROGRAM_BINARY differ between fma() and a*b+c?
 * One process, one queue, one device. For each arm: build kernel, query
 * CL_PROGRAM_BINARY_SIZES (0x1165), dump CL_PROGRAM_BINARIES (0x1166) to file,
 * and query CL_KERNEL_PRIVATE_MEM_SIZE (0x11B4) + CL_KERNEL_WORK_GROUP_SIZE (0x11B0)
 * as a fallback observable if binary is empty.
 * Build: aarch64-linux-android33-clang -O2 -o oclbin oclbin.c -ldl
 * No backslash-n in string literals; nl[] used.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

typedef void* cl_platform_id; typedef void* cl_device_id; typedef void* cl_context;
typedef void* cl_command_queue; typedef void* cl_program; typedef void* cl_kernel;
typedef void* cl_mem; typedef int cl_int; typedef unsigned int cl_uint; typedef unsigned long cl_bitfield;
#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_PROGRAM_BUILD_LOG 0x1183
#define CL_PROGRAM_BINARY_SIZES 0x1165
#define CL_PROGRAM_BINARIES 0x1166
#define CL_KERNEL_WORK_GROUP_SIZE 0x11B0
#define CL_KERNEL_PRIVATE_MEM_SIZE 0x11B4

typedef cl_int (*f_gpi)(cl_uint,cl_platform_id*,cl_uint*);
typedef cl_int (*f_gdi)(cl_platform_id,unsigned long,cl_uint,cl_device_id*,cl_uint*);
typedef cl_context (*f_cc)(const long*,cl_uint,const cl_device_id*,void*,void*,cl_int*);
typedef cl_command_queue (*f_ccq)(cl_context,cl_device_id,cl_bitfield,cl_int*);
typedef cl_program (*f_cp)(cl_context,cl_uint,const char**,const size_t*,cl_int*);
typedef cl_int (*f_bp)(cl_program,cl_uint,const cl_device_id*,const char*,void*,void*);
typedef cl_int (*f_gpbi)(cl_program,cl_device_id,cl_uint,size_t,void*,size_t*);
typedef cl_int (*f_gpi2)(cl_program,cl_uint,size_t,void*,size_t*);
typedef cl_kernel (*f_ck)(cl_program,const char*,cl_int*);
typedef cl_int (*f_gkwgi)(cl_kernel,cl_device_id,cl_uint,size_t,void*,size_t*);

static cl_device_id dev; static cl_context ctx;

static cl_program build(const char*src,const char*opts,const char*tag,cl_kernel*outk,
                        f_cp cp,f_bp bp,f_gpbi gpbi,f_gpi2 gpi2,f_ck ck,f_gkwgi gkwgi){
    cl_int err=0; const char*sp=src;
    cl_program prog=cp(ctx,1,&sp,NULL,&err);
    err=bp(prog,1,&dev,opts,NULL,NULL);
    if(err!=CL_SUCCESS){ char log[16384]; size_t z=0; gpbi(prog,dev,CL_PROGRAM_BUILD_LOG,sizeof(log),log,&z);
        printf("%s BUILD_FAIL rc=%d log=%s",tag,err,log); puts(""); return 0; }
    /* binary sizes */
    size_t bsz=0; cl_int rc=gpi2(prog,CL_PROGRAM_BINARY_SIZES,sizeof(bsz),&bsz,NULL);
    printf("%s BINARY_SIZE rc=%d size=%zu",tag,rc,bsz); puts("");
    if(bsz>0 && bsz< (size_t)64*1024*1024){
        unsigned char*buf=malloc(bsz); unsigned char*ptrs[1]; ptrs[0]=buf;
        cl_int rc2=gpi2(prog,CL_PROGRAM_BINARIES,sizeof(ptrs),ptrs,NULL);
        char fn[256]; snprintf(fn,sizeof(fn),"./oclbin_%s.bin",tag);
        FILE*f=fopen(fn,"wb"); if(f){ fwrite(buf,1,bsz,f); fclose(f); }
        /* count nonzero + first 16 bytes */
        size_t nz=0; for(size_t i=0;i<bsz;i++) if(buf[i]) nz++;
        printf("%s BINARY_DUMP rc=%d file=%s nonzero=%zu head=",tag,rc2,fn,nz);
        for(int i=0;i<16 && (size_t)i<bsz;i++) printf("%02x",buf[i]); puts("");
        free(buf);
    }
    cl_kernel k=ck(prog,"gk",&err);
    if(!k){ printf("%s KERNEL_FAIL rc=%d",tag,err); puts(""); return 0; }
    size_t pm=0,wgs=0;
    gkwgi(k,dev,CL_KERNEL_PRIVATE_MEM_SIZE,sizeof(pm),&pm,NULL);
    gkwgi(k,dev,CL_KERNEL_WORK_GROUP_SIZE,sizeof(wgs),&wgs,NULL);
    printf("%s PRIVATE_MEM=%zu WORKGROUP_MAX=%zu",tag,pm,wgs); puts("");
    *outk=k; return prog;
}

int main(void){
    setvbuf(stdout,0,_IONBF,0);
    void*lib=dlopen("libOpenCL.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib) lib=dlopen("/vendor/lib64/libOpenCL.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){ puts("dlopen fail"); return 1; }
    f_gpi gpi=(f_gpi)dlsym(lib,"clGetPlatformIDs"); f_gdi gdi=(f_gdi)dlsym(lib,"clGetDeviceIDs");
    f_cc cc=(f_cc)dlsym(lib,"clCreateContext"); f_ccq ccq=(f_ccq)dlsym(lib,"clCreateCommandQueue");
    f_cp cp=(f_cp)dlsym(lib,"clCreateProgramWithSource"); f_bp bp=(f_bp)dlsym(lib,"clBuildProgram");
    f_gpbi gpbi=(f_gpbi)dlsym(lib,"clGetProgramBuildInfo"); f_ck ck=(f_ck)dlsym(lib,"clCreateKernel");
    f_gpi2 gpi2=(f_gpi2)dlsym(lib,"clGetProgramInfo");
    f_gkwgi gkwgi=(f_gkwgi)dlsym(lib,"clGetKernelWorkGroupInfo");
    if(!gpi2){ puts("no clGetProgramInfo"); return 1; }
    cl_platform_id plat=0; cl_uint np=0; gpi(1,&plat,&np);
    cl_uint nd=0; gdi(plat,CL_DEVICE_TYPE_ALL,1,&dev,&nd);
    cl_int err=0; ctx=cc(NULL,1,&dev,NULL,NULL,&err);
    if(err!=CL_SUCCESS){ printf("ctx fail rc=%d",err); puts(""); return 1; }

    char nl[2]; nl[0]=10; nl[1]=0;
    char src[2048]; int o;
    const char*ops[3];
    /* A: fma */
    o=0;
    o+=snprintf(src+o,sizeof(src)-o,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  float4 xv=x[i]; float4 yv=y[i]; float4 zv=z[i];%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  for(uint t=0;t<iters;t++){ zv=fma(zv,xv,yv); }%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"}%s",nl);
    ops[0]="";
    cl_kernel kA=0, kB=0, kC=0;
    build(src,ops[0],"A_fma", &kA, cp,bp,gpbi,gpi2,ck,gkwgi);
    /* B: a*b+c */
    o=0;
    o+=snprintf(src+o,sizeof(src)-o,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  float4 xv=x[i]; float4 yv=y[i]; float4 zv=z[i];%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  for(uint t=0;t<iters;t++){ zv=zv*xv+yv; }%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"}%s",nl);
    build(src,ops[0],"B_muladd", &kB, cp,bp,gpbi,gpi2,ck,gkwgi);
    /* C: mad */
    o=0;
    o+=snprintf(src+o,sizeof(src)-o,"__kernel void gk(__global float4* x, __global float4* y, __global float4* z, uint iters){%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  uint i=get_global_id(0);%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  float4 xv=x[i]; float4 yv=y[i]; float4 zv=z[i];%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  for(uint t=0;t<iters;t++){ zv=mad(zv,xv,yv); }%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"  z[i]=zv;%s",nl);
    o+=snprintf(src+o,sizeof(src)-o,"}%s",nl);
    build(src,ops[0],"C_mad", &kC, cp,bp,gpbi,gpi2,ck,gkwgi);
    puts("DONE");
    return 0;
}
