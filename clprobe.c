#include <stdio.h>
#include <dlfcn.h>
int main(void){ setvbuf(stdout,0,_IONBF,0); puts("step1 start"); void*h=dlopen("libOpenCL.so",RTLD_NOW); printf("step2 dlopen=%p\n",h); if(!h){ printf("err=%s\n",dlerror()); } puts("step3 done"); return 0; }
