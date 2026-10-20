#include <stdio.h>
#include <dlfcn.h>
typedef int cl_int; typedef unsigned int cl_uint; typedef void* cl_platform_id;
int main(void){ setvbuf(stdout,0,_IONBF,0);
 void*h=dlopen("libOpenCL.so",RTLD_NOW); printf("dlopen=%p\n",h);
 void*s=dlsym(h,"clGetPlatformIDs"); printf("dlsym clGetPlatformIDs=%p\n",s);
 if(!s) return 1;
 cl_int (*fp)(cl_uint,cl_platform_id*,cl_uint*)=(cl_int(*)(cl_uint,cl_platform_id*,cl_uint*))s;
 printf("calling clGetPlatformIDs(0,NULL,&n)...\n");
 cl_uint n=0; cl_int e=fp(0,NULL,&n);
 printf("returned e=%d n=%u\n",e,n);
 return 0; }
