/* bench_cpu.c - CPU FMA throughput: scalar, NEON(4-wide), multi-thread.
 * Workload matches the GPU kernels: z[i] = z[i]*x + y, x=y=1, z0=0 -> z[i]=iters.
 * Modes: scalar (1 lane/iter), neon (float32x4), and -t <threads> variants via pthreads.
 * Build: gcc -O3 -mcpu=cortex-a75+crc -o bench_cpu bench_cpu.c -lpthread
 *        (aarch64 host: any -march with asimd works; use -O3 -march=armv8.2-a+fp16)
 * Run:   ./bench_cpu <n> <iters> <reps> <scalar|neon|neonMT> [threads]
 * Gate: all n lanes == iters (exact; iters<2^24).
 * No backslash-n inside string literals (write_file mangles them); use puts().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <arm_neon.h>

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec*1e-9; }

static uint32_t N, ITERS;
static float *X, *Y, *Z;
static int MODE; /* 0 scalar, 1 neon */
static int NTHREADS = 1;

static void* worker(void* arg){
    long tid=(long)arg;
    size_t total=(size_t)N*4;
    size_t lo=(total*(size_t)tid)/NTHREADS;
    size_t hi=(total*(size_t)(tid+1))/NTHREADS;
    if(MODE==0){
        for(size_t i=lo;i<hi;i++){
            float z=Z[i], x=X[i], y=Y[i];
            for(uint32_t t=0;t<ITERS;t++){ z = z*x + y; }
            Z[i]=z;
        }
    } else {
        size_t i=lo;
        for(; i+4<=hi; i+=4){
            float32x4_t zv=vld1q_f32(&Z[i]);
            float32x4_t xv=vld1q_f32(&X[i]);
            float32x4_t yv=vld1q_f32(&Y[i]);
            for(uint32_t t=0;t<ITERS;t++){ zv = vmlaq_f32(yv, zv, xv); }
            vst1q_f32(&Z[i], zv);
        }
        for(; i<hi; i++){
            float z=Z[i];
            for(uint32_t t=0;t<ITERS;t++){ z = z*X[i] + Y[i]; }
            Z[i]=z;
        }
    }
    return NULL;
}

int main(int argc, char**argv){
    setvbuf(stdout,0,_IONBF,0);
    if(argc<5){ puts("usage: bench_cpu <n> <iters> <reps> <scalar|neon|neonMT> [threads]"); return 2; }
    N=(uint32_t)strtoul(argv[1],0,0);
    ITERS=(uint32_t)strtoul(argv[2],0,0);
    uint32_t reps=(uint32_t)strtoul(argv[3],0,0);
    const char*m=argv[4];
    if(strcmp(m,"neon")==0) MODE=1; else if(strcmp(m,"neonMT")==0){ MODE=1; } else MODE=0;
    if(argc>=6) NTHREADS=atoi(argv[5]);
    if(NTHREADS<1) NTHREADS=1;

    X=(float*)aligned_alloc(64,(size_t)N*4*sizeof(float));
    Y=(float*)aligned_alloc(64,(size_t)N*4*sizeof(float));
    Z=(float*)aligned_alloc(64,(size_t)N*4*sizeof(float));
    if(!X||!Y||!Z){ puts("alloc fail"); return 1; }

    for(uint32_t r=0;r<reps;r++){
        for(size_t i=0;i<(size_t)N*4;i++){ X[i]=1.0f; Y[i]=1.0f; Z[i]=0.0f; }
        double t0=now_s();
        pthread_t th[64];
        for(int t=1;t<NTHREADS;t++) pthread_create(&th[t],NULL,worker,(void*)(long)t);
        worker((void*)0);
        for(int t=1;t<NTHREADS;t++) pthread_join(th[t],NULL);
        double t1=now_s();
        float fh=Z[0]; int nbad=0;
        for(size_t i=0;i<(size_t)N*4;i++){ if(Z[i]!=(float)ITERS){ nbad++; } }
        double flops=(double)N*4.0*(double)ITERS*2.0;
        double wall=t1-t0; double gf=flops/wall/1e9;
        printf("GATE mode=%s n=%u iters=%u C0=%g expect=%u nbad=%d %s", m, N, ITERS, (double)fh, ITERS, nbad, (nbad==0)?"PASS":"FAIL"); puts("");
        if(nbad!=0){ puts("GATE_FAIL_EXIT"); return 1; }
        printf("RESULT mode=%s n=%u iters=%u reps=%u threads=%d wall_s=%.6f gflops=%.3f", m, N, ITERS, reps, NTHREADS, wall, gf); puts("");
    }
    puts("DONE");
    return 0;
}
