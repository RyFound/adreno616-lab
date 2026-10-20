/* vdec.c - decode satu frame H.264 di /dev/video32 (msm_vidc vdec), V4L2 M2M.
   Kriteria lulus: buffer CAPTURE berisi YUV non-kosong, bukan sekadar DQBUF balik. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DEV "/dev/video32"
#define NPLANES 1

struct buf { void*addr[4]; size_t len[4]; unsigned nplanes; };

static void line(const char*tag, long v){ printf("%s=%ld", tag, v); putchar(10); }

static int xioctl(int fd, unsigned long req, void*p){
  int r;
  do { r = ioctl(fd, req, p); } while (r == -1 && errno == EINTR);
  return r;
}

static int s_fmt(int fd, unsigned type, unsigned pixfmt, unsigned w, unsigned h, unsigned szimg){
  struct v4l2_format f;
  memset(&f, 0, sizeof(f));
  f.type = type;
  f.fmt.pix_mp.width = w;
  f.fmt.pix_mp.height = h;
  f.fmt.pix_mp.pixelformat = pixfmt;
  f.fmt.pix_mp.field = V4L2_FIELD_ANY;
  f.fmt.pix_mp.num_planes = NPLANES;
  f.fmt.pix_mp.plane_fmt[0].sizeimage = szimg;
  int r = xioctl(fd, VIDIOC_S_FMT, &f);
  printf("S_FMT type=%u pix=%08x -> r=%d errno=%d got %ux%u num_planes=%u sizeimage=%u bytesperline=%u",
         type, pixfmt, r, errno, f.fmt.pix_mp.width, f.fmt.pix_mp.height,
         f.fmt.pix_mp.num_planes, f.fmt.pix_mp.plane_fmt[0].sizeimage,
         f.fmt.pix_mp.plane_fmt[0].bytesperline);
  putchar(10);
  return r;
}

int main(void){
  setvbuf(stdout, 0, _IONBF, 0);

  const char*path = getenv("VDEC_IN");
  if (!path) path = "/home/han/adreno616-lab/foreman.264";

  int fi = open(path, O_RDONLY);
  if (fi < 0) { printf("open input errno=%d", errno); putchar(10); return 1; }
  static unsigned char bs[1<<20];
  ssize_t n = read(fi, bs, sizeof(bs));
  close(fi);
  if (n <= 0) { puts("read input failed"); return 1; }
  line("input_bytes", n);

  /* SPS 176x144 diasumsikan; kalau S_FMT tolak, errno yang bicara. */
  unsigned W = 176, H = 144;

  int fd = open(DEV, O_RDWR | O_NONBLOCK);
  if (fd < 0) { printf("open %s errno=%d", DEV, errno); putchar(10); return 1; }
  puts("open vdec ok");

  if (s_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_PIX_FMT_H264, W, H, (unsigned)n) < 0) {
    puts("OUTPUT S_FMT failed");
  }
  if (s_fmt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_PIX_FMT_NV12, W, H, 0) < 0) {
    puts("CAPTURE S_FMT failed");
  }

  struct v4l2_requestbuffers rb;
  memset(&rb, 0, sizeof(rb));
  rb.count = 2; rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; rb.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd, VIDIOC_REQBUFS, &rb) < 0) { printf("OUTPUT REQBUFS errno=%d", errno); putchar(10); return 1; }
  line("OUT_REQBUFS_count", rb.count);
  unsigned nout = rb.count;

  memset(&rb, 0, sizeof(rb));
  rb.count = 4; rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; rb.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd, VIDIOC_REQBUFS, &rb) < 0) { printf("CAPTURE REQBUFS errno=%d", errno); putchar(10); return 1; }
  line("CAP_REQBUFS_count", rb.count);
  unsigned ncap = rb.count;

  struct buf out[8], cap[8];
  memset(out, 0, sizeof(out)); memset(cap, 0, sizeof(cap));

  struct v4l2_plane pl[4];
  struct v4l2_buffer b;

  for (unsigned i = 0; i < nout; i++){
    memset(&b, 0, sizeof(b)); memset(pl, 0, sizeof(pl));
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; b.memory = V4L2_MEMORY_MMAP;
    b.index = i; b.length = NPLANES; b.m.planes = pl;
    if (xioctl(fd, VIDIOC_QUERYBUF, &b) < 0) { printf("OUT QBUF-query errno=%d", errno); putchar(10); return 1; }
    out[i].nplanes = b.length;
    for (unsigned p = 0; p < b.length; p++){
      out[i].len[p] = pl[p].length;
      out[i].addr[p] = mmap(NULL, pl[p].length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, pl[p].m.mem_offset);
      if (out[i].addr[p] == MAP_FAILED) { printf("OUT mmap errno=%d", errno); putchar(10); return 1; }
    }
  }
  puts("out mmap ok");

  for (unsigned i = 0; i < ncap; i++){
    memset(&b, 0, sizeof(b)); memset(pl, 0, sizeof(pl));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; b.memory = V4L2_MEMORY_MMAP;
    b.index = i; b.length = NPLANES; b.m.planes = pl;
    if (xioctl(fd, VIDIOC_QUERYBUF, &b) < 0) { printf("CAP query errno=%d", errno); putchar(10); return 1; }
    cap[i].nplanes = b.length;
    for (unsigned p = 0; p < b.length; p++){
      cap[i].len[p] = pl[p].length;
      cap[i].addr[p] = mmap(NULL, pl[p].length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, pl[p].m.mem_offset);
      if (cap[i].addr[p] == MAP_FAILED) { printf("CAP mmap errno=%d", errno); putchar(10); return 1; }
    }
  }
  puts("cap mmap ok");

  /* isi bitstream ke OUTPUT buf 0 */
  memcpy(out[0].addr[0], bs, (size_t)n);
  memset(&b, 0, sizeof(b)); memset(pl, 0, sizeof(pl));
  b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; b.memory = V4L2_MEMORY_MMAP;
  b.index = 0; b.length = NPLANES; b.m.planes = pl;
  pl[0].bytesused = (unsigned)n; pl[0].length = out[0].len[0];
  pl[0].data_offset = 0;
  if (xioctl(fd, VIDIOC_QBUF, &b) < 0) { printf("OUT QBUF errno=%d", errno); putchar(10); return 1; }
  puts("out qbuf ok");

  for (unsigned i = 0; i < ncap; i++){
    memset(&b, 0, sizeof(b)); memset(pl, 0, sizeof(pl));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; b.memory = V4L2_MEMORY_MMAP;
    b.index = i; b.length = NPLANES; b.m.planes = pl;
    pl[0].length = cap[i].len[0];
    if (xioctl(fd, VIDIOC_QBUF, &b) < 0) { printf("CAP QBUF %u errno=%d", i, errno); putchar(10); return 1; }
  }
  puts("cap qbuf ok");

  unsigned ct = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (xioctl(fd, VIDIOC_STREAMON, &ct) < 0) { printf("STREAMON cap errno=%d", errno); putchar(10); return 1; }
  unsigned ot = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  if (xioctl(fd, VIDIOC_STREAMON, &ot) < 0) { printf("STREAMON out errno=%d", errno); putchar(10); return 1; }
  puts("streamon both ok");

  struct pollfd pfd = { .fd = fd, .events = POLLIN | POLLOUT };
  int pr = poll(&pfd, 1, 2000);
  printf("poll=%d revents=%04x", pr, pfd.revents); putchar(10);

  /* coba DQBUF capture */
  int got = 0;
  for (int tries = 0; tries < 4 && !got; tries++){
    memset(&b, 0, sizeof(b)); memset(pl, 0, sizeof(pl));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; b.memory = V4L2_MEMORY_MMAP;
    b.length = NPLANES; b.m.planes = pl;
    if (xioctl(fd, VIDIOC_DQBUF, &b) < 0){
      printf("DQBUF try=%d errno=%d", tries, errno); putchar(10);
      if (errno == EAGAIN) { usleep(200000); continue; }
      break;
    }
    got = 1;
    printf("DQBUF cap index=%u bytesused=%u data_offset=%u", b.index, pl[0].bytesused, pl[0].data_offset);
    putchar(10);

    unsigned char* y = (unsigned char*)cap[b.index].addr[0] + pl[0].data_offset;
    unsigned total = pl[0].bytesused ? pl[0].bytesused : cap[b.index].len[0];
    unsigned nz = 0; unsigned char mn = 255, mx = 0; long sum = 0;
    for (unsigned i = 0; i < total && i < 65536; i++){
      unsigned char v = y[i];
      if (v) nz++;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
      sum += v;
    }
    printf("Y: total=%u nonzero=%u min=%u max=%u sum=%ld first8=", total, nz, mn, mx, sum);
    for (unsigned i = 0; i < 8; i++) printf("%02x ", y[i]);
    putchar(10);
    if (nz > 0 && mx != mn) puts(">>> FRAME TER-DECODE: buffer YUV berisi data <<<");
    else puts("!!! buffer capture kosong/seragam - frame BELUM ter-decode !!!");
  }
  if (!got) puts("tidak ada frame keluar dalam jendela polling");

  unsigned ct2 = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  xioctl(fd, VIDIOC_STREAMOFF, &ct2);
  unsigned ot2 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  xioctl(fd, VIDIOC_STREAMOFF, &ot2);
  close(fd);
  return 0;
}
