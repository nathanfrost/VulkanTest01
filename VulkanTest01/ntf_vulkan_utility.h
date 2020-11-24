#include"volk.h"

///@todo: unit test
inline uint32_t Cast_VkDeviceSize_uint32_t(const VkDeviceSize vkDeviceSize)
{
    const uint32_t vkDeviceSize_uint32_t = static_cast<uint32_t>(vkDeviceSize);
    assert(vkDeviceSize_uint32_t == vkDeviceSize);
    return vkDeviceSize_uint32_t;
}
///@todo: unit test
inline size_t Cast_VkDeviceSize_size_t(const VkDeviceSize vkDeviceSize)
{
    const size_t vkDeviceSize_size_t = static_cast<size_t>(vkDeviceSize);
    assert(vkDeviceSize_size_t == vkDeviceSize);
    return vkDeviceSize_size_t;
}