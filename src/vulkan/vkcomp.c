/* vkcomp.c - minimal Vulkan 1.0 compute (Adreno), run from chroot via linker64.
 * Reads a .spv file, dispatches, verifies output.
 * Build: NDK clang -O2 -o vkcomp vkcomp.c -lvulkan
 * NOTE: use \\n inside C string literals (write_file turns \n into a real newline).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#define CHK(x) do{ VkResult _r=(x); if(_r!=VK_SUCCESS){ printf("VK ERROR %d @line %d\\n",(int)_r,__LINE__); exit(1);} }while(0)

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }

int main(int argc,char**argv){
  setvbuf(stdout,0,_IONBF,0);
  const char*spv=argc>1?argv[1]:"/home/han/adreno616-lab/fma.spv";
  int iters=argc>2?atoi(argv[2]):1000;
  int n=argc>3?atoi(argv[3]):65536;
  int reps=argc>4?atoi(argv[4]):10;

  FILE*f=fopen(spv,"rb");
  if(!f){ printf("cannot open %s\\n",spv); return 1; }
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  unsigned char*code=(unsigned char*)malloc((size_t)sz);
  if(fread(code,1,(size_t)sz,f)!=(size_t)sz){ printf("short read\\n"); return 1; }
  fclose(f);
  printf("SPIR-V %s = %ld bytes\\n",spv,sz);

  VkApplicationInfo ai; memset(&ai,0,sizeof(ai));
  ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.pApplicationName="nb-vk"; ai.apiVersion=VK_API_VERSION_1_0;
  VkInstanceCreateInfo ici; memset(&ici,0,sizeof(ici));
  ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo=&ai;
  VkInstance inst; CHK(vkCreateInstance(&ici,NULL,&inst));

  uint32_t nd=0; CHK(vkEnumeratePhysicalDevices(inst,&nd,NULL));
  printf("Vulkan physical devices: %u\\n",nd);
  if(!nd) return 2;
  if(nd>8) nd=8;
  VkPhysicalDevice pds[8]; CHK(vkEnumeratePhysicalDevices(inst,&nd,pds));
  VkPhysicalDeviceProperties pr; vkGetPhysicalDeviceProperties(pds[0],&pr);
  printf("device: %s\\n",pr.deviceName);
  printf("  api=%u.%u.%u driver=0x%08x vendor=0x%04x\\n",
    VK_VERSION_MAJOR(pr.apiVersion),VK_VERSION_MINOR(pr.apiVersion),VK_VERSION_PATCH(pr.apiVersion),
    pr.driverVersion,pr.vendorID);
  printf("  maxWGInvocations=%u maxSharedMem=%u maxComputeWorkGroupCount=%u,%u,%u\\n",
    pr.limits.maxComputeWorkGroupInvocations,pr.limits.maxComputeSharedMemorySize,
    pr.limits.maxComputeWorkGroupCount[0],pr.limits.maxComputeWorkGroupCount[1],pr.limits.maxComputeWorkGroupCount[2]);

  uint32_t nqf=0; vkGetPhysicalDeviceQueueFamilyProperties(pds[0],&nqf,NULL);
  if(nqf>16) nqf=16;
  VkQueueFamilyProperties qfp[16]; vkGetPhysicalDeviceQueueFamilyProperties(pds[0],&nqf,qfp);
  int qi=-1; for(uint32_t i=0;i<nqf;i++){ if(qfp[i].queueFlags & VK_QUEUE_COMPUTE_BIT){ qi=(int)i; break; } }
  printf("compute queue family=%d (of %u)\\n",qi,nqf);
  if(qi<0) return 3;

  float prio=1.0f;
  VkDeviceQueueCreateInfo qci; memset(&qci,0,sizeof(qci));
  qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex=(uint32_t)qi; qci.queueCount=1; qci.pQueuePriorities=&prio;
  VkDeviceCreateInfo dci; memset(&dci,0,sizeof(dci));
  dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
  VkDevice dev; CHK(vkCreateDevice(pds[0],&dci,NULL,&dev));
  VkQueue q; vkGetDeviceQueue(dev,(uint32_t)qi,0,&q);

  VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pds[0],&mp);

  size_t bytes=(size_t)n*16;
  VkBuffer bufs[3]; VkDeviceMemory mems[3]; void*maps[3];
  for(int i=0;i<3;i++){
    VkBufferCreateInfo bci; memset(&bci,0,sizeof(bci));
    bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size=bytes; bci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    CHK(vkCreateBuffer(dev,&bci,NULL,&bufs[i]));
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev,bufs[i],&mr);
    VkMemoryPropertyFlags need=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t mt=0; int ok=0;
    for(uint32_t k=0;k<mp.memoryTypeCount;k++){
      if((mr.memoryTypeBits&(1u<<k)) && ((mp.memoryTypes[k].propertyFlags&need)==need)){ mt=k; ok=1; break; }
    }
    if(!ok){ printf("no host-visible memory type\\n"); return 4; }
    VkMemoryAllocateInfo mai; memset(&mai,0,sizeof(mai));
    mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
    CHK(vkAllocateMemory(dev,&mai,NULL,&mems[i]));
    CHK(vkBindBufferMemory(dev,bufs[i],mems[i],0));
    CHK(vkMapMemory(dev,mems[i],0,bytes,0,&maps[i]));
    float*p=(float*)maps[i];
    for(int k=0;k<n*4;k++) p[k]=(i==0)?1.0001f:((i==1)?1.0002f:0.0f);
  }
  printf("3 storage buffers, %d float4 each (%.1f MB total)\\n",n,(double)bytes*3.0/1048576.0);

  VkDescriptorSetLayoutBinding binds[3];
  for(int i=0;i<3;i++){ binds[i].binding=(uint32_t)i; binds[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[i].descriptorCount=1; binds[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; binds[i].pImmutableSamplers=NULL; }
  VkDescriptorSetLayoutCreateInfo dslci; memset(&dslci,0,sizeof(dslci));
  dslci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dslci.bindingCount=3; dslci.pBindings=binds;
  VkDescriptorSetLayout dsl; CHK(vkCreateDescriptorSetLayout(dev,&dslci,NULL,&dsl));

  VkPushConstantRange pcr; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.offset=0; pcr.size=4;
  VkPipelineLayoutCreateInfo plci; memset(&plci,0,sizeof(plci));
  plci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plci.setLayoutCount=1; plci.pSetLayouts=&dsl; plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
  VkPipelineLayout pl; CHK(vkCreatePipelineLayout(dev,&plci,NULL,&pl));

  VkShaderModuleCreateInfo smci; memset(&smci,0,sizeof(smci));
  smci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smci.codeSize=(size_t)sz; smci.pCode=(const uint32_t*)code;
  VkShaderModule sm; CHK(vkCreateShaderModule(dev,&smci,NULL,&sm));

  VkPipelineShaderStageCreateInfo ss; memset(&ss,0,sizeof(ss));
  ss.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  ss.stage=VK_SHADER_STAGE_COMPUTE_BIT; ss.module=sm; ss.pName="main";
  VkComputePipelineCreateInfo cpci; memset(&cpci,0,sizeof(cpci));
  cpci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cpci.stage=ss; cpci.layout=pl;
  VkPipeline pipe; CHK(vkCreateComputePipelines(dev,VK_NULL_HANDLE,1,&cpci,NULL,&pipe));
  printf("compute pipeline OK\\n");

  VkDescriptorPoolSize dps; dps.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; dps.descriptorCount=3;
  VkDescriptorPoolCreateInfo dpci; memset(&dpci,0,sizeof(dpci));
  dpci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; dpci.maxSets=1; dpci.poolSizeCount=1; dpci.pPoolSizes=&dps;
  VkDescriptorPool dp; CHK(vkCreateDescriptorPool(dev,&dpci,NULL,&dp));
  VkDescriptorSetAllocateInfo dsai; memset(&dsai,0,sizeof(dsai));
  dsai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsai.descriptorPool=dp; dsai.descriptorSetCount=1; dsai.pSetLayouts=&dsl;
  VkDescriptorSet ds; CHK(vkAllocateDescriptorSets(dev,&dsai,&ds));

  VkDescriptorBufferInfo dbi[3]; VkWriteDescriptorSet wr[3];
  for(int i=0;i<3;i++){
    dbi[i].buffer=bufs[i]; dbi[i].offset=0; dbi[i].range=bytes;
    memset(&wr[i],0,sizeof(wr[i]));
    wr[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; wr[i].dstSet=ds; wr[i].dstBinding=(uint32_t)i;
    wr[i].descriptorCount=1; wr[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr[i].pBufferInfo=&dbi[i];
  }
  vkUpdateDescriptorSets(dev,3,wr,0,NULL);

  VkCommandPoolCreateInfo cpi; memset(&cpi,0,sizeof(cpi));
  cpi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpi.queueFamilyIndex=(uint32_t)qi;
  VkCommandPool cp; CHK(vkCreateCommandPool(dev,&cpi,NULL,&cp));
  VkCommandBufferAllocateInfo cbai; memset(&cbai,0,sizeof(cbai));
  cbai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.commandPool=cp; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1;
  VkCommandBuffer cb; CHK(vkAllocateCommandBuffers(dev,&cbai,&cb));

  VkCommandBufferBeginInfo cbbi; memset(&cbbi,0,sizeof(cbbi));
  cbbi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; cbbi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CHK(vkBeginCommandBuffer(cb,&cbbi));
  vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipe);
  vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&ds,0,NULL);
  vkCmdPushConstants(cb,pl,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&iters);
  uint32_t groups=(uint32_t)(n/64);
  vkCmdDispatch(cb,groups,1,1);
  CHK(vkEndCommandBuffer(cb));

  VkFenceCreateInfo fci; memset(&fci,0,sizeof(fci)); fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence; CHK(vkCreateFence(dev,&fci,NULL,&fence));
  VkSubmitInfo si; memset(&si,0,sizeof(si));
  si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cb;

  CHK(vkQueueSubmit(q,1,&si,fence));
  CHK(vkWaitForFences(dev,1,&fence,VK_TRUE,1000000000ULL));
  CHK(vkResetFences(dev,1,&fence));

  double t0=now_s();
  for(int r=0;r<reps;r++) CHK(vkQueueSubmit(q,1,&si,fence));
  CHK(vkWaitForFences(dev,1,&fence,VK_TRUE,100000000000ULL));
  double t1=now_s();

  double fma=(double)n*4.0*(double)iters*(double)reps;
  double secs=t1-t0;
  float*c=(float*)maps[2];
  double expect=(double)iters*1.0001*1.0002;
  printf("\\n=== Vulkan compute FMA ===\\n");
  printf("n=%d iters=%d reps=%d groups=%u (localsize 64)\\n",n,iters,reps,groups);
  printf("wall=%.4f s  FMA=%.4e  GFLOPS(fma=2flop)=%.2f\\n",secs,fma,fma*2.0/secs/1e9);
  printf("VERIFY C[0]=%g C[4]=%g C[last]=%g (expected ~%g)\\n",c[0],c[4],c[4*(n-1)],expect);
  return 0;
}
