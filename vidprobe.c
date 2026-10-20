/* vidprobe.c - enumerate V4L2 M2M codec capabilities from chroot (msm_vidc). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static const char* type_name(unsigned t){
  switch(t){
    case V4L2_BUF_TYPE_VIDEO_OUTPUT: return "OUTPUT";
    case V4L2_BUF_TYPE_VIDEO_CAPTURE: return "CAPTURE";
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE: return "OUTPUT_MPLANE";
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE: return "CAPTURE_MPLANE";
    default: return "?";
  }
}

static void probe(const char*path, unsigned buftype){
  int fd=open(path,O_RDWR|O_NONBLOCK);
  if(fd<0){ printf("%s: open fail errno=%d\n",path,errno); return; }
  printf("== %s (%s) ==\n",path,type_name(buftype));
  struct v4l2_fmtdesc f; memset(&f,0,sizeof(f)); f.type=buftype;
  int n=0;
  for(f.index=0; ioctl(fd,VIDIOC_ENUM_FMT,&f)==0; f.index++){
    printf("  fmt[%u] %c%c%c%c  %.32s  flags=0x%x\n", f.index,
      (f.pixelformat)&0xff,((f.pixelformat)>>8)&0xff,((f.pixelformat)>>16)&0xff,((f.pixelformat)>>24)&0xff,
      f.description, f.flags);
    n++;
    if(n>16) break;
  }
  if(n==0) printf("  (no formats; errno=%d)\n",errno);
  /* try G_FMT on the same type */
  struct v4l2_format fmt; memset(&fmt,0,sizeof(fmt)); fmt.type=buftype;
  if(ioctl(fd,VIDIOC_G_FMT,&fmt)==0){
    printf("  G_FMT width=%u height=%u sizeimage=%u\n", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.sizeimage);
  }
  close(fd);
}

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  probe("/dev/video32", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  probe("/dev/video32", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  probe("/dev/video32", V4L2_BUF_TYPE_VIDEO_OUTPUT);
  probe("/dev/video32", V4L2_BUF_TYPE_VIDEO_CAPTURE);
  probe("/dev/video33", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  probe("/dev/video33", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  return 0;
}
