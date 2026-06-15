#include <stdio.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <amdgpu.h>
#include <drm/amdgpu_drm.h>


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
    printf("device: %s , DRM: %u.%u , family_id:%u\n",
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
    //allow cpu to read/write this object buffer
    void *cpu;
    amdgpu_bo_cpu_map(bo, &cpu);

    // prepare mock data
    for(int i = 0 ; i < 4096 ;i++) ((uint8_t*)cpu)[i] = (uint8_t)(i*7 + 3);
    printf("read previous 4 bytes:  %02x %02x %02x %02x\n",
    ((uint8_t*)cpu)[0] , ((uint8_t*)cpu)[1] , ((uint8_t*)cpu)[2] , ((uint8_t*)cpu)[3]);

    amdgpu_device_deinitialize(dev);
    return 0;
}
