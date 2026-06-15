#include <stdio.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <amdgpu.h>


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
    amdgpu_device_deinitialize(dev);
    return 0;
}
