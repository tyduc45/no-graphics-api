#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <xf86drm.h> 
#include <amdgpu.h>
#include <drm/amdgpu_drm.h>

struct buffer_view {
    amdgpu_bo_handle bo;
    amdgpu_va_handle va_handle;
    void            *cpu;
    uint64_t         gpu_va;
};
static struct buffer_view make_buffer_view(amdgpu_device_handle dev, uint64_t size)
{
    struct buffer_view b = {0};
    struct amdgpu_bo_alloc_request req = {0};
    req.alloc_size = size;
    req.phys_alignment = 4096;
    req.preferred_heap = AMDGPU_GEM_DOMAIN_GTT;
    req.flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
    amdgpu_bo_alloc(dev, &req, &b.bo);
    amdgpu_bo_cpu_map(b.bo, &b.cpu);
    amdgpu_va_range_alloc(dev, amdgpu_gpu_va_range_general, size, 4096, 0, &b.gpu_va, &b.va_handle, 0);
    amdgpu_bo_va_op(b.bo , 0, size, b.gpu_va , 0 , AMDGPU_VA_OP_MAP);
    return b;
}


int main(void)
{
    int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if(fd == -1 ) {
        printf("error file open\n");
        return -1;
    }
    uint32_t maj, min;
    amdgpu_device_handle dev;
    amdgpu_device_initialize(fd,&maj , &min , &dev);

    struct amdgpu_gpu_info gpu = {0};
    amdgpu_query_gpu_info(dev, &gpu);
    printf("[1] device: %s , DRM: %u.%u , family_id:%u\n",
        amdgpu_get_marketing_name(dev), maj, min , gpu.family_id);

    // alloc bo on gpu to store value from cpu
    struct amdgpu_bo_alloc_request req = {0};
    req.alloc_size      = 4096;
    req.phys_alignment  = 4096;
    req.preferred_heap  = AMDGPU_GEM_DOMAIN_GTT; 
    req.flags           = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
    // alloc bo on device: dev , with request: req, result in handle bo 
    amdgpu_bo_handle bo;
    amdgpu_bo_alloc(dev, &req, &bo);
    //allow cpu to read/write this object buffer_viewfer
    void *cpu;
    amdgpu_bo_cpu_map(bo, &cpu);

    // prepare mock data
    for(int i = 0 ; i < 4096 ;i++) ((uint8_t*)cpu)[i] = (uint8_t)(i*7 + 3);
    printf("[2] read previous 4 bytes:  %02x %02x %02x %02x\n",
    ((uint8_t*)cpu)[0] , ((uint8_t*)cpu)[1] , ((uint8_t*)cpu)[2] , ((uint8_t*)cpu)[3]);
    struct buffer_view src = make_buffer_view(dev , 4096);
    struct buffer_view dst = make_buffer_view(dev , 4096);
    struct buffer_view ib  = make_buffer_view(dev , 4096);

    for(int i = 0 ; i < 4096;i++) ((uint8_t*)src.cpu)[i] = (uint8_t)(i*7 + 3);
    memset(dst.cpu , 0, 4096);
    printf("[4.1] src=0x%lx dst=0x%lx ib=0x%lx | src[0..3]=%02x %02x %02x %02x | dst[0...3]=%02x %02x %02x %02x\n",
       src.gpu_va, dst.gpu_va, ib.gpu_va,
       ((uint8_t*)src.cpu)[0],((uint8_t*)src.cpu)[1],
       ((uint8_t*)src.cpu)[2],((uint8_t*)src.cpu)[3],
       ((uint8_t*)dst.cpu)[0],((uint8_t*)dst.cpu)[1],
       ((uint8_t*)dst.cpu)[2],((uint8_t*)dst.cpu)[3]
    );
    int gfx9lpus = (gpu.family_id >= AMDGPU_FAMILY_AI);
    // command to gpu is send in packet, packet is allined in 32-bit 
     // command pack construction
    uint32_t *pm4 = (uint32_t*)ib.cpu;
    int n = 0;   
    pm4[n++] = 0x00000001;   // command : linear-copy 
    pm4[n++] = gfx9lpus ? 4096 - 1 : 4096;
    pm4[n++] = 0;
    pm4[n++] = (uint32_t)src.gpu_va;            // split a 64 bit number into 2 uint32_t
    pm4[n++] = (uint32_t)(src.gpu_va >> 32);
    pm4[n++] = (uint32_t)dst.gpu_va;             /* dst lo */
    pm4[n++] = (uint32_t)(dst.gpu_va >> 32);     /* dst hi */
    printf("[4.2] packet(%d dw):",n);
    for(int i = 0 ; i < n ;i++) printf(" %08x", pm4[i]);
    printf("\n");

    amdgpu_context_handle ctx;
    int r_ctx = amdgpu_cs_ctx_create(dev, &ctx);

    amdgpu_bo_handle bos[] = { ib.bo, src.bo, dst.bo };
    amdgpu_bo_list_handle list;
    int r_list = amdgpu_bo_list_create(dev, 3, bos, NULL, &list);

    printf("[4.3] ctx_create=%d  bo_list_create=%d\n", r_ctx, r_list);

    // tell gpu where the command is   
    struct amdgpu_cs_ib_info ib_info = {0};
    ib_info.ib_mc_address = ib.gpu_va;  //read command from this address
    ib_info.size = n;                   // command length 

    struct amdgpu_cs_request rq = {0}; // send request to smda to execute this command.
    rq.ip_type          = AMDGPU_HW_IP_DMA;
    rq.ring             = 0;
    rq.resources        = list;
    rq.number_of_ibs    = 1;
    rq.ibs              = &ib_info;

    int r_sub = amdgpu_cs_submit(ctx , 0, &rq,1);
    printf("[4.4a] submit=%d seq_no=%lu  (mounted on ring ring,GPU moving async)\n",
       r_sub, rq.seq_no);

    struct amdgpu_cs_fence f = {0};
    f.context = ctx;
    f.ip_type = AMDGPU_HW_IP_DMA;
    f.ring    = 0;
    f.fence   = rq.seq_no;
    uint32_t expired = 0;
    int r_wait = amdgpu_cs_query_fence_status(&f , AMDGPU_TIMEOUT_INFINITE, 0 ,&expired);

    int ok = (memcmp(src.cpu , dst.cpu , 4096) == 0);
    printf("[4.4b] wait=%d expired=%u | dst[0..3]=%02x %02x %02x %02x | %s\n",
       r_wait, expired,
       ((uint8_t*)dst.cpu)[0],((uint8_t*)dst.cpu)[1],
       ((uint8_t*)dst.cpu)[2],((uint8_t*)dst.cpu)[3], ok ? "COPY OK" : "MISMATCH");
    return 0;
}
