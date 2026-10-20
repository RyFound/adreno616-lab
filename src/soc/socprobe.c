/* socprobe.c - probe DSP (adsprpc), ISP/camera (V4L2), video codec (V4L2 M2M) from chroot. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static void v4l_probe(const char*path){
  int fd=open(path,O_RDWR|O_NONBLOCK);
  if(fd<0){ printf("  %-18s open fail errno=%d\n",path,errno); return; }
  struct v4l2_capability cap; memset(&cap,0,sizeof(cap));
  if(ioctl(fd,VIDIOC_QUERYCAP,&cap)==0){
    printf("  %-18s driver=%.16s card=%.32s caps=0x%08x\n",path,cap.driver,cap.card,cap.capabilities);
    printf("      capture=%d output=%d m2m_mplane=%d streaming=%d\n",
      (cap.capabilities&V4L2_CAP_VIDEO_CAPTURE)?1:0,
      (cap.capabilities&V4L2_CAP_VIDEO_OUTPUT)?1:0,
      (cap.capabilities&V4L2_CAP_VIDEO_M2M_MPLANE)?1:0,
      (cap.capabilities&V4L2_CAP_STREAMING)?1:0);
  } else printf("  %-18s VIDIOC_QUERYCAP fail errno=%d\n",path,errno);
  close(fd);
}

int main(void){
  setvbuf(stdout,0,_IONBF,0);
  puts("=== [1] DSP / Hexagon via adsprpc ===");
  errno=0; int sfd=open("/dev/adsprpc-smd",O_RDWR);
  printf("  open /dev/adsprpc-smd -> fd=%d errno=%d\n",sfd,errno);
  if(sfd>=0) close(sfd);

  void*h=dlopen("libadsprpc.so",RTLD_NOW);
  if(!h) h=dlopen("/vendor/lib64/libadsprpc.so",RTLD_NOW);
  if(!h){ printf("  dlopen libadsprpc.so FAIL: %s\n",dlerror()); }
  else{
    puts("  dlopen libadsprpc.so OK");
    const char*syms[]={"remote_handle64_open","remote_handle64_invoke","remote_handle64_close",
                       "remote_handle_open","remote_handle_invoke","remote_handle_close",
                       "fastrpc_mem_alloc","adsprpc_mem_map","remote_session_control",NULL};
    for(int i=0;syms[i];i++){ void*p=dlsym(h,syms[i]); printf("    sym %-28s = %s\n",syms[i],p?"FOUND":"-"); }
    typedef int (*fn_open)(const char*, long long*);
    fn_open f64=(fn_open)dlsym(h,"remote_handle64_open");
    const char*skels[]={"libfastcvadsp","fastcvadsp","libcamera_nn_skel","libapps_mem_heap",NULL};
    if(f64){
      for(int i=0;skels[i];i++){
        long long hd=0; errno=0; int rc=f64(skels[i],&hd);
        printf("    remote_handle64_open(%-20s) rc=%d handle=%lld errno=%d\n",skels[i],rc,hd,errno);
      }
    } else puts("    no remote_handle64_open");
  }

  puts("");
  puts("=== [2] ISP / camera (V4L2) ===");
  v4l_probe("/dev/video0"); v4l_probe("/dev/video1"); v4l_probe("/dev/video2");
  v4l_probe("/dev/video32"); v4l_probe("/dev/video33"); v4l_probe("/dev/v4l-subdev0");

  puts("");
  puts("=== [3] video codec (media + M2M) ===");
  v4l_probe("/dev/media0"); v4l_probe("/dev/media1");
  puts("done");
  return 0;
}
