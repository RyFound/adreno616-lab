	.arch armv8-a
	.file	"bench.c"
	.text
	.section	.text.cmp_d,"ax",@progbits
	.align	2
	.type	cmp_d, %function
cmp_d:
	ldr	d31, [x0]
	ldr	d30, [x1]
	fcmpe	d31, d30
	cset	w1, gt
	cset	w0, mi
	sub	w0, w1, w0
	ret
	.size	cmp_d, .-cmp_d
	.section	.text.pct_d,"ax",@progbits
	.align	2
	.type	pct_d, %function
pct_d:
	sub	w2, w1, #1
	fmov	d30, 5.0e-1
	scvtf	d31, w2
	fmadd	d30, d31, d0, d30
	fcvtzs	w3, d30
	bic	w3, w3, w3, asr #31
	cmp	w1, w3
	csel	w2, w2, w3, le
	ldr	d0, [x0, w2, sxtw 3]
	ret
	.size	pct_d, .-pct_d
	.section	.text.mem_kern_write,"ax",@progbits
	.align	2
	.p2align 5,,15
	.type	mem_kern_write, %function
mem_kern_write:
	lsr	x4, x2, 3
	cbz	x4, .L5
	cmp	x4, 1
	beq	.L9
	adrp	x1, .LC2
	and	x3, x2, -16
	add	x3, x3, x0
	lsr	x2, x2, 4
	ldr	q31, [x1, #:lo12:.LC2]
	mov	x1, x0
	.p2align 5,,15
.L7:
	str	q31, [x1], 16
	cmp	x3, x1
	bne	.L7
	lsl	x2, x2, 1
	cmp	x4, x2
	beq	.L5
.L6:
	mov	x1, 23130
	movk	x1, 0x5a5a, lsl 16
	str	x1, [x0, x2, lsl 3]
.L5:
	ret
.L9:
	mov	x2, 0
	b	.L6
	.size	mem_kern_write, .-mem_kern_write
	.section	.text.mem_kern_read,"ax",@progbits
	.align	2
	.p2align 5,,15
	.type	mem_kern_read, %function
mem_kern_read:
	cmp	x2, 31
	bls	.L18
	movi	v31.4s, 0
	lsr	x2, x2, 5
	mov	x0, 0
	mov	v30.16b, v31.16b
	.p2align 5,,15
.L17:
	ldp	q0, q29, [x1], 32
	add	x0, x0, 1
	add	v30.2d, v30.2d, v0.2d
	add	v31.2d, v31.2d, v29.2d
	cmp	x0, x2
	bcc	.L17
	add	v30.2d, v31.2d, v30.2d
	adrp	x0, mem_sink
	dup	d31, v30.d[1]
	add	d30, d31, d30
	str	d30, [x0, #:lo12:mem_sink]
	ret
	.p2align 2,,3
.L18:
	movi	v30.2d, #0
	adrp	x0, mem_sink
	str	d30, [x0, #:lo12:mem_sink]
	ret
	.size	mem_kern_read, .-mem_kern_read
	.section	.text.mem_kern_copy,"ax",@progbits
	.align	2
	.p2align 5,,15
	.type	mem_kern_copy, %function
mem_kern_copy:
	lsr	x4, x2, 3
	cbz	x4, .L21
	sub	x3, x4, #1
	cmp	x3, 6
	bls	.L28
	sub	x3, x0, x1
	cmp	x3, 8
	beq	.L28
	lsr	x5, x2, 4
	and	x2, x2, -16
	mov	x3, 0
	.p2align 5,,15
.L23:
	ldr	q31, [x1, x3]
	str	q31, [x0, x3]
	add	x3, x3, 16
	cmp	x2, x3
	bne	.L23
	cmp	x4, x5, lsl 1
	beq	.L21
	ldr	x3, [x1, x2]
	str	x3, [x0, x2]
.L21:
	ret
	.p2align 2,,3
.L28:
	mov	x2, 0
	.p2align 5,,15
.L25:
	ldr	x3, [x1, x2, lsl 3]
	str	x3, [x0, x2, lsl 3]
	add	x2, x2, 1
	cmp	x4, x2
	bne	.L25
	ret
	.size	mem_kern_copy, .-mem_kern_copy
	.section	.text.mem_kern_write_neon,"ax",@progbits
	.align	2
	.type	mem_kern_write_neon, %function
mem_kern_write_neon:
	lsr	x3, x2, 3
	and	x2, x2, -64
	movi	v31.16b, 0x5a
	mov	x1, x0
	add	x2, x2, x0
.L35:
	cmp	x1, x2
	bne	.L36
	and	x1, x3, -8
.L37:
	cmp	x1, x3
	bcc	.L38
	ret
.L36:
	add	x1, x1, 64
	stp	q31, q31, [x1, -64]
	stp	q31, q31, [x1, -32]
	b	.L35
.L38:
	lsl	x5, x1, 3
	add	x4, x0, x1, lsl 3
	mov	w2, 23130
	add	x1, x1, 1
	bfi	w2, w2, 16, 16
	str	w2, [x0, x5]
	str	w2, [x4, 4]
	b	.L37
	.size	mem_kern_write_neon, .-mem_kern_write_neon
	.section	.text.disk_xs,"ax",@progbits
	.align	2
	.type	disk_xs, %function
disk_xs:
	adrp	x1, disk_xr
	ldr	x0, [x1, #:lo12:disk_xr]
	eor	x0, x0, x0, lsl 13
	eor	x0, x0, x0, lsr 7
	eor	x0, x0, x0, lsl 17
	str	x0, [x1, #:lo12:disk_xr]
	ret
	.size	disk_xs, .-disk_xs
	.section	.text.mem_kern_copy_neon,"ax",@progbits
	.align	2
	.type	mem_kern_copy_neon, %function
mem_kern_copy_neon:
	lsr	x5, x2, 3
	and	x2, x2, -128
	mov	x3, x1
	mov	x4, x0
	add	x2, x2, x1
.L41:
	cmp	x3, x2
	bne	.L42
	and	x2, x5, -16
.L43:
	cmp	x2, x5
	bcc	.L44
	ret
.L42:
	ldr	q31, [x3]
	add	x4, x4, 128
	add	x3, x3, 128
	str	q31, [x4, -128]
	ldr	q31, [x3, -112]
	str	q31, [x4, -112]
	ldr	q31, [x3, -96]
	str	q31, [x4, -96]
	ldr	q31, [x3, -80]
	str	q31, [x4, -80]
	ldr	q31, [x3, -64]
	str	q31, [x4, -64]
	ldr	q31, [x3, -48]
	str	q31, [x4, -48]
	ldr	q31, [x3, -32]
	str	q31, [x4, -32]
	ldr	q31, [x3, -16]
	str	q31, [x4, -16]
	b	.L41
.L44:
	ldr	x3, [x1, x2, lsl 3]
	str	x3, [x0, x2, lsl 3]
	add	x2, x2, 1
	b	.L43
	.size	mem_kern_copy_neon, .-mem_kern_copy_neon
	.section	.text.mem_kern_read_neon,"ax",@progbits
	.align	2
	.type	mem_kern_read_neon, %function
mem_kern_read_neon:
	movi	v31.4s, 0
	lsr	x3, x2, 3
	and	x2, x2, -128
	mov	x0, x1
	add	x2, x1, x2
	mov	v30.16b, v31.16b
	mov	v29.16b, v31.16b
	mov	v28.16b, v31.16b
	mov	v27.16b, v31.16b
	mov	v26.16b, v31.16b
	mov	v25.16b, v31.16b
	mov	v24.16b, v31.16b
.L46:
	cmp	x0, x2
	bne	.L47
	add	v23.2d, v24.2d, v25.2d
	and	x0, x3, -16
	add	v7.2d, v26.2d, v27.2d
	add	v23.2d, v23.2d, v28.2d
	add	v7.2d, v7.2d, v29.2d
	add	v23.2d, v23.2d, v30.2d
	add	v7.2d, v7.2d, v31.2d
	add	v7.2d, v23.2d, v7.2d
	dup	d31, v7.d[1]
	add	d7, d31, d7
.L48:
	cmp	x0, x3
	bcc	.L49
	adrp	x0, mem_sink
	str	d7, [x0, #:lo12:mem_sink]
	ret
.L47:
	ldp	q6, q5, [x0]
	ldp	q4, q3, [x0, 32]
	ldp	q2, q1, [x0, 64]
	ldp	q0, q22, [x0, 96]
	add	x0, x0, 128
	add	v24.2d, v24.2d, v6.2d
	add	v25.2d, v25.2d, v5.2d
	add	v26.2d, v26.2d, v4.2d
	add	v27.2d, v27.2d, v3.2d
	add	v28.2d, v28.2d, v2.2d
	add	v29.2d, v29.2d, v1.2d
	add	v30.2d, v30.2d, v0.2d
	add	v31.2d, v31.2d, v22.2d
	b	.L46
.L49:
	ldr	d31, [x1, x0, lsl 3]
	add	x0, x0, 1
	add	d7, d7, d31
	b	.L48
	.size	mem_kern_read_neon, .-mem_kern_read_neon
	.section	.text.mem_kern_memcpy,"ax",@progbits
	.align	2
	.type	mem_kern_memcpy, %function
mem_kern_memcpy:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	mov	x20, x1
	bl	memcpy
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 32
	ret
	.size	mem_kern_memcpy, .-mem_kern_memcpy
	.section	.text.now,"ax",@progbits
	.align	2
	.type	now, %function
now:
	stp	x29, x30, [sp, -32]!
	mov	w0, 1
	mov	x29, sp
	add	x1, sp, 16
	bl	clock_gettime
	ldp	d0, d31, [sp, 16]
	mov	x0, 225833675390976
	movk	x0, 0x41cd, lsl 48
	fmov	d30, x0
	ldp	x29, x30, [sp], 32
	scvtf	d31, d31
	scvtf	d0, d0
	fdiv	d31, d31, d30
	fadd	d0, d31, d0
	ret
	.size	now, .-now
	.section	.text.mem_bw,"ax",@progbits
	.align	2
	.type	mem_bw, %function
mem_bw:
	stp	x29, x30, [sp, -80]!
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	mov	x21, x0
	adrp	x0, .LC3
	mov	w22, w4
	stp	x19, x20, [sp, 16]
	mov	x19, x3
	stp	x23, x24, [sp, 48]
	mov	x23, x1
	mov	x24, x2
	stp	d14, d15, [sp, 64]
	mov	w20, 0
	ldr	d15, [x0, #:lo12:.LC3]
.L55:
	cmp	w20, w22
	blt	.L56
	ucvtf	d0, x19
	mov	x0, 225833675390976
	movk	x0, 0x41cd, lsl 48
	fmov	d31, x0
	ldp	x19, x20, [sp, 16]
	fdiv	d0, d0, d31
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	fdiv	d0, d0, d15
	ldp	d14, d15, [sp, 64]
	ldp	x29, x30, [sp], 80
	ret
.L56:
	bl	now
	fmov	d14, d0
	mov	x2, x19
	mov	x1, x24
	mov	x0, x23
	add	w20, w20, 1
	blr	x21
	bl	now
	fsub	d0, d0, d14
	fcmpe	d15, d0
	fcsel	d15, d0, d15, gt
	b	.L55
	.size	mem_bw, .-mem_bw
	.section	.text.mem_th_run,"ax",@progbits
	.align	2
	.type	mem_th_run, %function
mem_th_run:
	stp	x29, x30, [sp, -32]!
	mov	x29, sp
	ldr	w4, [x0, 24]
	str	x19, [sp, 16]
	mov	x19, x0
	ldr	x3, [x0, 16]
	ldp	x1, x2, [x0]
	ldr	x0, [x0, 40]
	bl	mem_bw
	str	d0, [x19, 32]
	ldr	x19, [sp, 16]
	mov	x0, 0
	ldp	x29, x30, [sp], 32
	ret
	.size	mem_th_run, .-mem_th_run
	.section	.rodata.disk_seq_read.constprop.0.str1.1,"aMS",@progbits,1
.LC4:
	.string	"open r"
.LC5:
	.string	"read"
	.section	.text.disk_seq_read.constprop.0,"ax",@progbits
	.align	2
	.type	disk_seq_read.constprop.0, %function
disk_seq_read.constprop.0:
	stp	x29, x30, [sp, -96]!
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	mov	x22, x1
	lsl	w1, w2, 16
	str	d15, [sp, 48]
	bl	__open_2
	tbz	w0, #31, .L61
	adrp	x0, .LC4
	add	x0, x0, :lo12:.LC4
	bl	perror
.L62:
	fmov	d0, -1.0e+0
.L60:
	ldr	d15, [sp, 48]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x29, x30, [sp], 96
	ret
.L61:
	mov	w19, w0
	mov	x2, 1048576
	add	x0, sp, 88
	mov	x1, 4096
	bl	posix_memalign
	cbnz	w0, .L62
	ldr	x21, [sp, 88]
	bl	now
	fmov	d15, d0
	mov	x20, 0
.L65:
	cmp	x20, x22
	bcs	.L68
	mov	x1, x21
	mov	w0, w19
	mov	x2, 1048576
	bl	read
	cmp	x0, 0
	bgt	.L66
	beq	.L68
	adrp	x0, .LC5
	add	x0, x0, :lo12:.LC5
	bl	perror
.L68:
	bl	now
	fsub	d0, d0, d15
	mov	w0, w19
	str	d0, [sp, 72]
	bl	close
	mov	x0, x21
	bl	free
	ldr	d0, [sp, 72]
	b	.L60
.L66:
	add	x20, x20, x0
	b	.L65
	.size	disk_seq_read.constprop.0, .-disk_seq_read.constprop.0
	.section	.text.mem_mt.constprop.0,"ax",@progbits
	.align	2
	.type	mem_mt.constprop.0, %function
mem_mt.constprop.0:
	sub	sp, sp, #1024
	stp	x29, x30, [sp]
	mov	x29, sp
	stp	x25, x26, [sp, 64]
	mov	x26, x0
	sxtw	x0, w4
	stp	x23, x24, [sp, 48]
	add	x23, sp, 256
	mov	x25, x0
	udiv	x3, x3, x0
	stp	x19, x20, [sp, 16]
	mov	x20, x23
	stp	x21, x22, [sp, 32]
	mov	x22, x2
	mov	x21, x1
	adrp	x2, mem_th_run
	and	x19, x3, -64
	add	x2, x2, :lo12:mem_th_run
	stp	x27, x28, [sp, 80]
	add	x28, sp, 128
	mov	x24, 0
	mov	w27, 6
	str	d15, [sp, 96]
.L73:
	cmp	w25, w24
	bgt	.L74
	movi	d15, #0
	add	x20, sp, 128
	mov	x19, 0
.L75:
	cmp	w25, w19
	bgt	.L76
	ldp	x29, x30, [sp]
	fmov	d0, d15
	ldr	d15, [sp, 96]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	add	sp, sp, 1024
	ret
.L74:
	stp	x21, x22, [x20]
	mov	x3, x20
	add	x0, x28, w24, uxtw 3
	str	x19, [x20, 16]
	mov	x1, 0
	str	w27, [x20, 24]
	add	x24, x24, 1
	str	x26, [x20, 40]
	add	x21, x21, x19
	str	x2, [sp, 120]
	add	x22, x22, x19
	bl	pthread_create
	add	x20, x20, 48
	ldr	x2, [sp, 120]
	b	.L73
.L76:
	ldr	x0, [x20, x19, lsl 3]
	add	x23, x23, 48
	mov	x1, 0
	add	x19, x19, 1
	bl	pthread_join
	ldr	d31, [x23, -16]
	fadd	d15, d15, d31
	b	.L75
	.size	mem_mt.constprop.0, .-mem_mt.constprop.0
	.section	.rodata.run_mem.isra.0.str1.1,"aMS",@progbits,1
.LC6:
	.string	"== MEM: cap %zuMB  threads=%ld ==\n"
.LC7:
	.string	"memcpy"
.LC8:
	.string	"copy"
.LC9:
	.string	"write"
.LC10:
	.string	"size"
.LC11:
	.string	"%-10s %10s %10s %10s %10s\n"
.LC12:
	.string	"alloc"
.LC13:
	.string	"-- NEON kernels (explicit vld1q/vst1q) --"
.LC14:
	.string	"copy_n"
.LC15:
	.string	"read_n"
.LC16:
	.string	"write_n"
.LC17:
	.string	"%-10s %10s %10s %10s\n"
.LC18:
	.string	"%-10zu %8.1fGB/s %8.1fGB/s %8.1fGB/s %8.1fGB/s\n"
.LC19:
	.string	"%-10zu %8.1fGB/s %8.1fGB/s %8.1fGB/s\n"
.LC20:
	.string	"-- thread scaling @%zuMB (payload GB/s; copy moves 2x traffic) --\n"
.LC21:
	.string	"threads"
.LC22:
	.string	"%-8s %10s %10s %10s\n"
.LC23:
	.string	"%-8d %8.1fGB/s %8.1fGB/s %8.1fGB/s\n"
.LC24:
	.string	"-- random-access latency --"
.LC25:
	.string	"  %6zuKB: %6.1f ns/access\n"
	.section	.text.run_mem.isra.0,"ax",@progbits
	.align	2
	.type	run_mem.isra.0, %function
run_mem.isra.0:
	stp	x29, x30, [sp, -320]!
	mov	x1, 64
	mov	x29, sp
	stp	x21, x22, [sp, 32]
	lsl	x21, x0, 20
	mov	x22, x0
	mov	x2, x21
	add	x0, sp, 192
	stp	x19, x20, [sp, 16]
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	str	d15, [sp, 96]
	bl	posix_memalign
	cbnz	w0, .L79
	ldr	x20, [sp, 192]
	mov	x2, x21
	add	x0, sp, 184
	mov	x1, 64
	bl	posix_memalign
	mov	w24, w0
	cbnz	w0, .L79
	ldr	x19, [sp, 184]
	mov	x2, x21
	mov	w1, 1
	mov	x0, x20
	bl	memset
	adrp	x28, .LC9
	mov	x2, x21
	mov	w1, 2
	mov	x0, x19
	bl	memset
	mov	w0, 84
	add	x23, sp, 248
	bl	sysconf
	mov	x3, x0
	mov	x2, x22
	adrp	x1, .LC6
	mov	w0, 2
	add	x1, x1, :lo12:.LC6
	bl	__printf_chk
	adrp	x22, .LANCHOR0
	adrp	x0, .LC8
	add	x3, x28, :lo12:.LC9
	add	x5, x0, :lo12:.LC8
	adrp	x6, .LC7
	adrp	x0, .LC5
	add	x6, x6, :lo12:.LC7
	add	x4, x0, :lo12:.LC5
	adrp	x0, .LC10
	adrp	x1, .LC11
	add	x2, x0, :lo12:.LC10
	add	x1, x1, :lo12:.LC11
	mov	w0, 2
	bl	__printf_chk
	mov	x26, 0
	add	x1, x22, :lo12:.LANCHOR0
	mov	x0, x23
	mov	x2, 72
	bl	memcpy
	adrp	x0, mem_kern_write
	add	x0, x0, :lo12:mem_kern_write
	mov	x6, 1048575
	str	x0, [sp, 112]
	adrp	x0, mem_kern_read
	add	x0, x0, :lo12:mem_kern_read
	str	x0, [sp, 120]
.L80:
	ldr	x27, [x23, x26, lsl 3]
	cmp	x21, x27
	bcs	.L82
.L85:
	adrp	x0, .LC13
	add	x0, x0, :lo12:.LC13
	bl	puts
	adrp	x25, mem_kern_write_neon
	adrp	x0, .LC10
	adrp	x5, .LC14
	add	x2, x0, :lo12:.LC10
	add	x5, x5, :lo12:.LC14
	adrp	x4, .LC15
	adrp	x3, .LC16
	add	x4, x4, :lo12:.LC15
	add	x3, x3, :lo12:.LC16
	adrp	x1, .LC17
	mov	w0, 2
	add	x1, x1, :lo12:.LC17
	bl	__printf_chk
	add	x0, x25, :lo12:mem_kern_write_neon
	mov	x26, 0
	mov	x6, 1048575
	str	x0, [sp, 112]
	adrp	x0, mem_kern_read_neon
	add	x0, x0, :lo12:mem_kern_read_neon
	str	x0, [sp, 120]
.L83:
	ldr	x27, [x23, x26, lsl 3]
	cmp	x21, x27
	bcc	.L86
	cmp	x27, x6
	bls	.L106
	mov	x0, 33554431
	cmp	x27, x0
	mov	w4, 8
	mov	w0, 30
	csel	w25, w4, w0, hi
.L87:
	ldr	x0, [sp, 112]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	add	x26, x26, 1
	bl	mem_bw
	fmov	d15, d0
	ldr	x0, [sp, 120]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	bl	mem_bw
	str	d0, [sp, 128]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	adrp	x0, mem_kern_copy_neon
	add	x0, x0, :lo12:mem_kern_copy_neon
	bl	mem_bw
	fmov	d2, d0
	fmov	d0, d15
	ldr	d1, [sp, 128]
	lsr	x2, x27, 10
	adrp	x1, .LC19
	mov	w0, 2
	add	x1, x1, :lo12:.LC19
	bl	__printf_chk
	cmp	x26, 9
	mov	x6, 1048575
	bne	.L83
.L86:
	mov	x0, 67108864
	stp	x0, xzr, [sp, 200]
	add	x0, sp, 200
	str	x0, [sp, 112]
	adrp	x0, .LC20
	add	x0, x0, :lo12:.LC20
	mov	x26, 0
	str	x0, [sp, 144]
	adrp	x0, .LC21
	add	x0, x0, :lo12:.LC21
	str	x0, [sp, 152]
.L92:
	ldr	x0, [sp, 112]
	ldr	x27, [x0, x26, lsl 3]
	cmp	x27, 0
	csel	x27, x27, x21, ne
	cbz	x26, .L89
	mov	x0, 67108864
	cmp	x21, x0
	beq	.L90
.L89:
	ldr	x1, [sp, 144]
	lsr	x2, x27, 20
	mov	w0, 2
	adrp	x25, .LC23
	add	x25, x25, :lo12:.LC23
	mov	w23, 1
	bl	__printf_chk
	ldr	x2, [sp, 152]
	adrp	x0, .LC8
	add	x5, x0, :lo12:.LC8
	adrp	x0, .LC5
	add	x4, x28, :lo12:.LC9
	add	x3, x0, :lo12:.LC5
	adrp	x1, .LC22
	mov	w0, 2
	add	x1, x1, :lo12:.LC22
	bl	__printf_chk
	adrp	x0, mem_kern_read
	add	x0, x0, :lo12:mem_kern_read
	str	x0, [sp, 120]
	adrp	x0, mem_kern_write
	add	x0, x0, :lo12:mem_kern_write
	mov	w5, 4
	str	x0, [sp, 128]
	adrp	x0, mem_kern_copy
	add	x0, x0, :lo12:mem_kern_copy
	str	x0, [sp, 136]
.L91:
	ldr	x0, [sp, 120]
	mov	w4, w23
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	str	w5, [sp, 172]
	bl	mem_mt.constprop.0
	fmov	d15, d0
	ldr	x0, [sp, 128]
	mov	w4, w23
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	bl	mem_mt.constprop.0
	str	d0, [sp, 160]
	ldr	x0, [sp, 136]
	mov	w4, w23
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	bl	mem_mt.constprop.0
	fmov	d2, d0
	fmov	d0, d15
	ldr	d1, [sp, 160]
	mov	w2, w23
	mov	x1, x25
	mov	w0, 2
	lsl	w23, w23, 1
	bl	__printf_chk
	ldr	w5, [sp, 172]
	subs	w5, w5, #1
	bne	.L91
	add	x0, x26, 1
	mov	x26, 1
	cmp	x0, 2
	bne	.L92
.L90:
	add	x22, x22, :lo12:.LANCHOR0
	adrp	x0, .LC24
	add	x22, x22, 8
	add	x0, x0, :lo12:.LC24
	mov	x28, 33103
	bl	puts
	movk	x28, 0xf767, lsl 16
	add	x0, sp, 216
	ldp	q30, q31, [x22, 64]
	movk	x28, 0x7b7e, lsl 32
	mov	x22, 0
	movk	x28, 0x1405, lsl 48
	stp	q30, q31, [x0]
.L103:
	add	x0, sp, 216
	ldr	x23, [x0, x22, lsl 3]
	cmp	x21, x23
	bcc	.L93
	lsl	x25, x23, 3
	mov	x0, x25
	bl	malloc
	mov	x26, x0
	lsr	x0, x23, 3
	mov	x3, 0
.L94:
	cmp	x0, x3
	bne	.L95
	sub	x27, x3, #1
.L96:
	cbnz	x27, .L97
	lsl	x2, x3, 3
	stp	x2, x3, [sp, 112]
	mov	x0, x2
	bl	malloc
	mov	x1, x0
	ldp	x2, x3, [sp, 112]
	sub	x7, x26, #8
	mov	x6, 0
.L98:
	cmp	x3, x6
	bne	.L101
	mov	x3, x25
	mov	x0, x26
	str	x1, [sp, 112]
	bl	__memcpy_chk
	ldr	x0, [sp, 112]
	bl	free
	bl	now
	fmov	d15, d0
	mov	x0, 33920
	movk	x0, 0x1e, lsl 16
.L102:
	ldr	x27, [x26, x27, lsl 3]
	subs	x0, x0, #1
	bne	.L102
	bl	now
	fsub	d0, d0, d15
	adrp	x0, mem_sink
	lsr	x2, x23, 10
	adrp	x1, .LC25
	add	x1, x1, :lo12:.LC25
	str	x27, [x0, #:lo12:mem_sink]
	mov	x0, 145685290680320
	movk	x0, 0x413e, lsl 48
	fmov	d31, x0
	mov	x0, 225833675390976
	add	x22, x22, 1
	fdiv	d0, d0, d31
	movk	x0, 0x41cd, lsl 48
	fmov	d31, x0
	mov	w0, 2
	fmul	d0, d0, d31
	bl	__printf_chk
	mov	x0, x26
	bl	free
	cmp	x22, 4
	bne	.L103
.L93:
	mov	x0, x20
	bl	free
	mov	x0, x19
	bl	free
	b	.L78
.L79:
	mov	w24, 1
	adrp	x0, .LC12
	add	x0, x0, :lo12:.LC12
	bl	perror
.L78:
	ldr	d15, [sp, 96]
	mov	w0, w24
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	x29, x30, [sp], 320
	ret
.L82:
	cmp	x27, x6
	bls	.L104
	mov	x0, 33554431
	cmp	x27, x0
	mov	w4, 8
	mov	w0, 30
	csel	w25, w4, w0, hi
.L84:
	ldr	x0, [sp, 112]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	add	x26, x26, 1
	bl	mem_bw
	fmov	d15, d0
	ldr	x0, [sp, 120]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	bl	mem_bw
	str	d0, [sp, 136]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	adrp	x0, mem_kern_copy
	add	x0, x0, :lo12:mem_kern_copy
	bl	mem_bw
	str	d0, [sp, 128]
	mov	w4, w25
	mov	x3, x27
	mov	x2, x20
	mov	x1, x19
	adrp	x0, mem_kern_memcpy
	add	x0, x0, :lo12:mem_kern_memcpy
	bl	mem_bw
	fmov	d3, d0
	ldp	d2, d1, [sp, 128]
	fmov	d0, d15
	lsr	x2, x27, 10
	adrp	x1, .LC18
	mov	w0, 2
	add	x1, x1, :lo12:.LC18
	bl	__printf_chk
	cmp	x26, 9
	mov	x6, 1048575
	bne	.L80
	b	.L85
.L104:
	mov	w25, 200
	b	.L84
.L106:
	mov	w25, 200
	b	.L87
.L95:
	str	x3, [x26, x3, lsl 3]
	add	x3, x3, 1
	b	.L94
.L97:
	mov	x0, 32557
	add	x2, x27, 1
	movk	x0, 0x4c95, lsl 16
	movk	x0, 0xf42d, lsl 32
	movk	x0, 0x5851, lsl 48
	ldr	x6, [x26, x27, lsl 3]
	madd	x1, x27, x0, x28
	lsr	x1, x1, 33
	udiv	x0, x1, x2
	msub	w0, w0, w2, w1
	lsl	x0, x0, 3
	ldr	x1, [x26, x0]
	str	x1, [x26, x27, lsl 3]
	sub	x27, x27, #1
	str	x6, [x26, x0]
	b	.L96
.L101:
	add	x6, x6, 1
	cmp	x3, x6
	bls	.L99
	ldr	x0, [x26, x6, lsl 3]
.L100:
	ldr	x8, [x7, x6, lsl 3]
	str	x0, [x1, x8, lsl 3]
	b	.L98
.L99:
	ldr	x0, [x26]
	b	.L100
	.size	run_mem.isra.0, .-run_mem.isra.0
	.section	.rodata.run_disk.isra.0.str1.1,"aMS",@progbits,1
.LC26:
	.string	"== DISK: %s  size=%zuMB  randops=%d ==\n"
.LC27:
	.string	"open w"
.LC28:
	.string	"seq write BUFFERED | %8.1f MB/s | %.3f s\n"
.LC29:
	.string	"open rr"
.LC30:
	.string	"rand 4K read  DIRECT | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n"
.LC31:
	.string	"open rw"
.LC32:
	.string	"rand 4K write DIRECT | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n"
.LC33:
	.string	"open dsync"
.LC34:
	.string	"rand 4K write DSYNC  | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms  (NOT durable)\n"
.LC35:
	.string	"open wfsync"
.LC36:
	.string	"rand 4K write+FSYNC | IOPS %8.0f | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms  (true durable)\n"
.LC37:
	.string	"seq read  DIRECT     | %8.1f MB/s | %.3f s\n"
.LC38:
	.string	"seq read  BUFFERED   | %8.1f MB/s | %.3f s (page cache)\n"
.LC39:
	.string	"fsync       | p50 %.3f ms | p99 %.3f ms | p99.9 %.3f ms\n"
	.section	.text.run_disk.isra.0,"ax",@progbits
	.align	2
	.type	run_disk.isra.0, %function
run_disk.isra.0:
	stp	x29, x30, [sp, -288]!
	mov	w4, w2
	mov	x3, x1
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x19, x0
	stp	x21, x22, [sp, 32]
	mov	w22, w2
	lsl	x21, x1, 20
	mov	x2, x0
	mov	w0, 2
	stp	x23, x24, [sp, 48]
	mov	x24, x1
	adrp	x1, .LC26
	add	x1, x1, :lo12:.LC26
	stp	x25, x26, [sp, 64]
	stp	x27, x28, [sp, 80]
	stp	d8, d9, [sp, 96]
	stp	d10, d11, [sp, 112]
	stp	d12, d13, [sp, 128]
	stp	d14, d15, [sp, 144]
	bl	__printf_chk
	mov	x0, x19
	mov	w2, 420
	mov	w1, 66
	bl	open
	tbz	w0, #31, .L116
	adrp	x0, .LC27
	add	x0, x0, :lo12:.LC27
	bl	perror
.L117:
	fmov	d15, -1.0e+0
.L119:
	ucvtf	d13, x24
	fmov	d1, d15
	adrp	x1, .LC28
	mov	w0, 2
	add	x1, x1, :lo12:.LC28
	fdiv	d0, d13, d15
	bl	__printf_chk
	mov	x0, x19
	mov	w1, 65536
	bl	open
	mov	w25, w0
	tbz	w0, #31, .L123
	adrp	x0, .LC29
	add	x0, x0, :lo12:.LC29
	bl	perror
.L124:
	ldr	d31, [sp, 224]
	mov	x0, 70368744177664
	movk	x0, 0x408f, lsl 48
	fmov	d15, x0
	ldr	d0, [sp, 208]
	adrp	x1, .LC30
	fmul	d2, d14, d15
	fmul	d1, d10, d15
	fmul	d3, d31, d15
	add	x1, x1, :lo12:.LC30
	mov	w0, 2
	bl	__printf_chk
	mov	w1, 2
	mov	x0, x19
	movk	w1, 0x1, lsl 16
	bl	open
	mov	w25, w0
	tbz	w0, #31, .L129
	adrp	x0, .LC31
	add	x0, x0, :lo12:.LC31
	bl	perror
.L130:
	fmul	d3, d11, d15
	fmul	d2, d9, d15
	fmul	d1, d8, d15
	ldr	d0, [sp, 216]
	adrp	x1, .LC32
	mov	w0, 2
	add	x1, x1, :lo12:.LC32
	bl	__printf_chk
	mov	w1, 4098
	mov	x0, x19
	movk	w1, 0x1, lsl 16
	bl	open
	mov	w26, w0
	tbz	w0, #31, .L135
	adrp	x0, .LC33
	add	x0, x0, :lo12:.LC33
	bl	perror
.L136:
	ldr	d31, [sp, 200]
	adrp	x1, .LC34
	ldr	d0, [sp, 160]
	add	x1, x1, :lo12:.LC34
	mov	w0, 2
	fmul	d3, d31, d15
	ldr	d31, [sp, 232]
	fmul	d2, d31, d15
	ldr	d31, [sp, 192]
	fmul	d1, d31, d15
	bl	__printf_chk
	mov	w1, 2
	mov	x0, x19
	movk	w1, 0x1, lsl 16
	bl	open
	mov	w23, w0
	tbz	w0, #31, .L141
	adrp	x0, .LC35
	add	x0, x0, :lo12:.LC35
	bl	perror
.L142:
	ldp	d0, d31, [sp, 168]
	fmul	d1, d12, d15
	adrp	x1, .LC36
	mov	w0, 2
	add	x1, x1, :lo12:.LC36
	fmul	d3, d31, d15
	ldr	d31, [sp, 184]
	fmul	d2, d31, d15
	bl	__printf_chk
	mov	w2, 1
	mov	x1, x21
	mov	x0, x19
	bl	disk_seq_read.constprop.0
	fmov	d1, d0
	adrp	x1, .LC37
	fdiv	d0, d13, d0
	add	x1, x1, :lo12:.LC37
	mov	w0, 2
	bl	__printf_chk
	mov	w2, 0
	mov	x1, x21
	mov	x0, x19
	bl	disk_seq_read.constprop.0
	fmov	d1, d0
	adrp	x1, .LC38
	fdiv	d0, d13, d0
	add	x1, x1, :lo12:.LC38
	mov	w0, 2
	bl	__printf_chk
	mov	w1, 66
	mov	w2, 420
	movk	w1, 0x1, lsl 16
	mov	x0, x19
	bl	open
	mov	w23, w0
	mov	x2, 1048576
	add	x0, sp, 240
	mov	x1, 4096
	bl	posix_memalign
	cbnz	w0, .L145
	ldr	x22, [sp, 240]
	mov	x2, 1048576
	mov	w1, 1
	mov	x21, 0
	mov	x0, x22
	bl	memset
	mov	x0, 1600
	bl	malloc
	mov	x20, x0
.L146:
	ubfiz	x3, x21, 20, 32
	mov	x1, x22
	mov	x2, 1048576
	mov	w0, w23
	bl	pwrite
	bl	now
	fmov	d14, d0
	mov	w0, w23
	bl	fsync
	bl	now
	fsub	d0, d0, d14
	str	d0, [x20, x21, lsl 3]
	add	x21, x21, 1
	cmp	x21, 200
	bne	.L146
	mov	x2, 8
	mov	x1, x21
	mov	x0, x20
	adrp	x3, cmp_d
	add	x3, x3, :lo12:cmp_d
	bl	qsort
	ldr	d0, [x20, 800]
	adrp	x1, .LC39
	ldr	d1, [x20, 1576]
	add	x1, x1, :lo12:.LC39
	ldr	d2, [x20, 1592]
	mov	w0, 2
	fmul	d0, d0, d15
	fmul	d1, d1, d15
	fmul	d2, d2, d15
	bl	__printf_chk
	mov	w0, w23
	bl	close
	mov	x0, x22
	bl	free
	mov	x0, x20
	bl	free
.L145:
	mov	x0, x19
	bl	unlink
	ldp	d8, d9, [sp, 96]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	x27, x28, [sp, 80]
	ldp	d10, d11, [sp, 112]
	ldp	d12, d13, [sp, 128]
	ldp	d14, d15, [sp, 144]
	ldp	x29, x30, [sp], 288
	ret
.L116:
	mov	w20, w0
	mov	x2, 1048576
	add	x0, sp, 280
	mov	x1, 4096
	bl	posix_memalign
	cbnz	w0, .L117
	ldr	x23, [sp, 280]
	mov	x2, 1048576
	mov	w1, 165
	mov	x25, 0
	mov	x0, x23
	bl	memset
	bl	now
	fmov	d15, d0
.L120:
	cmp	x21, x25
	bhi	.L122
	mov	w0, w20
	bl	fsync
	bl	now
	fsub	d15, d0, d15
	mov	w0, w20
	bl	close
	mov	x0, x23
	bl	free
	b	.L119
.L122:
	mov	x1, x23
	mov	w0, w20
	mov	x2, 1048576
	bl	write
	tbz	x0, #63, .L121
	adrp	x0, .LC9
	add	x0, x0, :lo12:.LC9
	bl	perror
	mov	w0, w20
	bl	close
	mov	x0, x23
	bl	free
	b	.L117
.L121:
	add	x25, x25, x0
	b	.L120
.L123:
	mov	x2, 4096
	add	x0, sp, 272
	mov	x1, x2
	bl	posix_memalign
	mov	w24, w0
	cbnz	w0, .L124
	ldr	x26, [sp, 272]
	sbfiz	x0, x22, 3, 32
	bl	malloc
	mov	x23, x0
	bl	now
	fmov	d15, d0
	lsr	x27, x21, 12
	mov	x20, 0
.L126:
	cmp	w22, w24
	bgt	.L128
	bl	now
	fsub	d15, d0, d15
	mov	x1, x20
	mov	x0, x23
	mov	x2, 8
	adrp	x3, cmp_d
	add	x3, x3, :lo12:cmp_d
	bl	qsort
	fmov	d0, 5.0e-1
	mov	w1, w20
	mov	x0, x23
	bl	pct_d
	fmov	d10, d0
	adrp	x2, .LC40
	ldr	d0, [x2, #:lo12:.LC40]
	bl	pct_d
	fmov	d14, d0
	adrp	x2, .LC41
	ldr	d0, [x2, #:lo12:.LC41]
	bl	pct_d
	mov	w0, w25
	ucvtf	d31, x20
	str	d0, [sp, 224]
	fdiv	d31, d31, d15
	str	d31, [sp, 208]
	bl	close
	mov	x0, x26
	bl	free
	mov	x0, x23
	bl	free
	b	.L124
.L128:
	bl	disk_xs
	mov	x28, x0
	bl	now
	fmov	d10, d0
	mov	x1, x26
	mov	x2, 4096
	udiv	x3, x28, x27
	mov	w0, w25
	msub	x3, x3, x27, x28
	lsl	x3, x3, 12
	bl	pread
	mov	x28, x0
	bl	now
	cmp	x28, 0
	ble	.L127
	fsub	d0, d0, d10
	str	d0, [x23, x20, lsl 3]
	add	x20, x20, 1
.L127:
	add	w24, w24, 1
	b	.L126
.L129:
	mov	x2, 4096
	add	x0, sp, 264
	mov	x1, x2
	bl	posix_memalign
	mov	w24, w0
	cbnz	w0, .L130
	ldr	x26, [sp, 264]
	mov	x2, 4096
	mov	w1, 90
	lsr	x27, x21, 12
	mov	x0, x26
	bl	memset
	sbfiz	x0, x22, 3, 32
	bl	malloc
	mov	x23, x0
	bl	now
	fmov	d11, d0
	mov	x20, 0
.L132:
	cmp	w22, w24
	bgt	.L134
	bl	now
	fsub	d10, d0, d11
	mov	w0, w25
	bl	fsync
	mov	x1, x20
	mov	x0, x23
	mov	x2, 8
	adrp	x3, cmp_d
	add	x3, x3, :lo12:cmp_d
	bl	qsort
	fmov	d0, 5.0e-1
	mov	w1, w20
	mov	x0, x23
	bl	pct_d
	fmov	d8, d0
	adrp	x2, .LC40
	ldr	d0, [x2, #:lo12:.LC40]
	bl	pct_d
	fmov	d9, d0
	adrp	x2, .LC41
	ldr	d0, [x2, #:lo12:.LC41]
	bl	pct_d
	fmov	d11, d0
	ucvtf	d31, x20
	mov	w0, w25
	fdiv	d31, d31, d10
	str	d31, [sp, 216]
	bl	close
	mov	x0, x26
	bl	free
	mov	x0, x23
	bl	free
	b	.L130
.L134:
	bl	disk_xs
	mov	x28, x0
	bl	now
	fmov	d10, d0
	mov	x1, x26
	mov	x2, 4096
	udiv	x3, x28, x27
	mov	w0, w25
	msub	x3, x3, x27, x28
	lsl	x3, x3, 12
	bl	pwrite
	mov	x28, x0
	bl	now
	cmp	x28, 0
	ble	.L133
	fsub	d0, d0, d10
	str	d0, [x23, x20, lsl 3]
	add	x20, x20, 1
.L133:
	add	w24, w24, 1
	b	.L132
.L135:
	mov	x2, 4096
	add	x0, sp, 256
	mov	x1, x2
	bl	posix_memalign
	mov	w23, w0
	cbnz	w0, .L136
	mov	w24, 4
	mov	x2, 4096
	ldr	x25, [sp, 256]
	mov	w1, 60
	sdiv	w24, w22, w24
	lsr	x27, x21, 12
	mov	x0, x25
	bl	memset
	mov	x20, 0
	sbfiz	x0, x24, 3, 32
	bl	malloc
	mov	x22, x0
	bl	now
	fmov	d14, d0
.L138:
	cmp	w24, w23
	bgt	.L140
	bl	now
	fsub	d11, d0, d14
	mov	x1, x20
	mov	x0, x22
	mov	x2, 8
	adrp	x3, cmp_d
	add	x3, x3, :lo12:cmp_d
	bl	qsort
	fmov	d0, 5.0e-1
	mov	w1, w20
	mov	x0, x22
	ucvtf	d14, x20
	bl	pct_d
	str	d0, [sp, 192]
	adrp	x2, .LC40
	ldr	d0, [x2, #:lo12:.LC40]
	bl	pct_d
	str	d0, [sp, 232]
	adrp	x2, .LC41
	ldr	d0, [x2, #:lo12:.LC41]
	bl	pct_d
	mov	w0, w26
	fdiv	d31, d14, d11
	str	d0, [sp, 200]
	str	d31, [sp, 160]
	bl	close
	mov	x0, x25
	bl	free
	mov	x0, x22
	bl	free
	b	.L136
.L140:
	bl	disk_xs
	mov	x28, x0
	bl	now
	fmov	d11, d0
	mov	x1, x25
	mov	x2, 4096
	udiv	x3, x28, x27
	mov	w0, w26
	msub	x3, x3, x27, x28
	lsl	x3, x3, 12
	bl	pwrite
	mov	x28, x0
	bl	now
	cmp	x28, 0
	ble	.L139
	fsub	d0, d0, d11
	str	d0, [x22, x20, lsl 3]
	add	x20, x20, 1
.L139:
	add	w23, w23, 1
	b	.L138
.L141:
	mov	x2, 4096
	add	x0, sp, 248
	mov	x1, x2
	bl	posix_memalign
	cbnz	w0, .L142
	ldr	x24, [sp, 248]
	mov	x2, 4096
	mov	w1, 119
	lsr	x25, x21, 12
	mov	x0, x24
	bl	memset
	mov	x0, 16000
	bl	malloc
	mov	x20, x0
	bl	now
	fmov	d14, d0
	mov	x22, 0
.L144:
	bl	disk_xs
	mov	x26, x0
	bl	now
	fmov	d12, d0
	mov	x1, x24
	mov	x2, 4096
	udiv	x3, x26, x25
	mov	w0, w23
	msub	x3, x3, x25, x26
	lsl	x3, x3, 12
	bl	pwrite
	mov	w0, w23
	bl	fsync
	bl	now
	fsub	d0, d0, d12
	str	d0, [x20, x22, lsl 3]
	add	x22, x22, 1
	cmp	x22, 2000
	bne	.L144
	bl	now
	fsub	d14, d0, d14
	mov	x1, x22
	mov	x0, x20
	mov	x2, 8
	adrp	x3, cmp_d
	add	x3, x3, :lo12:cmp_d
	bl	qsort
	fmov	d0, 5.0e-1
	mov	w1, w22
	mov	x0, x20
	bl	pct_d
	fmov	d12, d0
	adrp	x2, .LC40
	ldr	d0, [x2, #:lo12:.LC40]
	bl	pct_d
	str	d0, [sp, 184]
	adrp	x2, .LC41
	ldr	d0, [x2, #:lo12:.LC41]
	bl	pct_d
	mov	x0, 70368744177664
	movk	x0, 0x409f, lsl 48
	fmov	d31, x0
	mov	w0, w23
	fdiv	d31, d31, d14
	stp	d31, d0, [sp, 168]
	bl	close
	mov	x0, x24
	bl	free
	mov	x0, x20
	bl	free
	b	.L142
	.size	run_disk.isra.0, .-run_disk.isra.0
	.section	.rodata.main.str1.1,"aMS",@progbits,1
.LC42:
	.string	"./bench.bin"
.LC43:
	.string	"all"
.LC44:
	.string	"mem"
.LC45:
	.string	"disk"
.LC46:
	.string	"./bench.%d.bin"
.LC47:
	.string	"usage: %s [mem|disk|all] [args...]\n"
.LC48:
	.string	"  mem  [sizeMB] [randops]\n  disk [path] [sizeMB] [randops]\n  all  [sizeMB] [randops]\n"
	.section	.text.startup.main,"ax",@progbits
	.align	2
	.global	main
	.type	main, %function
main:
	sub	sp, sp, #576
	stp	x29, x30, [sp]
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	mov	x20, x1
	stp	x21, x22, [sp, 32]
	mov	w21, w0
	str	x23, [sp, 48]
	cmp	w0, 1
	ble	.L158
	ldr	x22, [x20, 8]
	adrp	x1, .LC44
	add	x1, x1, :lo12:.LC44
	mov	x0, x22
	bl	strcmp
	cbnz	w0, .L152
	cmp	w21, 2
	beq	.L159
	ldr	x0, [x20, 16]
	bl	atol
.L153:
	ldp	x29, x30, [sp]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldr	x23, [sp, 48]
	add	sp, sp, 576
	b	run_mem.isra.0
.L159:
	mov	x0, 256
	b	.L153
.L152:
	mov	x0, x22
	adrp	x1, .LC45
	add	x1, x1, :lo12:.LC45
	bl	strcmp
	mov	w19, w0
	cbnz	w0, .L151
	cmp	w21, 2
	beq	.L160
	ldr	x23, [x20, 16]
	cmp	w21, 3
	beq	.L161
	ldr	x0, [x20, 24]
	bl	atol
	mov	x22, x0
	cmp	w21, 4
	beq	.L162
	ldr	x0, [x20, 32]
	bl	atoi
	mov	w2, w0
.L154:
	mov	x1, x22
	mov	x0, x23
.L166:
	bl	run_disk.isra.0
.L150:
	ldr	x23, [sp, 48]
	mov	w0, w19
	ldp	x29, x30, [sp]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	add	sp, sp, 576
	ret
.L160:
	adrp	x23, .LC42
	add	x23, x23, :lo12:.LC42
	mov	x22, 512
.L162:
	mov	w2, 20000
	b	.L154
.L161:
	mov	x22, 512
	b	.L162
.L158:
	adrp	x22, .LC43
	add	x22, x22, :lo12:.LC43
.L151:
	adrp	x1, .LC43
	mov	x0, x22
	add	x1, x1, :lo12:.LC43
	bl	strcmp
	cbnz	w0, .L156
	cmp	w21, 2
	ble	.L163
	ldr	x0, [x20, 16]
	bl	atol
	mov	x22, x0
	cmp	w21, 3
	beq	.L164
	ldr	x0, [x20, 24]
	bl	atoi
	mov	w20, w0
.L157:
	mov	x0, x22
	bl	run_mem.isra.0
	mov	w19, w0
	mov	w0, 10
	bl	putchar
	bl	getpid
	mov	w5, w0
	mov	x3, 512
	mov	w2, 2
	mov	x1, x3
	add	x0, sp, 64
	adrp	x4, .LC46
	add	x4, x4, :lo12:.LC46
	bl	__snprintf_chk
	mov	w2, w20
	mov	x1, x22
	add	x0, sp, 64
	b	.L166
.L163:
	mov	x22, 256
.L164:
	mov	w20, 20000
	b	.L157
.L156:
	adrp	x19, :got:stderr
	ldr	x19, [x19, :got_lo12:stderr]
	mov	w1, 2
	ldr	x3, [x20]
	adrp	x2, .LC47
	ldr	x0, [x19]
	add	x2, x2, :lo12:.LC47
	bl	__fprintf_chk
	ldr	x1, [x19]
	adrp	x0, .LC48
	mov	w19, 2
	add	x0, x0, :lo12:.LC48
	bl	fputs
	b	.L150
	.size	main, .-main
	.section	.data.disk_xr,"aw"
	.align	3
	.type	disk_xr, %object
	.size	disk_xr, 8
disk_xr:
	.xword	-7046029254386353131
	.section	.bss.mem_sink,"aw",@nobits
	.align	3
	.type	mem_sink, %object
	.size	mem_sink, 8
mem_sink:
	.zero	8
	.section	.rodata.cst16,"aM",@progbits,16
	.align	4
.LC2:
	.xword	1515870810
	.xword	1515870810
	.section	.rodata.cst8,"aM",@progbits,8
	.align	3
.LC3:
	.word	966823146
	.word	1177108057
	.align	3
.LC40:
	.word	2061584302
	.word	1072672276
	.align	3
.LC41:
	.word	-652835029
	.word	1072691150
	.section	.rodata
	.align	3
	.set	.LANCHOR0,. + 0
.LC0:
	.xword	8192
	.xword	32768
	.xword	131072
	.xword	524288
	.xword	2097152
	.xword	8388608
	.xword	33554432
	.xword	134217728
	.xword	268435456
.LC1:
	.xword	16384
	.xword	262144
	.xword	4194304
	.xword	67108864
	.section	.note.GNU-stack,"",@progbits
