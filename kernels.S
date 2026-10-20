// kernels.S - hand-optimized NEON kernels untuk bench (aarch64).
// AAPCS64: v0-v7, v16-v31 caller-saved. v8-v15 callee-saved -> TIDAK dipakai.
// Semua kernel mengasumsikan n kelipatan 64 (write) / 128 (read,copy);
// ukuran benchmark (>=8KB) selalu memenuhi itu.

// void mem_kern_write_asm(void *dst, const void *src, size_t n)   x0=dst x2=n
    .section .text.mem_kern_write_asm,"ax",%progbits
    .globl  mem_kern_write_asm
    .type   mem_kern_write_asm, %function
mem_kern_write_asm:
    lsr     x3, x2, #6              // n/64
    cbz     x3, 2f
    movi    v0.4s, #0x5a            // 0x5A5A5A5A per 32-bit lane
    movi    v1.4s, #0x5a
    movi    v2.4s, #0x5a
    movi    v3.4s, #0x5a
    mov     x4, x0
1:  stp     q0, q1, [x4]           // 32 B
    stp     q2, q3, [x4, #32]       // +32 B  -> 64 B/iter
    add     x4, x4, #64
    subs    x3, x3, #1
    b.ne    1b
2:  ret
    .size   mem_kern_write_asm, .-mem_kern_write_asm

// uint64_t mem_kern_read_asm(void *dst, const void *src, size_t n)  x1=src x2=n
    .section .text.mem_kern_read_asm,"ax",%progbits
    .globl  mem_kern_read_asm
    .type   mem_kern_read_asm, %function
mem_kern_read_asm:
    lsr     x3, x2, #7              // n/128
    cbz     x3, 2f
    movi    v0.2d, #0
    movi    v1.2d, #0
    movi    v2.2d, #0
    movi    v3.2d, #0
    movi    v4.2d, #0
    movi    v5.2d, #0
    movi    v6.2d, #0
    movi    v7.2d, #0
1:  ldp     q16, q17, [x1], #32     // 4x ldp = 128 B/iter, 8 independent loads
    ldp     q18, q19, [x1], #32
    ldp     q20, q21, [x1], #32
    ldp     q22, q23, [x1], #32
    add     v0.2d, v0.2d, v16.2d     // 8 accumulator chains -> ILP, bukan 1 chain
    add     v1.2d, v1.2d, v17.2d
    add     v2.2d, v2.2d, v18.2d
    add     v3.2d, v3.2d, v19.2d
    add     v4.2d, v4.2d, v20.2d
    add     v5.2d, v5.2d, v21.2d
    add     v6.2d, v6.2d, v22.2d
    add     v7.2d, v7.2d, v23.2d
    subs    x3, x3, #1
    b.ne    1b
    add     v0.2d, v0.2d, v1.2d      // reduksi akhir (sekali per call)
    add     v2.2d, v2.2d, v3.2d
    add     v4.2d, v4.2d, v5.2d
    add     v6.2d, v6.2d, v7.2d
    add     v0.2d, v0.2d, v2.2d
    add     v4.2d, v4.2d, v6.2d
    add     v0.2d, v0.2d, v4.2d
    addp    d0, v0.2d
    fmov    x0, d0
    ret
2:  mov     x0, #0
    ret
    .size   mem_kern_read_asm, .-mem_kern_read_asm

// void mem_kern_copy_asm(void *dst, const void *src, size_t n)  x0=dst x1=src x2=n
    .section .text.mem_kern_copy_asm,"ax",%progbits
    .globl  mem_kern_copy_asm
    .type   mem_kern_copy_asm, %function
mem_kern_copy_asm:
    lsr     x3, x2, #7              // n/128
    cbz     x3, 2f
1:  ldp     q0, q1, [x1], #32       // 4x ldp + 4x stp = 128 B/iter
    ldp     q2, q3, [x1], #32
    ldp     q4, q5, [x1], #32
    ldp     q6, q7, [x1], #32
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x0], #32
    stp     q4, q5, [x0], #32
    stp     q6, q7, [x0], #32
    subs    x3, x3, #1
    b.ne    1b
2:  ret
    .size   mem_kern_copy_asm, .-mem_kern_copy_asm

// ---- prefetch variants: prfm pldl1keep, jarak 512 B ke depan ----
// Latency-hiding: isi pipeline sebelum load benar-benar dipakai.
// Buffer benchmark >= 8 KB, jadi prefetch 512 B ke depan masih dalam alokasi
// kecuali pada iterasi terakhir; aman karena hanya hint (tidak fault).

// uint64_t mem_kern_read_pf(void *dst, const void *src, size_t n)
    .section .text.mem_kern_read_pf,"ax",%progbits
    .globl  mem_kern_read_pf
    .type   mem_kern_read_pf, %function
mem_kern_read_pf:
    lsr     x3, x2, #7
    cbz     x3, 2f
    movi    v0.2d, #0
    movi    v1.2d, #0
    movi    v2.2d, #0
    movi    v3.2d, #0
    movi    v4.2d, #0
    movi    v5.2d, #0
    movi    v6.2d, #0
    movi    v7.2d, #0
1:  prfm    pldl1keep, [x1, #512]     // hint: tarik 8 baris ke depan
    prfm    pldl1keep, [x1, #576]
    ldp     q16, q17, [x1], #32
    ldp     q18, q19, [x1], #32
    ldp     q20, q21, [x1], #32
    ldp     q22, q23, [x1], #32
    add     v0.2d, v0.2d, v16.2d
    add     v1.2d, v1.2d, v17.2d
    add     v2.2d, v2.2d, v18.2d
    add     v3.2d, v3.2d, v19.2d
    add     v4.2d, v4.2d, v20.2d
    add     v5.2d, v5.2d, v21.2d
    add     v6.2d, v6.2d, v22.2d
    add     v7.2d, v7.2d, v23.2d
    subs    x3, x3, #1
    b.ne    1b
    add     v0.2d, v0.2d, v1.2d
    add     v2.2d, v2.2d, v3.2d
    add     v4.2d, v4.2d, v5.2d
    add     v6.2d, v6.2d, v7.2d
    add     v0.2d, v0.2d, v2.2d
    add     v4.2d, v4.2d, v6.2d
    add     v0.2d, v0.2d, v4.2d
    addp    d0, v0.2d
    fmov    x0, d0
    ret
2:  mov     x0, #0
    ret
    .size   mem_kern_read_pf, .-mem_kern_read_pf

// void mem_kern_copy_pf(void *dst, const void *src, size_t n)
    .section .text.mem_kern_copy_pf,"ax",%progbits
    .globl  mem_kern_copy_pf
    .type   mem_kern_copy_pf, %function
mem_kern_copy_pf:
    lsr     x3, x2, #7
    cbz     x3, 2f
1:  prfm    pldl1keep, [x1, #512]     // prefetch src
    prfm    pstl1keep, [x0, #512]     // prefetch dst untuk tulis
    ldp     q0, q1, [x1], #32
    ldp     q2, q3, [x1], #32
    ldp     q4, q5, [x1], #32
    ldp     q6, q7, [x1], #32
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x0], #32
    stp     q4, q5, [x0], #32
    stp     q6, q7, [x0], #32
    subs    x3, x3, #1
    b.ne    1b
2:  ret
    .size   mem_kern_copy_pf, .-mem_kern_copy_pf
