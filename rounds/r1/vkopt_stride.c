/* vkopt_stride.c - minimal harness for the COALESCED grid-stride D=2 kernel.
 * One work-item handles elements g and g+stride. The second push-constant
 * (offset 4) carries `stride` = total invocation count = n/2.
 * Gate: element 0 runs iters times from 0 -> C0 must equal iters exactly.
 * Build: NDK clang -O2 -o vkopt_stride vkopt_stride.c -ldl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }
static int read_freq(void){ FILE*f=fopen("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq","r"); if(!f) return -1; long v=0; if(fscanf(f,"%ld",&v)!=1) v=-1; fclose(f); return (int)v; }

int main(int argc,char**argv){
    setvbuf(stdout,0,_IONBF,0);
    uint32_t iters=2000, n=65536, reps=10, L=64;
    const char*spvpath=getenv("VKOPT_SPV");
    if(!spvpath){ puts("set VKOPT_SPV"); return 2; }

    const char*LIB="/vendor/lib64/hw/vulkan.adreno.so";
    void*lib=dlopen(LIB, RTLD_NOW|RTLD_LOCAL);
    if(!lib){ printf("dlopen fail: %s", dlerror()); puts(""); return 1; }
    typedef PFN_vkVoidFunction (*GIPA_t)(VkInstance, const char*);
    GIPA_t gipa=(GIPA_t)dlsym(lib,"_ZN11qglinternal21vkGetInstanceProcAddrEP12VkInstance_TPKc");
    if(!gipa){ puts("no GIPA"); return 1; }
    PFN_vkCreateInstance pCreateInstance=(PFN_vkCreateInstance)gipa(NULL,"vkCreateInstance");
    VkApplicationInfo ai; memset(&ai,0,sizeof(ai));
    ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.pApplicationName="vkopt_stride"; ai.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici; memset(&ici,0,sizeof(ici)); ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo=&ai;
    VkInstance inst; if(pCreateInstance(&ici,NULL,&inst)!=VK_SUCCESS){ puts("no instance"); return 1; }

    PFN_vkEnumeratePhysicalDevices pEnum=(PFN_vkEnumeratePhysicalDevices)gipa(inst,"vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties pQFP=(PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(inst,"vkGetPhysicalDeviceQueueFamilyProperties");
    PFN_vkGetPhysicalDeviceMemoryProperties pMP=(PFN_vkGetPhysicalDeviceMemoryProperties)gipa(inst,"vkGetPhysicalDeviceMemoryProperties");
    PFN_vkCreateDevice pCreateDevice=(PFN_vkCreateDevice)gipa(inst,"vkCreateDevice");
    PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(inst,"vkGetDeviceProcAddr");

    uint32_t nd=0; pEnum(inst,&nd,NULL); if(!nd){ puts("0 devices"); return 1; }
    VkPhysicalDevice pd; pEnum(inst,&nd,&pd);
    uint32_t nqf=0; pQFP(pd,&nqf,NULL); if(nqf>16) nqf=16;
    VkQueueFamilyProperties qf[16]; pQFP(pd,&nqf,qf);
    int qi=-1; for(uint32_t i=0;i<nqf;i++){ if(qf[i].queueFlags&VK_QUEUE_COMPUTE_BIT){ qi=(int)i; break; } }
    if(qi<0){ puts("no compute queue"); return 1; }
    float prio=1.0f;
    VkDeviceQueueCreateInfo qci; memset(&qci,0,sizeof(qci));
    qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qci.queueFamilyIndex=(uint32_t)qi; qci.queueCount=1; qci.pQueuePriorities=&prio;
    VkDeviceCreateInfo dci; memset(&dci,0,sizeof(dci));
    dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
    VkDevice dev; if(pCreateDevice(pd,&dci,NULL,&dev)!=VK_SUCCESS){ puts("no device"); return 1; }

#define GFN(t,f) t f=(t)gdpa(dev,#f); if(!f){ printf("missing %s",#f); puts(""); return 1; }
    GFN(PFN_vkGetDeviceQueue, vkGetDeviceQueue)
    GFN(PFN_vkCreateBuffer, vkCreateBuffer)
    GFN(PFN_vkGetBufferMemoryRequirements, vkGetBufferMemoryRequirements)
    GFN(PFN_vkAllocateMemory, vkAllocateMemory)
    GFN(PFN_vkBindBufferMemory, vkBindBufferMemory)
    GFN(PFN_vkMapMemory, vkMapMemory)
    GFN(PFN_vkCreateShaderModule, vkCreateShaderModule)
    GFN(PFN_vkCreateDescriptorSetLayout, vkCreateDescriptorSetLayout)
    GFN(PFN_vkCreatePipelineLayout, vkCreatePipelineLayout)
    GFN(PFN_vkCreateComputePipelines, vkCreateComputePipelines)
    GFN(PFN_vkCreateDescriptorPool, vkCreateDescriptorPool)
    GFN(PFN_vkAllocateDescriptorSets, vkAllocateDescriptorSets)
    GFN(PFN_vkUpdateDescriptorSets, vkUpdateDescriptorSets)
    GFN(PFN_vkCreateCommandPool, vkCreateCommandPool)
    GFN(PFN_vkAllocateCommandBuffers, vkAllocateCommandBuffers)
    GFN(PFN_vkBeginCommandBuffer, vkBeginCommandBuffer)
    GFN(PFN_vkCmdBindPipeline, vkCmdBindPipeline)
    GFN(PFN_vkCmdBindDescriptorSets, vkCmdBindDescriptorSets)
    GFN(PFN_vkCmdPushConstants, vkCmdPushConstants)
    GFN(PFN_vkCmdDispatch, vkCmdDispatch)
    GFN(PFN_vkEndCommandBuffer, vkEndCommandBuffer)
    GFN(PFN_vkCreateFence, vkCreateFence)
    GFN(PFN_vkQueueSubmit, vkQueueSubmit)
    GFN(PFN_vkWaitForFences, vkWaitForFences)
    GFN(PFN_vkResetFences, vkResetFences)
    GFN(PFN_vkDeviceWaitIdle, vkDeviceWaitIdle)

    FILE*f=fopen(spvpath,"rb"); if(!f){ printf("no spv %s",spvpath); puts(""); return 1; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    uint32_t*code=malloc((size_t)sz);
    if(fread(code,1,(size_t)sz,f)!=(size_t)sz){ puts("short read"); return 1; } fclose(f);

    VkQueue q; vkGetDeviceQueue(dev,(uint32_t)qi,0,&q);
    VkPhysicalDeviceMemoryProperties mp; memset(&mp,0,sizeof(mp)); pMP(pd,&mp);
    uint32_t mti=0xFFFFFFFFu;
    for(uint32_t i=0;i<mp.memoryTypeCount;i++){
        if((mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){ mti=i; break; }
    }
    if(mti==0xFFFFFFFFu){ puts("no host-visible mem"); return 1; }

    VkDeviceSize bsz=(VkDeviceSize)n*16;
    VkBuffer buf[3]; VkDeviceMemory mem[3]; void*ptr[3];
    for(int i=0;i<3;i++){
        VkBufferCreateInfo bci; memset(&bci,0,sizeof(bci)); bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size=bsz; bci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateBuffer(dev,&bci,NULL,&buf[i])!=VK_SUCCESS){ puts("buf fail"); return 1; }
        VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev,buf[i],&mr);
        VkMemoryAllocateInfo mai; memset(&mai,0,sizeof(mai)); mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize=mr.size; mai.memoryTypeIndex=mti;
        if(vkAllocateMemory(dev,&mai,NULL,&mem[i])!=VK_SUCCESS){ puts("mem fail"); return 1; }
        if(vkBindBufferMemory(dev,buf[i],mem[i],0)!=VK_SUCCESS){ puts("bind fail"); return 1; }
        if(vkMapMemory(dev,mem[i],0,VK_WHOLE_SIZE,0,&ptr[i])!=VK_SUCCESS){ puts("map fail"); return 1; }
        float*p=(float*)ptr[i];
        for(VkDeviceSize k=0;k<(VkDeviceSize)n*4;k++) p[k]=(i==2)?0.0f:1.0f;
    }

    VkDescriptorSetLayoutBinding b[3]; memset(b,0,sizeof(b));
    for(int i=0;i<3;i++){ b[i].binding=i; b[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; b[i].descriptorCount=1; b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; }
    VkDescriptorSetLayoutCreateInfo dlci; memset(&dlci,0,sizeof(dlci)); dlci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dlci.bindingCount=3; dlci.pBindings=b;
    VkDescriptorSetLayout dsl; vkCreateDescriptorSetLayout(dev,&dlci,NULL,&dsl);

    VkPushConstantRange pcr; memset(&pcr,0,sizeof(pcr)); pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.offset=0; pcr.size=8;
    VkPipelineLayoutCreateInfo plci; memset(&plci,0,sizeof(plci)); plci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; plci.setLayoutCount=1; plci.pSetLayouts=&dsl; plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
    VkPipelineLayout pl; if(vkCreatePipelineLayout(dev,&plci,NULL,&pl)!=VK_SUCCESS){ puts("pl fail"); return 1; }

    VkShaderModuleCreateInfo smci; memset(&smci,0,sizeof(smci)); smci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smci.codeSize=(size_t)sz; smci.pCode=code;
    VkShaderModule sm; if(vkCreateShaderModule(dev,&smci,NULL,&sm)!=VK_SUCCESS){ puts("sm fail"); return 1; }
    VkPipelineShaderStageCreateInfo ss; memset(&ss,0,sizeof(ss));
    ss.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ss.stage=VK_SHADER_STAGE_COMPUTE_BIT; ss.module=sm; ss.pName="main";
    VkComputePipelineCreateInfo cpci; memset(&cpci,0,sizeof(cpci)); cpci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cpci.stage=ss; cpci.layout=pl;
    VkPipeline pipe; if(vkCreateComputePipelines(dev,VK_NULL_HANDLE,1,&cpci,NULL,&pipe)!=VK_SUCCESS){ puts("pipe fail"); return 1; }

    VkDescriptorPoolSize dps; memset(&dps,0,sizeof(dps)); dps.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; dps.descriptorCount=3;
    VkDescriptorPoolCreateInfo dpci; memset(&dpci,0,sizeof(dpci)); dpci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; dpci.maxSets=1; dpci.poolSizeCount=1; dpci.pPoolSizes=&dps;
    VkDescriptorPool dp; vkCreateDescriptorPool(dev,&dpci,NULL,&dp);
    VkDescriptorSetAllocateInfo dsai; memset(&dsai,0,sizeof(dsai)); dsai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsai.descriptorPool=dp; dsai.descriptorSetCount=1; dsai.pSetLayouts=&dsl;
    VkDescriptorSet ds; vkAllocateDescriptorSets(dev,&dsai,&ds);
    VkDescriptorBufferInfo dbi[3]; VkWriteDescriptorSet wr[3];
    for(int i=0;i<3;i++){ dbi[i].buffer=buf[i]; dbi[i].offset=0; dbi[i].range=VK_WHOLE_SIZE;
        memset(&wr[i],0,sizeof(wr[i])); wr[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; wr[i].dstSet=ds; wr[i].dstBinding=(uint32_t)i; wr[i].descriptorCount=1; wr[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr[i].pBufferInfo=&dbi[i]; }
    vkUpdateDescriptorSets(dev,3,wr,0,NULL);

    VkCommandPoolCreateInfo cpi; memset(&cpi,0,sizeof(cpi)); cpi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpi.queueFamilyIndex=(uint32_t)qi;
    VkCommandPool cp; vkCreateCommandPool(dev,&cpi,NULL,&cp);
    VkCommandBufferAllocateInfo cbai; memset(&cbai,0,sizeof(cbai)); cbai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbai.commandPool=cp; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1;
    VkCommandBuffer cb; vkAllocateCommandBuffers(dev,&cbai,&cb);
    VkFenceCreateInfo fci; memset(&fci,0,sizeof(fci)); fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence; vkCreateFence(dev,&fci,NULL,&fence);

    uint32_t gx=n/L/2;
    uint32_t stride=gx*L;
    uint32_t pc[2]={iters,stride};
    VkCommandBufferBeginInfo bi; memset(&bi,0,sizeof(bi)); bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cb,&bi);
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipe);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&ds,0,NULL);
    vkCmdPushConstants(cb,pl,VK_SHADER_STAGE_COMPUTE_BIT,0,8,pc);
    vkCmdDispatch(cb,gx,1,1);
    vkEndCommandBuffer(cb);
    VkSubmitInfo si; memset(&si,0,sizeof(si)); si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cb;

    vkResetFences(dev,1,&fence); vkQueueSubmit(q,1,&si,fence); vkWaitForFences(dev,1,&fence,VK_TRUE,~0ull); vkDeviceWaitIdle(dev);
    float g0=((float*)ptr[2])[0];
    printf("GATE C0=%g expect=%g stride=%u D=2 %s", (double)g0, (double)iters, stride, (g0==(float)iters)?"PASS":"FAIL"); puts("");
    if(g0!=(float)iters){ puts("GATE_FAIL_EXIT"); return 1; }

    /* clock ramp */
    double wt0=now_s();
    while(now_s()-wt0 < 0.5){ vkResetFences(dev,1,&fence); vkQueueSubmit(q,1,&si,fence); vkWaitForFences(dev,1,&fence,VK_TRUE,~0ull); }

    double flops=(double)n*4.0*(double)iters*2.0;
    double t0=now_s();
    for(uint32_t r=0;r<reps;r++){ vkResetFences(dev,1,&fence); vkQueueSubmit(q,1,&si,fence); vkWaitForFences(dev,1,&fence,VK_TRUE,~0ull); }
    double t1=now_s();
    int fa=read_freq();
    printf("RESULT L=%u D=2 n=%u iters=%u reps=%u gx=%u wall_s=%.6f gflops_wall=%.3f cur_freq_after=%d",
           L,n,iters,reps,gx,t1-t0,flops*(double)reps/(t1-t0)/1e9,fa); puts("");
    return 0;
}
