/* vkopt.c - Vulkan kernel-shape sweep for Adreno 616.
 * Axes: local_size (L) x ILP depth (K independent accumulators per work-item).
 * Gate is exact: with x=y=1, z0=0, each accumulator a_i ends at exactly iters,
 * so the stored sum is K*iters -- an integer < 2^24, representable exactly in f32.
 * FLOPs per rep = n * 4(lanes) * K * iters * 2.
 * Build: NDK clang -O2 -o vkopt vkopt.c -ldl   (no -lvulkan)
 * Run:   LD_LIBRARY_PATH=/vendor/lib64:/system/lib64 \
 *          /apex/com.android.runtime/bin/linker64 /abs/vkopt <iters> <n> <reps> <L> <K> <ramp|cold>
 * SPIR-V path from env VKOPT_SPV.
 * No backslash-n inside string literals (write_file mangles it); use puts().
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

static uint32_t* load_spv(const char*path, size_t*out_sz){
    FILE*f=fopen(path,"rb"); if(!f){ printf("SPV open fail: %s", path); puts(""); return NULL; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<=0){ fclose(f); puts("SPV empty"); return NULL; }
    uint32_t*buf=(uint32_t*)malloc((size_t)sz);
    if(fread(buf,1,(size_t)sz,f)!=(size_t)sz){ fclose(f); free(buf); puts("SPV read fail"); return NULL; }
    fclose(f); *out_sz=(size_t)sz; return buf;
}

int main(int argc, char**argv){
    setvbuf(stdout,0,_IONBF,0);
    if(argc<7){ puts("usage: vkopt <iters> <n> <reps> <L> <K> <ramp|cold>"); return 2; }
    uint32_t iters=(uint32_t)strtoul(argv[1],0,0);
    uint32_t n=(uint32_t)strtoul(argv[2],0,0);
    uint32_t reps=(uint32_t)strtoul(argv[3],0,0);
    uint32_t L=(uint32_t)strtoul(argv[4],0,0);
    uint32_t K=(uint32_t)strtoul(argv[5],0,0);
    int is_cold = strcmp(argv[6],"cold")==0;
    if(L==0||n%L!=0){ puts("n must be a multiple of L"); return 2; }
    if(K!=1&&K!=2&&K!=3&&K!=4&&K!=8&&K!=16){ puts("K must be 1,2,3,4,8,16"); return 2; }

    uint32_t ramp_ms=500;
    { const char*e=getenv("VKOPT_RAMP"); if(e) ramp_ms=(uint32_t)strtoul(e,0,0); }

    /* Batch mode: record R dispatches into ONE command buffer, one submit, one
     * fence per rep. Requires an ACCUMULATING shader (e.g. vkopt_k2_batch.spv);
     * with the default overwrite kernels the gate would only prove the last
     * dispatch ran. Gate becomes K*iters*R. */
    uint32_t R=1;
    { const char*e=getenv("VKOPT_R"); if(e) R=(uint32_t)strtoul(e,0,0); }
    if(R<1) R=1;
    int is_batch = (R>1);

    const char*LIB="/vendor/lib64/hw/vulkan.adreno.so";
    void*lib=dlopen(LIB, RTLD_NOW|RTLD_LOCAL);
    if(!lib){ printf("dlopen fail: %s", dlerror()); puts(""); return 1; }

    typedef PFN_vkVoidFunction (*GIPA_t)(VkInstance, const char*);
    GIPA_t gipa=(GIPA_t)dlsym(lib,"_ZN11qglinternal21vkGetInstanceProcAddrEP12VkInstance_TPKc");
    if(!gipa){ puts("dlsym mangled vkGetInstanceProcAddr fail"); return 1; }

    PFN_vkCreateInstance pCreateInstance=(PFN_vkCreateInstance)gipa(NULL,"vkCreateInstance");
    if(!pCreateInstance){ puts("no vkCreateInstance"); return 1; }

    VkApplicationInfo ai; memset(&ai,0,sizeof(ai)); ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName="vkopt"; ai.applicationVersion=1; ai.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici; memset(&ici,0,sizeof(ici)); ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo=&ai;
    VkInstance inst=VK_NULL_HANDLE;
    if(pCreateInstance(&ici,NULL,&inst)!=VK_SUCCESS){ puts("vkCreateInstance fail"); return 1; }

    PFN_vkEnumeratePhysicalDevices pEnum=(PFN_vkEnumeratePhysicalDevices)gipa(inst,"vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceQueueFamilyProperties pQFP=(PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(inst,"vkGetPhysicalDeviceQueueFamilyProperties");
    PFN_vkGetPhysicalDeviceMemoryProperties pMemProps=(PFN_vkGetPhysicalDeviceMemoryProperties)gipa(inst,"vkGetPhysicalDeviceMemoryProperties");
    PFN_vkCreateDevice pCreateDevice=(PFN_vkCreateDevice)gipa(inst,"vkCreateDevice");
    PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(inst,"vkGetDeviceProcAddr");
    if(!pEnum||!pQFP||!gdpa||!pMemProps){ puts("GIPA missing core fns"); return 1; }

    uint32_t nd=0; pEnum(inst,&nd,NULL);
    if(nd<1){ puts("0 physical devices"); return 1; }
    VkPhysicalDevice phys=VK_NULL_HANDLE; { uint32_t one=1; pEnum(inst,&one,&phys); }

    uint32_t nq=0; pQFP(phys,&nq,NULL);
    VkQueueFamilyProperties*qf=(VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*nq); pQFP(phys,&nq,qf);
    uint32_t qi=0; int found=0;
    for(uint32_t i=0;i<nq;i++){ if(qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT){ qi=i; found=1; break; } }
    if(!found){ puts("no compute queue"); return 1; }
    free(qf);

    float prio=1.0f;
    VkDeviceQueueCreateInfo qci; memset(&qci,0,sizeof(qci)); qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qci.queueFamilyIndex=qi; qci.queueCount=1; qci.pQueuePriorities=&prio;
    VkDeviceCreateInfo dci; memset(&dci,0,sizeof(dci)); dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
    VkDevice dev=VK_NULL_HANDLE;
    if(pCreateDevice(phys,&dci,NULL,&dev)!=VK_SUCCESS){ puts("vkCreateDevice fail"); return 1; }

    PFN_vkGetDeviceQueue pGetQueue=(PFN_vkGetDeviceQueue)gdpa(dev,"vkGetDeviceQueue");
    VkQueue queue=VK_NULL_HANDLE; pGetQueue(dev,qi,0,&queue);

    PFN_vkCreateBuffer pCreateBuffer=(PFN_vkCreateBuffer)gdpa(dev,"vkCreateBuffer");
    PFN_vkGetBufferMemoryRequirements pBufReq=(PFN_vkGetBufferMemoryRequirements)gdpa(dev,"vkGetBufferMemoryRequirements");
    PFN_vkAllocateMemory pAlloc=(PFN_vkAllocateMemory)gdpa(dev,"vkAllocateMemory");
    PFN_vkBindBufferMemory pBind=(PFN_vkBindBufferMemory)gdpa(dev,"vkBindBufferMemory");
    PFN_vkMapMemory pMap=(PFN_vkMapMemory)gdpa(dev,"vkMapMemory");
    PFN_vkCreateShaderModule pShader=(PFN_vkCreateShaderModule)gdpa(dev,"vkCreateShaderModule");
    PFN_vkCreateDescriptorSetLayout pDSL=(PFN_vkCreateDescriptorSetLayout)gdpa(dev,"vkCreateDescriptorSetLayout");
    PFN_vkCreatePipelineLayout pPL=(PFN_vkCreatePipelineLayout)gdpa(dev,"vkCreatePipelineLayout");
    PFN_vkCreateComputePipelines pCP=(PFN_vkCreateComputePipelines)gdpa(dev,"vkCreateComputePipelines");
    PFN_vkCreateDescriptorPool pDP=(PFN_vkCreateDescriptorPool)gdpa(dev,"vkCreateDescriptorPool");
    PFN_vkAllocateDescriptorSets pADS=(PFN_vkAllocateDescriptorSets)gdpa(dev,"vkAllocateDescriptorSets");
    PFN_vkUpdateDescriptorSets pUDS=(PFN_vkUpdateDescriptorSets)gdpa(dev,"vkUpdateDescriptorSets");
    PFN_vkCreateCommandPool pCmdPool=(PFN_vkCreateCommandPool)gdpa(dev,"vkCreateCommandPool");
    PFN_vkAllocateCommandBuffers pAllocCB=(PFN_vkAllocateCommandBuffers)gdpa(dev,"vkAllocateCommandBuffers");
    PFN_vkBeginCommandBuffer pBegin=(PFN_vkBeginCommandBuffer)gdpa(dev,"vkBeginCommandBuffer");
    PFN_vkEndCommandBuffer pEnd=(PFN_vkEndCommandBuffer)gdpa(dev,"vkEndCommandBuffer");
    PFN_vkCmdBindPipeline pBindPipe=(PFN_vkCmdBindPipeline)gdpa(dev,"vkCmdBindPipeline");
    PFN_vkCmdBindDescriptorSets pBindDS=(PFN_vkCmdBindDescriptorSets)gdpa(dev,"vkCmdBindDescriptorSets");
    PFN_vkCmdPushConstants pPush=(PFN_vkCmdPushConstants)gdpa(dev,"vkCmdPushConstants");
    PFN_vkCmdDispatch pDispatch=(PFN_vkCmdDispatch)gdpa(dev,"vkCmdDispatch");
    PFN_vkQueueSubmit pSubmit=(PFN_vkQueueSubmit)gdpa(dev,"vkQueueSubmit");
    PFN_vkCreateFence pFence=(PFN_vkCreateFence)gdpa(dev,"vkCreateFence");
    PFN_vkWaitForFences pWait=(PFN_vkWaitForFences)gdpa(dev,"vkWaitForFences");
    PFN_vkResetFences pReset=(PFN_vkResetFences)gdpa(dev,"vkResetFences");
    PFN_vkDeviceWaitIdle pIdle=(PFN_vkDeviceWaitIdle)gdpa(dev,"vkDeviceWaitIdle");
    if(!pCreateBuffer||!pShader||!pCP||!pSubmit||!pWait||!pDispatch){ puts("GDPA missing fns"); return 1; }

    VkPhysicalDeviceMemoryProperties mp; memset(&mp,0,sizeof(mp)); pMemProps(phys,&mp);
    uint32_t mti=0xFFFFFFFFu;
    for(uint32_t i=0;i<mp.memoryTypeCount;i++){
        if((mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){ mti=i; break; }
    }
    if(mti==0xFFFFFFFFu){ puts("no HOST_VISIBLE|COHERENT memory type"); return 1; }

    VkDeviceSize bsz=(VkDeviceSize)n*16;
    VkBuffer buf[3]; VkDeviceMemory mem[3]; void*ptr[3];
    for(int i=0;i<3;i++){
        VkBufferCreateInfo bci; memset(&bci,0,sizeof(bci)); bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size=bsz; bci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(pCreateBuffer(dev,&bci,NULL,&buf[i])!=VK_SUCCESS){ puts("buf create fail"); return 1; }
        VkMemoryRequirements mr; pBufReq(dev,buf[i],&mr);
        VkMemoryAllocateInfo mai; memset(&mai,0,sizeof(mai)); mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize=mr.size; mai.memoryTypeIndex=mti;
        if(pAlloc(dev,&mai,NULL,&mem[i])!=VK_SUCCESS){ puts("mem alloc fail"); return 1; }
        if(pBind(dev,buf[i],mem[i],0)!=VK_SUCCESS){ puts("bind fail"); return 1; }
        if(pMap(dev,mem[i],0,VK_WHOLE_SIZE,0,&ptr[i])!=VK_SUCCESS){ puts("map fail"); return 1; }
        float*f=(float*)ptr[i];
        for(VkDeviceSize k=0;k<(VkDeviceSize)n*4;k++){ f[k]=(i==2)?0.0f:1.0f; }
    }

    VkDescriptorSetLayoutBinding b[3]; memset(b,0,sizeof(b));
    for(int i=0;i<3;i++){ b[i].binding=i; b[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; b[i].descriptorCount=1; b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; }
    VkDescriptorSetLayoutCreateInfo dlci; memset(&dlci,0,sizeof(dlci)); dlci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dlci.bindingCount=3; dlci.pBindings=b;
    VkDescriptorSetLayout dsl; pDSL(dev,&dlci,NULL,&dsl);

    VkPushConstantRange pcr; memset(&pcr,0,sizeof(pcr)); pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.offset=0; pcr.size=4;
    VkPipelineLayoutCreateInfo plci; memset(&plci,0,sizeof(plci)); plci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; plci.setLayoutCount=1; plci.pSetLayouts=&dsl; plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
    VkPipelineLayout pl=VK_NULL_HANDLE; if(pPL(dev,&plci,NULL,&pl)!=VK_SUCCESS){ puts("pipeline layout fail"); return 1; }

    char spvpath[1024];
    { const char*e=getenv("VKOPT_SPV"); if(e) snprintf(spvpath,sizeof(spvpath),"%s",e);
      else { snprintf(spvpath,sizeof(spvpath),"%s",argv[0]); char*sl=strrchr(spvpath,'/'); if(sl) sl[1]=0; else spvpath[0]=0; strncat(spvpath,"vkopt.spv",sizeof(spvpath)-strlen(spvpath)-1); } }
    size_t spvsz=0; uint32_t*spv=load_spv(spvpath,&spvsz);
    if(!spv){ printf("need SPV at %s", spvpath); puts(""); return 1; }
    VkShaderModuleCreateInfo smci; memset(&smci,0,sizeof(smci)); smci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; smci.codeSize=spvsz; smci.pCode=spv;
    VkShaderModule sm=VK_NULL_HANDLE; if(pShader(dev,&smci,NULL,&sm)!=VK_SUCCESS){ puts("shader module fail"); return 1; }

    VkComputePipelineCreateInfo cpci; memset(&cpci,0,sizeof(cpci)); cpci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; cpci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; cpci.stage.module=sm; cpci.stage.pName="main"; cpci.layout=pl;
    VkPipeline pipe=VK_NULL_HANDLE; if(pCP(dev,VK_NULL_HANDLE,1,&cpci,NULL,&pipe)!=VK_SUCCESS){ puts("pipeline fail"); return 1; }

    VkDescriptorPoolSize dps; memset(&dps,0,sizeof(dps)); dps.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; dps.descriptorCount=3;
    VkDescriptorPoolCreateInfo dpci; memset(&dpci,0,sizeof(dpci)); dpci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; dpci.maxSets=1; dpci.poolSizeCount=1; dpci.pPoolSizes=&dps;
    VkDescriptorPool dp; pDP(dev,&dpci,NULL,&dp);
    VkDescriptorSetAllocateInfo dsai; memset(&dsai,0,sizeof(dsai)); dsai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsai.descriptorPool=dp; dsai.descriptorSetCount=1; dsai.pSetLayouts=&dsl;
    VkDescriptorSet ds; pADS(dev,&dsai,&ds);
    VkDescriptorBufferInfo dbi[3];
    for(int i=0;i<3;i++){ dbi[i].buffer=buf[i]; dbi[i].offset=0; dbi[i].range=VK_WHOLE_SIZE; }
    VkWriteDescriptorSet w[3]; memset(w,0,sizeof(w));
    for(int i=0;i<3;i++){ w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet=ds; w[i].dstBinding=i; w[i].descriptorCount=1; w[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[i].pBufferInfo=&dbi[i]; }
    pUDS(dev,3,w,0,NULL);

    VkCommandPoolCreateInfo cpi; memset(&cpi,0,sizeof(cpi)); cpi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpi.queueFamilyIndex=qi;
    VkCommandPool cp; pCmdPool(dev,&cpi,NULL,&cp);
    VkCommandBufferAllocateInfo cbai; memset(&cbai,0,sizeof(cbai)); cbai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbai.commandPool=cp; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1;
    VkCommandBuffer cb; pAllocCB(dev,&cbai,&cb);
    VkFenceCreateInfo fci; memset(&fci,0,sizeof(fci)); fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence; pFence(dev,&fci,NULL,&fence);

    /* VKOPT_DIV: divide the grid for kernels where one work-item owns DIV
     * elements (grid-stride). Buffers still cover all n elements; only the grid
     * shrinks. FLOPs accounting stays correct because such kernels are run with
     * K=1 -- the shader itself covers every element. */
    uint32_t DIV=1;
    { const char*e=getenv("VKOPT_DIV"); if(e) DIV=(uint32_t)strtoul(e,0,0); }
    if(DIV<1) DIV=1;
    if(n%(L*DIV)!=0){ puts("n must be a multiple of L*DIV"); return 2; }

    uint32_t gx=n/L/DIV;
    VkCommandBufferBeginInfo bi; memset(&bi,0,sizeof(bi)); bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    pBegin(cb,&bi);
    pBindPipe(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipe);
    pBindDS(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&ds,0,NULL);
    pPush(cb,pl,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&iters);
    for(uint32_t r=0;r<R;r++){ pDispatch(cb,gx,1,1); }
    pEnd(cb);
    if(is_batch){ printf("DISPATCHES_RECORDED %u", R); puts(""); }

    VkSubmitInfo si; memset(&si,0,sizeof(si)); si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cb;

    /* gate run */
    pReset(dev,1,&fence);
    VkResult gr1=pSubmit(queue,1,&si,fence);
    VkResult gr2=pWait(dev,1,&fence,VK_TRUE,~0ull);
    pIdle(dev);
    printf("GATERES submit=%d wait=%d", (int)gr1, (int)gr2); puts("");
    float g0=((float*)ptr[2])[0];
    float expect=(float)iters*(float)K*(float)R;
    printf("GATE C0=%g expect=%g K=%u L=%u R=%u %s", (double)g0, (double)expect, K, L, R, (g0==expect)?"PASS":"FAIL"); puts("");
    if(g0!=expect){ puts("GATE_FAIL_EXIT"); return 1; }

    if(!is_cold){
        double wt0=now_s();
        while(now_s()-wt0 < ramp_ms/1000.0){
            pReset(dev,1,&fence); pSubmit(queue,1,&si,fence); pWait(dev,1,&fence,VK_TRUE,~0ull);
        }
    }

    double flops_per_rep=(double)n*4.0*(double)K*(double)iters*2.0*(double)R;
    double t0=now_s();
    for(uint32_t rep=0; rep<reps; rep++){
        double r0=now_s();
        pReset(dev,1,&fence);
        VkResult sr=pSubmit(queue,1,&si,fence);
        VkResult wr=pWait(dev,1,&fence,VK_TRUE,~0ull);
        double r1=now_s();
        printf("REP i=%u submit=%d wait=%d wall_s=%.6f", rep, (int)sr, (int)wr, r1-r0); puts("");
    }
    double t1=now_s();
    int f_after=read_freq();
    double wall=t1-t0;
    double gf=flops_per_rep*(double)reps/wall/1e9;
    printf("RESULT mode=%s L=%u K=%u R=%u n=%u iters=%u reps=%u gx=%u ramp=%u wall_s=%.6f gflops_wall=%.3f cur_freq_after=%d",
           is_cold?"cold":"ramp", L, K, R, n, iters, reps, gx, is_cold?0u:ramp_ms, wall, gf, f_after);
    puts("");
    return 0;
}
