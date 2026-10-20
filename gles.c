/* gles.c - dlopen Adreno EGL + GLESv2 langsung dari chroot (bukan lewat loader Android).
   Tujuan: ciptakan konteks GLES 3.x, laporkan version/renderer, cek jalur compute. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef int EGLint;
typedef unsigned int EGLBoolean;

#define EGL_DEFAULT_DISPLAY ((void*)0)
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES3_BIT 0x00000040
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_NONE 0x3038
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_VENDOR 0x3053
#define EGL_VERSION 0x3054
#define EGL_EXTENSIONS 0x3055
#define EGL_CLIENT_APIS 0x308D

#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_EXTENSIONS 0x1F03

static void* EGLh;
static void* GLh;

int main(void){
  setvbuf(stdout,0,_IONBF,0);

  EGLh = dlopen("/vendor/lib64/egl/libEGL_adreno.so", RTLD_NOW|RTLD_LOCAL);
  if(!EGLh) EGLh = dlopen("libEGL_adreno.so", RTLD_NOW|RTLD_LOCAL);
  printf("dlopen libEGL_adreno -> %p", EGLh); putchar(10);
  if(!EGLh){ puts(dlerror()); return 1; }

  EGLDisplay (*pGetDisplay)(void*);
  EGLBoolean (*pInitialize)(EGLDisplay, EGLint*, EGLint*);
  const char* (*pQueryString)(EGLDisplay, EGLint);
  EGLBoolean (*pBindAPI)(unsigned);
  EGLBoolean (*pChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
  EGLContext (*pCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
  EGLSurface (*pCreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint*);
  EGLBoolean (*pMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);

#define SYM(v,nm) do{ *(void**)(&v)=dlsym(EGLh,nm); if(!(v)){ puts("missing " nm); return 1; } }while(0)
  SYM(pGetDisplay,"eglGetDisplay");
  SYM(pInitialize,"eglInitialize");
  SYM(pQueryString,"eglQueryString");
  SYM(pBindAPI,"eglBindAPI");
  SYM(pChooseConfig,"eglChooseConfig");
  SYM(pCreateContext,"eglCreateContext");
  SYM(pCreatePbufferSurface,"eglCreatePbufferSurface");
  SYM(pMakeCurrent,"eglMakeCurrent");

  EGLDisplay dpy = pGetDisplay(EGL_DEFAULT_DISPLAY);
  printf("eglGetDisplay -> %p", dpy); putchar(10);
  if(!dpy) return 1;

  EGLint maj=0, min=0;
  EGLBoolean ok = pInitialize(dpy, &maj, &min);
  printf("eglInitialize -> %d  ver=%d.%d", ok, maj, min); putchar(10);
  if(!ok) return 1;

  printf("EGL_VENDOR  = %s", pQueryString(dpy, EGL_VENDOR)); putchar(10);
  printf("EGL_VERSION = %s", pQueryString(dpy, EGL_VERSION)); putchar(10);
  printf("EGL_APIS    = %s", pQueryString(dpy, EGL_CLIENT_APIS)); putchar(10);
  const char* ee = pQueryString(dpy, EGL_EXTENSIONS);
  printf("EGL_surfaceless_context = %d", (ee && strstr(ee,"surfaceless_context"))?1:0); putchar(10);

  pBindAPI(EGL_OPENGL_ES_API);

  EGLint cfgattr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLConfig cfg=NULL; EGLint ncfg=0;
  ok = pChooseConfig(dpy, cfgattr, &cfg, 1, &ncfg);
  printf("eglChooseConfig -> %d n=%d cfg=%p", ok, ncfg, cfg); putchar(10);
  if(!ok || ncfg==0) return 1;

  EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  EGLContext ctx = pCreateContext(dpy, cfg, NULL, ctxattr);
  printf("eglCreateContext(ES3) -> %p", ctx); putchar(10);
  if(!ctx) return 1;

  EGLint pbattr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
  EGLSurface pb = pCreatePbufferSurface(dpy, cfg, pbattr);
  printf("eglCreatePbufferSurface -> %p", pb); putchar(10);

  ok = pMakeCurrent(dpy, pb?pb:NULL, pb?pb:NULL, ctx);
  printf("eglMakeCurrent -> %d", ok); putchar(10);
  if(!ok) return 1;

  GLh = dlopen("/vendor/lib64/egl/libGLESv2_adreno.so", RTLD_NOW|RTLD_LOCAL);
  printf("dlopen libGLESv2_adreno -> %p", GLh); putchar(10);
  if(!GLh) return 1;

  const unsigned char* (*pGetString)(unsigned);
  *(void**)(&pGetString)=dlsym(GLh,"glGetString");
  if(!pGetString){ puts("no glGetString"); return 1; }

  const unsigned char* v;
  v=pGetString(GL_VENDOR);   printf("GL_VENDOR   = %s", v?v:(const unsigned char*)"?"); putchar(10);
  v=pGetString(GL_RENDERER); printf("GL_RENDERER = %s", v?v:(const unsigned char*)"?"); putchar(10);
  v=pGetString(GL_VERSION);  printf("GL_VERSION  = %s", v?v:(const unsigned char*)"?"); putchar(10);
  v=pGetString(GL_SHADING_LANGUAGE_VERSION); printf("GL_GLSL     = %s", v?v:(const unsigned char*)"?"); putchar(10);

  return 0;
}
