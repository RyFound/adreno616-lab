/* vdec3.c - QUERYCAP + REQBUFS count=0/DMABUF + cek /dev/video33. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static int xioctl(int fd, unsigned long r, void*p){ int v; do{v=ioctl(fd,r,p);}while(v==-1&&errno==EINTR); return v; }

static void cap(int fd){
  struct v4l2_capability c; memset(&c,0,sizeof c);
  if(xioctl(fd,VIDIOC_QUERYCAP,&c)<0){ printf("  QUERYCAP errno=%d\n",errno); return; }
  printf("  QUERYCAP driver=%s card=%s bus=%s caps=0x%08x devcaps=0x%08x\n",
    c.driver,c.card,c.bus_info,c.capabilities,c.device_caps);
  printf("  M2M_MPLANE=%d STREAMING=%d\n",
    !!(c.device_caps&V4L2_CAP_VIDEO_M2M_MPLANE), !!(c.device_caps&V4L2_CAP_STREAMING));
}

static void req(int fd, unsigned t, unsigned cnt, unsigned mem, const char*tag){
  struct v4l2_requestbuffers rb; memset(&rb,0,sizeof rb);
  rb.count=cnt; rb.type=t; rb.memory=mem; errno=0;
  int r=xioctl(fd,VIDIOC_REQBUFS,&rb);
  printf("  %s t=%u cnt=%u mem=%u -> r=%d errno=%d got=%u\n",tag,t,cnt,mem,r,errno,rb.count);
}

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  const char*devs[]={"/dev/video32","/dev/video33",NULL};
  for(int i=0;devs[i];i++){
    printf("=== %s ===\n",devs[i]);
    int fd=open(devs[i],O_RDWR|O_NONBLOCK);
    if(fd<0){ printf("  open errno=%d\n",errno); continue; }
    cap(fd);
    req(fd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,0,V4L2_MEMORY_MMAP,"free");
    req(fd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,1,V4L2_MEMORY_MMAP,"mmap");
    req(fd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,1,V4L2_MEMORY_DMABUF,"dmabuf");
    req(fd,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,1,V4L2_MEMORY_MMAP,"cap-mmap");
    close(fd);
  }
  return 0;
}
