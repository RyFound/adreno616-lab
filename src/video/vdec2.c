/* vdec2.c - diagnosa REQBUFS EINVAL pada msm_vidc. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static int xioctl(int fd, unsigned long req, void*p){
  int r; do { r = ioctl(fd, req, p); } while (r == -1 && errno == EINTR); return r;
}

static void setfmt(int fd, unsigned type, unsigned pix, unsigned w, unsigned h, unsigned np, unsigned sz){
  struct v4l2_format f; memset(&f,0,sizeof f);
  f.type=type; f.fmt.pix_mp.width=w; f.fmt.pix_mp.height=h; f.fmt.pix_mp.pixelformat=pix;
  f.fmt.pix_mp.field=V4L2_FIELD_ANY; f.fmt.pix_mp.num_planes=np;
  if(np>0) f.fmt.pix_mp.plane_fmt[0].sizeimage=sz;
  int r=xioctl(fd,VIDIOC_S_FMT,&f);
  printf("  S_FMT t=%u pix=%c%c%c%c np=%u -> r=%d errno=%d got np=%u sz=%u\n",
    type,(pix)&255,(pix>>8)&255,(pix>>16)&255,(pix>>24)&255,np,r,errno,
    f.fmt.pix_mp.num_planes,f.fmt.pix_mp.plane_fmt[0].sizeimage);
}

static int try_req(int fd, unsigned type, unsigned cnt){
  struct v4l2_requestbuffers rb; memset(&rb,0,sizeof rb);
  rb.count=cnt; rb.type=type; rb.memory=V4L2_MEMORY_MMAP;
  errno=0;
  int r=xioctl(fd,VIDIOC_REQBUFS,&rb);
  printf("  REQBUFS t=%u want=%u -> r=%d errno=%d got=%u\n",type,cnt,r,errno,rb.count);
  return r;
}

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  printf("sizeof reqbuf=%zu buf=%zu fmt=%zu\n",
    sizeof(struct v4l2_requestbuffers), sizeof(struct v4l2_buffer), sizeof(struct v4l2_format));
  printf("VIDIOC_REQBUFS=0x%lx S_FMT=0x%lx\n",(unsigned long)VIDIOC_REQBUFS,(unsigned long)VIDIOC_S_FMT);

  int fd=open("/dev/video32",O_RDWR|O_NONBLOCK);
  if(fd<0){ printf("open errno=%d\n",errno); return 1; }
  puts("open ok");

  unsigned OUT=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  unsigned CAP=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

  puts("--- set formats (np=1 out, np=2 cap) ---");
  setfmt(fd,OUT,V4L2_PIX_FMT_H264,176,144,1,3734);
  setfmt(fd,CAP,V4L2_PIX_FMT_NV12,176,144,2,0);

  puts("--- capture-first order ---");
  unsigned counts[]={1,2,4,8,0};
  for(int i=0;counts[i];i++){ if(try_req(fd,CAP,counts[i])==0) break; }
  for(int i=0;counts[i];i++){ if(try_req(fd,OUT,counts[i])==0) break; }

  puts("--- try np=1 on capture too (fresh fd) ---");
  close(fd);
  fd=open("/dev/video32",O_RDWR|O_NONBLOCK);
  setfmt(fd,OUT,V4L2_PIX_FMT_H264,176,144,1,3734);
  setfmt(fd,CAP,V4L2_PIX_FMT_NV12,176,144,1,0);
  for(int i=0;counts[i];i++){ if(try_req(fd,OUT,counts[i])==0) break; }
  for(int i=0;counts[i];i++){ if(try_req(fd,CAP,counts[i])==0) break; }

  close(fd);
  return 0;
}
