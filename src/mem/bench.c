// bench - one binary: RAM bandwidth/latency + storage O_DIRECT/durability
// usage: bench [mem|disk|all] [args...]
//   bench mem  [sizeMB] [randops]            default 256 20000
//   bench disk [path] [sizeMB] [randops]     default /data/local/tmp/bench.bin 512 20000
//   bench all  [sizeMB] [randops]            runs mem on sizeMB, disk on path in cwd
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define ALIGN 4096

// ---------------- shared timing / stats ----------------
static inline double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec/1e9; }
static int cmp_d(const void*a,const void*b){ double x=*(double*)a,y=*(double*)b; return (x>y)-(x<y); }
static double pct_d(double*v,int n,double p){ int i=(int)(p*(n-1)+0.5); if(i<0)i=0; if(i>=n)i=n-1; return v[i]; }

// =====================================================================
// == MEMORY SUITE ==
// =====================================================================
static volatile uint64_t mem_sink;
static uint64_t mem_xr = 0x9E3779B97F4A7C15ULL;
static inline uint64_t mem_xs(void){ mem_xr^=mem_xr<<13; mem_xr^=mem_xr>>7; mem_xr^=mem_xr<<17; return mem_xr; }

#define HOT __attribute__((optimize("O3")))
HOT static void mem_kern_write(void*dst, const void*src, size_t n){ (void)src; uint64_t*v=dst; size_t k=n/8; for(size_t i=0;i<k;i++) v[i]=0x5A5A5A5AULL; __asm__ __volatile__(""::"r"(dst):"memory"); }
HOT static void mem_kern_read(void*dst, const void*src, size_t n){ (void)dst; const uint64_t*v=src; size_t k=n/8; uint64_t a=0,b=0,c=0,d=0; for(size_t i=0;i+3<k;i+=4){a+=v[i];b+=v[i+1];c+=v[i+2];d+=v[i+3];} mem_sink=a+b+c+d; }
HOT static void mem_kern_copy(void*dst,const void*src,size_t n){ uint64_t*d=dst; const uint64_t*s=src; size_t k=n/8; for(size_t i=0;i<k;i++) d[i]=s[i]; __asm__ __volatile__(""::"r"(d),"r"(s):"memory"); }
static void mem_kern_memcpy(void*dst,const void*src,size_t n){ memcpy(dst,src,n); __asm__ __volatile__(""::"r"(dst),"r"(src):"memory"); }

// ---- NEON kernels: explicit vector load/store with multi-accumulator ----
#if defined(__aarch64__)
#include <arm_neon.h>
static void mem_kern_write_neon(void*dst,const void*src,size_t n){ (void)src; uint64_t*v=dst; size_t k=n/8; uint64x2_t z=vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL); size_t i=0; for(;i+8<=k;i+=8){ vst1q_u64(v+i,z); vst1q_u64(v+i+2,z); vst1q_u64(v+i+4,z); vst1q_u64(v+i+6,z);} for(;i<k;i++) v[i]=0x5A5A5A5A5A5A5A5AULL; __asm__ __volatile__(""::"r"(dst):"memory"); }
static void mem_kern_read_neon(void*dst,const void*src,size_t n){ (void)dst; const uint64_t*v=src; size_t k=n/8; uint64x2_t a0=vdupq_n_u64(0),a1=a0,a2=a0,a3=a0,a4=a0,a5=a0,a6=a0,a7=a0; size_t i=0; for(;i+16<=k;i+=16){ a0=vaddq_u64(a0,vld1q_u64(v+i)); a1=vaddq_u64(a1,vld1q_u64(v+i+2)); a2=vaddq_u64(a2,vld1q_u64(v+i+4)); a3=vaddq_u64(a3,vld1q_u64(v+i+6)); a4=vaddq_u64(a4,vld1q_u64(v+i+8)); a5=vaddq_u64(a5,vld1q_u64(v+i+10)); a6=vaddq_u64(a6,vld1q_u64(v+i+12)); a7=vaddq_u64(a7,vld1q_u64(v+i+14)); } uint64x2_t s=vaddq_u64(vaddq_u64(vaddq_u64(a0,a1),vaddq_u64(a2,a3)),vaddq_u64(vaddq_u64(a4,a5),vaddq_u64(a6,a7))); uint64_t acc=vgetq_lane_u64(s,0)+vgetq_lane_u64(s,1); for(;i<k;i++) acc+=v[i]; mem_sink=acc; }
static void mem_kern_copy_neon(void*dst,const void*src,size_t n){ uint64_t*d=dst; const uint64_t*s=src; size_t k=n/8; size_t i=0; for(;i+16<=k;i+=16){ vst1q_u64(d+i,vld1q_u64(s+i)); vst1q_u64(d+i+2,vld1q_u64(s+i+2)); vst1q_u64(d+i+4,vld1q_u64(s+i+4)); vst1q_u64(d+i+6,vld1q_u64(s+i+6)); vst1q_u64(d+i+8,vld1q_u64(s+i+8)); vst1q_u64(d+i+10,vld1q_u64(s+i+10)); vst1q_u64(d+i+12,vld1q_u64(s+i+12)); vst1q_u64(d+i+14,vld1q_u64(s+i+14)); } for(;i<k;i++) d[i]=s[i]; __asm__ __volatile__(""::"r"(d),"r"(s):"memory"); }
// hand-written kernels (kernels.S): 8 accumulators / 128 B per iteration, no compiler heuristics
extern void mem_kern_write_asm(void*dst,const void*src,size_t n);
extern uint64_t mem_kern_read_asm(void*dst,const void*src,size_t n);
extern void mem_kern_copy_asm(void*dst,const void*src,size_t n);
static void mem_kern_read_asm_v(void*dst,const void*src,size_t n){ mem_sink=mem_kern_read_asm(dst,src,n); }
extern uint64_t mem_kern_read_pf(void*dst,const void*src,size_t n);
extern void mem_kern_copy_pf(void*dst,const void*src,size_t n);
static void mem_kern_read_pf_v(void*dst,const void*src,size_t n){ mem_sink=mem_kern_read_pf(dst,src,n); }
#endif

static double mem_bw(void(*fn)(void*,const void*,size_t), void*dst, void*src, size_t n, int reps){
    double best=1e30;
    for(int r=0;r<reps;r++){ double t0=now(); fn(dst,src,n); double dt=now()-t0; if(dt<best)best=dt; }
    return (double)n/1e9/best;
}

struct mem_th{ void*dst; void*src; size_t n; int reps; double gbs; void(*fn)(void*,const void*,size_t); };
static void* mem_th_run(void*arg){ struct mem_th*t=arg; t->gbs=mem_bw(t->fn,t->dst,t->src,t->n,t->reps); return NULL; }
// aggregate payload GB/s across threads (read/write ignore one operand)
static double mem_mt(void(*fn)(void*,const void*,size_t),void*dst,void*src,size_t n,int nthreads,int reps){
    pthread_t th[16]; struct mem_th a[16];
    size_t chunk=(n/nthreads)&~63ULL;
    for(int i=0;i<nthreads;i++){ a[i].dst=(char*)dst+chunk*i; a[i].src=(char*)src+chunk*i; a[i].n=chunk; a[i].reps=reps; a[i].fn=fn; pthread_create(&th[i],NULL,mem_th_run,&a[i]); }
    double sum=0; for(int i=0;i<nthreads;i++){ pthread_join(th[i],NULL); sum+=a[i].gbs; }
    return sum;
}

static void mem_chase_build(size_t*idx,size_t n){ for(size_t i=0;i<n;i++) idx[i]=i; for(size_t i=n-1;i>0;i--){ size_t j=(size_t)(( (uint64_t)i*6364136223846793005ULL + 1442695040888963407ULL)>>33)%(i+1); size_t t=idx[i];idx[i]=idx[j];idx[j]=t; } size_t*next=malloc(n*sizeof(size_t)); for(size_t i=0;i<n;i++) next[idx[i]]=(i+1<n)?idx[i+1]:idx[0]; memcpy(idx,next,n*sizeof(size_t)); free(next); }
static double mem_chase_ns(size_t*idx,size_t n,long iters){ size_t v=0; double t0=now(); for(long i=0;i<iters;i++){ v=idx[v]; } double dt=now()-t0; mem_sink=v; return dt/iters*1e9; }

static int run_mem(size_t MB, int rn_unused){
    (void)rn_unused;
    size_t total=MB*1024UL*1024UL;
    char *A,*B; if(posix_memalign((void**)&A,64,total)||posix_memalign((void**)&B,64,total)){perror("alloc");return 1;}
    memset(A,1,total); memset(B,2,total);
    printf("== MEM: cap %zuMB  threads=%ld ==\n",MB,(long)sysconf(_SC_NPROCESSORS_ONLN));
    printf("%-10s %10s %10s %10s %10s\n","size","write","read","copy","memcpy");
    size_t sizes[]={8UL<<10,32UL<<10,128UL<<10,512UL<<10,2UL<<20,8UL<<20,32UL<<20,128UL<<20,256UL<<20};
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        size_t n=sizes[i]; if(n>total)break;
        int reps=(n<(1UL<<20))?200:(n<(32UL<<20)?30:8);
        double wr=mem_bw(mem_kern_write,B,(void*)A,n,reps);
        double rd=mem_bw(mem_kern_read,B,(void*)A,n,reps);
        double cp=mem_bw(mem_kern_copy,B,(void*)A,n,reps);
        double mc=mem_bw(mem_kern_memcpy,B,(void*)A,n,reps);
        printf("%-10zu %8.1fGB/s %8.1fGB/s %8.1fGB/s %8.1fGB/s\n", n>>10, wr, rd, cp, mc);
    }
#if defined(__aarch64__)
    printf("-- NEON kernels (explicit vld1q/vst1q) --\n");
    printf("%-10s %10s %10s %10s\n","size","write_n","read_n","copy_n");
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        size_t n=sizes[i]; if(n>total)break;
        int reps=(n<(1UL<<20))?200:(n<(32UL<<20)?30:8);
        double wr=mem_bw(mem_kern_write_neon,B,(void*)A,n,reps);
        double rd=mem_bw(mem_kern_read_neon,B,(void*)A,n,reps);
        double cp=mem_bw(mem_kern_copy_neon,B,(void*)A,n,reps);
        printf("%-10zu %8.1fGB/s %8.1fGB/s %8.1fGB/s\n", n>>10, wr, rd, cp);
    }
    printf("-- ASM kernels (hand-written, 8 accum / 128B per iter) --\n");
    printf("%-10s %10s %10s %10s\n","size","write_a","read_a","copy_a");
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        size_t n=sizes[i]; if(n>total)break;
        int reps=(n<(1UL<<20))?200:(n<(32UL<<20)?30:8);
        double wr=mem_bw(mem_kern_write_asm,B,(void*)A,n,reps);
        double rd=mem_bw(mem_kern_read_asm_v,B,(void*)A,n,reps);
        double cp=mem_bw(mem_kern_copy_asm,B,(void*)A,n,reps);
        printf("%-10zu %8.1fGB/s %8.1fGB/s %8.1fGB/s\n", n>>10, wr, rd, cp);
    }
    printf("-- ASM + PRFM (prefetch 512B ahead) --\n");
    printf("%-10s %10s %10s %10s\n","size","read_pf","copy_pf","gain_rd");
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        size_t n=sizes[i]; if(n>total)break;
        int reps=(n<(1UL<<20))?200:(n<(32UL<<20)?30:8);
        double rd0=mem_bw(mem_kern_read_asm_v,B,(void*)A,n,reps);
        double rd=mem_bw(mem_kern_read_pf_v,B,(void*)A,n,reps);
        double cp=mem_bw(mem_kern_copy_pf,B,(void*)A,n,reps);
        printf("%-10zu %8.1fGB/s %8.1fGB/s %+7.1f%%\n", n>>10, rd, cp, (rd-rd0)/rd0*100.0);
    }
#endif
    size_t scale_sizes[2]={64UL<<20, 0}; /* 0 = whole buffer */
    for(int si=0;si<2;si++){
        size_t ns = scale_sizes[si]?scale_sizes[si]:total;
        if(si==1 && scale_sizes[0]==total) break; /* avoid duplicate pass */
        printf("-- thread scaling @%zuMB (payload GB/s; copy moves 2x traffic) --\n", ns>>20);
        printf("%-8s %10s %10s %10s\n","threads","read","write","copy");
        for(int t=1;t<=8;t*=2){
            double r=mem_mt(mem_kern_read,B,A,ns,t,6);
            double w=mem_mt(mem_kern_write,B,A,ns,t,6);
            double c=mem_mt(mem_kern_copy,B,A,ns,t,6);
            printf("%-8d %8.1fGB/s %8.1fGB/s %8.1fGB/s\n",t,r,w,c);
        }
    }
    printf("-- random-access latency --\n");
    size_t ls[]={16UL<<10,256UL<<10,4UL<<20,64UL<<20};
    for(size_t i=0;i<sizeof(ls)/sizeof(ls[0]);i++){ size_t n=ls[i]; if(n>total)break; size_t*idx=malloc(n*sizeof(size_t)); mem_chase_build(idx,n/8); double ns2=mem_chase_ns(idx,n/8,2000000); printf("  %6zuKB: %6.1f ns/access\n", n>>10, ns2); free(idx);} 
    free(A); free(B); return 0;
}

// =====================================================================
// == DISK SUITE ==
// =====================================================================
static uint64_t disk_xr = 0x9E3779B97F4A7C15ULL;
static inline uint64_t disk_xs(void){ disk_xr^=disk_xr<<13; disk_xr^=disk_xr>>7; disk_xr^=disk_xr<<17; return disk_xr; }

static int disk_open(const char*path,int direct){
    return open(path,O_RDWR|O_CREAT|(direct?O_DIRECT:0),0644);
}

static double disk_seq_write(const char*path,size_t total,size_t blk,int direct){
    int fd=disk_open(path,direct); if(fd<0){perror("open w");return -1;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return -1;}
    memset(buf,0xA5,blk);
    double t0=now(); size_t done=0;
    while(done<total){ ssize_t r=write(fd,buf,blk); if(r<0){perror("write");close(fd);free(buf);return -1;} done+=r; }
    if(!direct) fsync(fd);
    double t=now()-t0; close(fd); free(buf); return t;
}

static double disk_seq_read(const char*path,size_t total,size_t blk,int direct){
    int fd=open(path,O_RDONLY|(direct?O_DIRECT:0)); if(fd<0){perror("open r");return -1;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return -1;}
    double t0=now(); size_t done=0;
    while(done<total){ ssize_t r=read(fd,buf,blk); if(r<=0){ if(r<0)perror("read"); break;} done+=r; }
    double t=now()-t0; close(fd); free(buf); return t;
}

static void disk_rand_read(const char*path,size_t total,size_t blk,int nops,double*out){
    int fd=open(path,O_RDONLY|O_DIRECT); if(fd<0){perror("open rr");return;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return;}
    size_t maxblk=total/blk; double*lat=malloc(sizeof(double)*nops); size_t reads=0; double t0=now();
    for(int i=0;i<nops;i++){ uint64_t o=(disk_xs()%maxblk)*blk; double a=now(); ssize_t r=pread(fd,buf,blk,o); double b=now(); if(r>0) lat[reads++]=b-a; }
    double t=now()-t0; qsort(lat,reads,sizeof(double),cmp_d);
    out[0]=pct_d(lat,reads,0.50); out[1]=pct_d(lat,reads,0.99); out[2]=pct_d(lat,reads,0.999); out[3]=(double)reads/t;
    close(fd); free(buf); free(lat);
}

static void disk_rand_write(const char*path,size_t total,size_t blk,int nops,double*out){
    int fd=open(path,O_RDWR|O_DIRECT); if(fd<0){perror("open rw");return;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return;}
    memset(buf,0x5A,blk); size_t maxblk=total/blk; double*lat=malloc(sizeof(double)*nops); size_t wr=0; double t0=now();
    for(int i=0;i<nops;i++){ uint64_t o=(disk_xs()%maxblk)*blk; double a=now(); ssize_t r=pwrite(fd,buf,blk,o); double b=now(); if(r>0) lat[wr++]=b-a; }
    double t=now()-t0; fsync(fd); qsort(lat,wr,sizeof(double),cmp_d);
    out[0]=pct_d(lat,wr,0.50); out[1]=pct_d(lat,wr,0.99); out[2]=pct_d(lat,wr,0.999); out[3]=(double)wr/t;
    close(fd); free(buf); free(lat);
}

// O_DSYNC: NOT durable on this kernel (returns before flash) - kept to show the trap
static void disk_rand_dsync(const char*path,size_t total,size_t blk,int nops,double*out){
    int fd=open(path,O_RDWR|O_DIRECT|O_DSYNC); if(fd<0){perror("open dsync");return;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return;}
    memset(buf,0x3C,blk); size_t maxblk=total/blk; double*lat=malloc(sizeof(double)*nops); size_t wr=0; double t0=now();
    for(int i=0;i<nops;i++){ uint64_t o=(disk_xs()%maxblk)*blk; double a=now(); ssize_t r=pwrite(fd,buf,blk,o); double b=now(); if(r>0) lat[wr++]=b-a; }
    double t=now()-t0; qsort(lat,wr,sizeof(double),cmp_d);
    out[0]=pct_d(lat,wr,0.50); out[1]=pct_d(lat,wr,0.99); out[2]=pct_d(lat,wr,0.999); out[3]=(double)wr/t;
    close(fd); free(buf); free(lat);
}

// true durable: pwrite + fsync per op
static void disk_rand_fsync(const char*path,size_t total,size_t blk,int nops,double*out){
    int fd=open(path,O_RDWR|O_DIRECT); if(fd<0){perror("open wfsync");return;}
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return;}
    memset(buf,0x77,blk); size_t maxblk=total/blk; double*lat=malloc(sizeof(double)*nops); size_t wr=0; double t0=now();
    for(int i=0;i<nops;i++){ uint64_t o=(disk_xs()%maxblk)*blk; double a=now(); pwrite(fd,buf,blk,o); fsync(fd); double b=now(); lat[wr++]=b-a; }
    double t=now()-t0; qsort(lat,wr,sizeof(double),cmp_d);
    out[0]=pct_d(lat,wr,0.50); out[1]=pct_d(lat,wr,0.99); out[2]=pct_d(lat,wr,0.999); out[3]=(double)wr/t;
    close(fd); free(buf); free(lat);
}

static void disk_fsync_lat(const char*path,size_t blk,int n){
    int fd=open(path,O_RDWR|O_DIRECT|O_CREAT,0644);
    void*buf; if(posix_memalign(&buf,ALIGN,blk)){return;} memset(buf,1,blk);
    double*lat=malloc(sizeof(double)*n);
    for(int i=0;i<n;i++){ pwrite(fd,buf,blk,(size_t)(i%1024)*blk); double a=now(); fsync(fd); double b=now(); lat[i]=b-a; }
    qsort(lat,n,sizeof(double),cmp_d);
    printf("fsync       | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n", lat[n/2]*1e3, pct_d(lat,n,0.99)*1e3, pct_d(lat,n,0.999)*1e3);
    close(fd); free(buf); free(lat);
}

static int run_disk(const char*path,size_t MB,int rn){
    size_t total=MB*1024UL*1024UL;
    size_t seqblk=1024*1024, rblk=4096;
    printf("== DISK: %s  size=%zuMB  randops=%d ==\n",path,MB,rn);
    double t=disk_seq_write(path,total,seqblk,0);
    printf("seq write BUFFERED | %8.1f MB/s | %.3f s\n", MB/t, t);
    double rl[4]; disk_rand_read(path,total,rblk,rn,rl);
    printf("rand 4K read  DIRECT | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n", rl[3], rl[0]*1e3, rl[1]*1e3, rl[2]*1e3);
    double rw[4]; disk_rand_write(path,total,rblk,rn,rw);
    printf("rand 4K write DIRECT | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n", rw[3], rw[0]*1e3, rw[1]*1e3, rw[2]*1e3);
    double dw[4]; disk_rand_dsync(path,total,rblk,rn/4,dw);
    printf("rand 4K write DSYNC  | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms  (NOT durable)\n", dw[3], dw[0]*1e3, dw[1]*1e3, dw[2]*1e3);
    double fw[4]; disk_rand_fsync(path,total,rblk,2000,fw);
    printf("rand 4K write+FSYNC | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms  (true durable)\n", fw[3], fw[0]*1e3, fw[1]*1e3, fw[2]*1e3);
    t=disk_seq_read(path,total,seqblk,1);
    printf("seq read  DIRECT     | %8.1f MB/s | %.3f s\n", MB/t, t);
    t=disk_seq_read(path,total,seqblk,0);
    printf("seq read  BUFFERED   | %8.1f MB/s | %.3f s (page cache)\n", MB/t, t);
    disk_fsync_lat(path,seqblk,200);
    unlink(path);
    return 0;
}

// =====================================================================
int main(int argc,char**argv){
    const char*mode = argc>1?argv[1]:"all";
    if(!strcmp(mode,"mem")){
        size_t MB=argc>2?atol(argv[2]):256;
        int rn=argc>3?atoi(argv[3]):20000;
        return run_mem(MB,rn);
    }
    if(!strcmp(mode,"disk")){
        const char*path=argc>2?argv[2]:"./bench.bin"; /* /data/local/tmp unwritable by uid 1000 */
        size_t MB=argc>3?atol(argv[3]):512;
        int rn=argc>4?atoi(argv[4]):20000;
        return run_disk(path,MB,rn);
    }
    if(!strcmp(mode,"all")){
        size_t MB=argc>2?atol(argv[2]):256;
        int rn=argc>3?atoi(argv[3]):20000;
        int rc=run_mem(MB,rn);
        printf("\n");
        // disk path in cwd (tmp not writable by uid 1000); same /data partition
        char path[512]; snprintf(path,sizeof(path),"./bench.%d.bin",(int)getpid());
        rc|=run_disk(path,MB,rn);
        return rc;
    }
    fprintf(stderr,"usage: %s [mem|disk|all] [args...]\n",argv[0]);
    fprintf(stderr,"  mem  [sizeMB] [randops]\n  disk [path] [sizeMB] [randops]\n  all  [sizeMB] [randops]\n");
    return 2;
}
