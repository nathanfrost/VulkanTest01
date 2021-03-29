#include<assert.h>
///@todo: replace STL with more performant datastructures
#include<string>
#include<vector>

#include"bmpImageFormat.h"
#include"MemoryUtil.h"
#include"ntf_math.h"
#include"ntf_vulkan.h"
#include"StreamingCookAndRuntime.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include"tiny_obj_loader.h"

using namespace ntf;

//BEG_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
//BEG_#StbMemoryManagement
StackCpu<size_t>* g_stbAllocator;
void* __cdecl stb_malloc(size_t _Size)
{
    assert(g_stbAllocator);

    void* memory;
    g_stbAllocator->PushAlloc(&memory, 0, _Size);
    assert(memory);
    return memory;
}
void* __cdecl stb_assertRealloc(void*  _Block, size_t _Size) { assert(false); return nullptr; }
void __cdecl stb_nullFree(void* const block) {}
#include"stb_image.h"

//BEG_Globals
VkInstance m_instance;
VkDebugReportCallbackEXT m_callback;
VkPhysicalDevice m_physicalDevice;
QueueFamilyIndices m_queueFamilyIndices;
VkDevice m_device;//interface to the physical device; must be destroyed before the physical device.  Need not be synchronized until, of course, vkDestroyDevice()
VkQueue m_graphicsQueue, m_presentQueue, m_transferQueue;//queues are implicitly cleaned up with the logical device; no need to delete
VkCommandPool m_commandPoolPrimary;
VulkanPagedStackAllocator m_deviceLocalMemoryTracker;
VkCommandBuffer m_commandBufferPrimary;//automatically freed when VkCommandPool is destroyed
VkBuffer m_stagingBufferGpu;
VkDeviceMemory m_stagingBufferGpuMemory;
VkDeviceSize m_offsetToFirstByteOfStagingBuffer;
VkDeviceSize m_stagingBufferGpuAlignmentStandard;
ArraySafeRef<uint8_t> m_stagingBufferMemoryMapCpuToGpu;
VkFence m_fence;
//END_Globals

//BEG_#StbMemoryManagement
void STBAllocatorCreate(StackCpu<size_t>**const stbAllocatorPtrPtr)
{
    NTF_REF(stbAllocatorPtrPtr, stbAllocatorPtr);

    stbAllocatorPtr = new StackCpu<size_t>();
    const size_t sizeBytes = 128 * 1024 * 1024;
    stbAllocatorPtr->Initialize(reinterpret_cast<uint8_t*>(malloc(sizeBytes)), sizeBytes);
}
void STBAllocatorDestroy(StackCpu<size_t>**const stbAllocatorPtrPtr)
{
    NTF_REF(stbAllocatorPtrPtr, stbAllocatorPtr);

    free(stbAllocatorPtr->GetMemory());
    stbAllocatorPtr->Destroy();
    delete stbAllocatorPtr;
    stbAllocatorPtr = nullptr;
}

/** use when you're done with the data returned from stbi_load(); never call stbi_image_free() directly; only use this function to clear all stack
allocations stbi made using the (global) stbAllocatorPtr */
void STBIImageFree(void*const retval_from_stbi_load, StackCpu<size_t>*const stbAllocatorPtr)
{
    assert(stbAllocatorPtr);
    auto& stbAllocator = *stbAllocatorPtr;

    stbi_image_free(retval_from_stbi_load);
    stbAllocator.Clear();
}
//END_#StbMemoryManagement
//END_STB_IMAGE

using namespace std;

typedef string StreamingUnitCookerString;
typedef float StreamingUnitCookerScalar;
struct StreamingUnitCookerTexturedGeometry
{
    StreamingUnitCookerString m_texturePathRelativeFromRoot;
    StreamingUnitCookerString m_modelPathRelativeFromRoot;
    StreamingUnitCookerScalar m_uniformScale;
};
typedef vector<StreamingUnitCookerTexturedGeometry> StreamingUnitCookerTexturedGeometries;
struct StreamingUnitCooker
{
    void FileNameOutputSet(const char*const filenameNoExtension);
    void TexturedGeometryAdd(
        const StreamingUnitCookerString& texturePath,
        const StreamingUnitCookerString& modelPath,
        const float uniformScale);
    void Cook();
    void Clear();


    StreamingUnitCookerString m_fileNameOutput;
    StreamingUnitCookerTexturedGeometries m_texturedGeometries;
    bool m_flipNormals = false;
};

void StreamingUnitCooker::FileNameOutputSet(const char*const filenameNoExtension)
{
    ArraySafe<char, 512> filenameExtension;
    filenameExtension.Sprintf("%s.%s", filenameNoExtension, StreamingUnitFilenameExtensionGet());
    m_fileNameOutput = StreamingUnitCookerString(filenameExtension.begin());
}

void StreamingUnitCooker::TexturedGeometryAdd(
    const StreamingUnitCookerString& texturePath,
    const StreamingUnitCookerString& modelPath,
    const float uniformScale)
{
    StreamingUnitCookerTexturedGeometry texturedGeometry;
    texturedGeometry.m_texturePathRelativeFromRoot = texturePath;
    texturedGeometry.m_modelPathRelativeFromRoot = modelPath;
    texturedGeometry.m_uniformScale = uniformScale;
    m_texturedGeometries.push_back(texturedGeometry);
}

void StreamingUnitCooker::Cook()
{
    const StreamingUnitVersion version = 0;
    const StreamingUnitCookerString rootDirectoryWithTrailingBackslash("..\\..\\VulkanTest01\\VulkanTest01\\");
    const StreamingUnitCookerString cookedFileDirectoryWithTrailingBackslash = 
        rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(CookedFileDirectoryGet()) + StreamingUnitCookerString("\\");
    FILE* f;
    StreamingUnitCookerString filePathOutput = cookedFileDirectoryWithTrailingBackslash + m_fileNameOutput;
    Fopen(&f, filePathOutput.c_str(), "wb");///<@todo: make directory if necessary

    //BEG_GENERALIZE_READER_WRITER
    Fwrite(f, &version, sizeof(version), 1);
    const StreamingUnitTexturedGeometryNum texturedGeometryNum = CastWithAssert<size_t, StreamingUnitTexturedGeometryNum>(m_texturedGeometries.size());
    Fwrite(f, &texturedGeometryNum, sizeof(texturedGeometryNum), 1);
    //END_GENERALIZE_READER_WRITER

    for (size_t texturedGeometryIndex = 0; texturedGeometryIndex < texturedGeometryNum; ++texturedGeometryIndex)
    {
        const StreamingUnitCookerTexturedGeometry& texturedGeometry = m_texturedGeometries[texturedGeometryIndex];

        const char* texturePathRelativeFromRootCStr = texturedGeometry.m_texturePathRelativeFromRoot.c_str();
        if (texturePathRelativeFromRootCStr[0])
        {
            //cook and write texture
            const StreamingUnitCookerString texturePath = rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(texturePathRelativeFromRootCStr);
            int textureWidth, textureHeight, textureChannels;

            assert(g_stbAllocator->GetFirstByteFree() == 0);//ensure we can Clear() the whole stack correctly in STBIImageFree() (eg there's nothing already allocated in the stack)
            NTF_STATIC_ASSERT(sizeof(stbi_uc) == sizeof(StreamingUnitByte));
            StreamingUnitByte* pixels = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);///<@todo: output ASTC (Android and PC), BC (PC), etc (see VK_FORMAT_* in vulkan_core.h).  In theory I could probably upload and image transition everything to VK_IMAGE_TILING_OPTIMAL and then write that buffer to disk for the fastest possible texture load, but in practice I don't think this can be done in 2019 with Vulkan (or DX12 or Metal)
            assert(pixels);
            textureChannels = 4;///<this is true because we passed STBI_rgb_alpha; stbi_load() reports the number of textures actually present even as it respects this flag

            StreamingUnitTextureDimension textureWidthCook = CastWithAssert<int, StreamingUnitTextureDimension>(textureWidth);
            StreamingUnitTextureDimension textureHeightCook = CastWithAssert<int, StreamingUnitTextureDimension>(textureHeight);
            StreamingUnitTextureChannels textureChannelsCook = CastWithAssert<int, StreamingUnitTextureChannels>(textureChannels);

            TextureSerializeHeader<SerializerCookerOut>(f, &textureWidthCook, &textureHeightCook, &textureChannelsCook);
            const size_t imageSizeBytes = ImageSizeBytesCalculate(textureWidth, textureHeight, textureChannels);
            TextureSerializeImagePixels<SerializerCookerOut>(f, ConstArraySafeRef<StreamingUnitByte>(pixels, imageSizeBytes), nullptr, 0, imageSizeBytes, nullptr);

            const uint32_t mipLevels = MipsLevelsCalculate(textureWidth, textureHeight);
            VkDeviceMemory memoryHandleTextureOptimal;
            VkImage textureImage;
            VkMemoryRequirements memoryRequirements;
            VkDeviceSize stagingBufferGpuOffsetToTextureOptimal;
            const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
            const bool createAllocateBindImageResult = CreateAllocateBindImageIfAllocatorHasSpace(
                &textureImage,
                &m_deviceLocalMemoryTracker,
                &memoryRequirements,
                &stagingBufferGpuOffsetToTextureOptimal,
                &memoryHandleTextureOptimal,
                textureWidth,
                textureHeight,
                mipLevels,
                imageFormat,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                false,
                m_physicalDevice,
                m_device);
            assert(createAllocateBindImageResult);

            memcpy(&m_stagingBufferMemoryMapCpuToGpu[stagingBufferGpuOffsetToTextureOptimal], pixels, imageSizeBytes);
            //{
            //    ArraySafe<char, 128> filename;
            //    filename.Sprintf("E:\\readbackImageMip0.bmp");
            //    WriteR8G8B8A8ToBmpFile(pixels, textureWidth, textureHeight, filename);
            //}

            //generate and write mips
            VkBuffer stagingBuffer;
            CreateBuffer(
                &stagingBuffer,
                &stagingBufferGpuOffsetToTextureOptimal,
                m_stagingBufferGpuMemory,
                m_offsetToFirstByteOfStagingBuffer,
                imageSizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                m_device,
                m_physicalDevice);

            //transition memory to format optimal for copying from CPU->GPU
            const uint32_t graphicsQueueIndex = m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue];
            const VkImageAspectFlagBits colorAspectBit = VK_IMAGE_ASPECT_COLOR_BIT;
            BeginCommandBuffer(m_commandBufferPrimary, m_device);
            ImageMemoryBarrier(
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                graphicsQueueIndex,
                graphicsQueueIndex,
                colorAspectBit,
                textureImage,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                mipLevels,
                m_commandBufferPrimary,
                m_instance);
            CopyBufferToImage(stagingBuffer, textureImage, textureWidth, textureHeight, 0, m_commandBufferPrimary, m_device, m_instance);

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = textureImage;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = colorAspectBit;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;

            int32_t mipWidth = textureWidth;
            int32_t mipHeight = textureHeight;
            for (uint32_t i = 1; i < mipLevels; i++)
            {
                //transition previous mip level to read transfer
                const uint32_t iMinusOne = i - 1;
                barrier.subresourceRange.baseMipLevel = iMinusOne;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                CmdPipelineImageBarrier(&barrier, m_commandBufferPrimary, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

                VkImageBlit blit{};
                /*  #VkImageRegionZOffset:  Vulkan considers 2D images to have a depth of 1, so to specify a 2D region of pixels use offset[0].z = 0 and
                                            offset[1].z = 1 */
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };

                blit.srcSubresource.aspectMask = colorAspectBit;
                blit.srcSubresource.mipLevel = iMinusOne;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;

                //#VkImageRegionZOffset
                DivideByTwoIfGreaterThanOne(&mipWidth);
                DivideByTwoIfGreaterThanOne(&mipHeight);
                blit.dstOffsets[0] = { 0, 0, 0 };
                blit.dstOffsets[1] = { mipWidth, mipHeight, 1 };

                blit.dstSubresource.aspectMask = colorAspectBit;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = 1;

                vkCmdBlitImage(
                    m_commandBufferPrimary,
                    textureImage,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    textureImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit,
                    VK_FILTER_LINEAR);
            }

            //this image now contains all its mip's -- prepare it to be read out and cooked to disk
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            CmdPipelineImageBarrier(&barrier, m_commandBufferPrimary, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            EndCommandBuffer(m_commandBufferPrimary);
            SubmitCommandBuffer(
                nullptr,
                ConstVectorSafeRef<VkSemaphore>(),
                ConstVectorSafeRef<VkSemaphore>(),
                ConstArraySafeRef<VkPipelineStageFlags>(),
                m_commandBufferPrimary,
                m_graphicsQueue,
                m_fence,
                m_instance);

            FenceWaitUntilSignalled(m_fence, m_device);
            FenceReset(m_fence, m_device);

            //write out mips
            int32_t textureWidthCurrentMipLevel = textureWidth;
            int32_t textureHeightCurrentMipLevel = textureHeight;
            VkImage readbackImage;
            for (uint32_t mipLevel = 1; mipLevel < mipLevels; ++mipLevel)
            {
                DivideByTwoIfGreaterThanOne(&textureWidthCurrentMipLevel);
                DivideByTwoIfGreaterThanOne(&textureHeightCurrentMipLevel);

                VkMemoryRequirements memoryRequirements;
                VkDeviceSize memoryOffsetTextureLinear;
                VkDeviceMemory memoryHandleTextureLinear;

                const bool createAllocateBindImageResult = CreateAllocateBindImageIfAllocatorHasSpace(
                    &readbackImage,
                    &m_deviceLocalMemoryTracker,
                    &memoryRequirements,
                    &memoryOffsetTextureLinear,
                    &memoryHandleTextureLinear,
                    static_cast<uint32_t>(textureWidthCurrentMipLevel),
                    static_cast<uint32_t>(textureHeightCurrentMipLevel),
                    1,
                    imageFormat,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_TILING_LINEAR,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    false,
                    m_physicalDevice,
                    m_device);
                assert(createAllocateBindImageResult);
                void* readbackBufferCpuMemoryCPtr = nullptr;
                MapMemory(&readbackBufferCpuMemoryCPtr, memoryHandleTextureLinear, memoryOffsetTextureLinear, memoryRequirements.size, m_device);
                const ConstArraySafeRef<uint8_t> readbackBufferCpuMemory(reinterpret_cast<uint8_t*>(readbackBufferCpuMemoryCPtr), memoryRequirements.size);

                BeginCommandBuffer(m_commandBufferPrimary, m_device);
                ImageMemoryBarrier(
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    graphicsQueueIndex,
                    graphicsQueueIndex,
                    colorAspectBit,
                    readbackImage,
                    0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    1,
                    m_commandBufferPrimary,
                    m_instance);


                VkImageCopy region = {};
                region.srcOffset = { 0, 0, 0 };
                region.dstOffset = { 0, 0, 0 };

                region.srcSubresource.aspectMask = colorAspectBit;
                region.srcSubresource.mipLevel = mipLevel;
                region.srcSubresource.baseArrayLayer = 0;
                region.srcSubresource.layerCount = 1;

                region.dstSubresource.aspectMask = colorAspectBit;
                region.dstSubresource.mipLevel = 0;
                region.dstSubresource.baseArrayLayer = 0;
                region.dstSubresource.layerCount = 1;

                //#VkImageRegionZOffset
                region.extent.width = textureWidthCurrentMipLevel;
                region.extent.height = textureHeightCurrentMipLevel;
                region.extent.depth = 1;

                vkCmdCopyImage(
                    m_commandBufferPrimary, 
                    textureImage, 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                    readbackImage, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                    1, 
                    &region);

                EndCommandBuffer(m_commandBufferPrimary);
                SubmitCommandBuffer(
                    nullptr,
                    ConstVectorSafeRef<VkSemaphore>(),
                    ConstVectorSafeRef<VkSemaphore>(),
                    ConstArraySafeRef<VkPipelineStageFlags>(),
                    m_commandBufferPrimary,
                    m_graphicsQueue,
                    m_fence,
                    m_instance);

                FenceWaitUntilSignalled(m_fence, m_device);
                FenceReset(m_fence, m_device);
                vkResetCommandBuffer(m_commandBufferPrimary, 0);
                VkMappedMemoryRange mappedMemoryRange;
                mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedMemoryRange.pNext = nullptr;
                mappedMemoryRange.memory = memoryHandleTextureLinear;
                mappedMemoryRange.offset = memoryOffsetTextureLinear;
                mappedMemoryRange.size = memoryRequirements.size;
                vkFlushMappedMemoryRanges(m_device, 1, &mappedMemoryRange);

                VkSubresourceLayout readbackImageSubresourceLayout;
                VkImageSubresource readbackImageSubresource;
                readbackImageSubresource.aspectMask = colorAspectBit;
                readbackImageSubresource.mipLevel = 0;
                readbackImageSubresource.arrayLayer = 0;
                vkGetImageSubresourceLayout(m_device, readbackImage, &readbackImageSubresource, &readbackImageSubresourceLayout);
                assert(readbackImageSubresourceLayout.size == memoryRequirements.size);

                const ConstArraySafeRef<uint8_t> bitmapToWrite(
                    readbackBufferCpuMemory.GetAddressOfUnderlyingArray() + readbackImageSubresourceLayout.offset,
                    memoryRequirements.size - readbackImageSubresourceLayout.offset);

                const size_t bytesInPixel = 4;
                const size_t textureCurrentMipNumBytesInRowNoPadding = textureWidthCurrentMipLevel*bytesInPixel;
                assert(textureWidthCurrentMipLevel > 0);
                assert(textureHeightCurrentMipLevel > 0);
                assert(readbackImageSubresourceLayout.rowPitch >= textureCurrentMipNumBytesInRowNoPadding);
                assert(readbackImageSubresourceLayout.size > 0);
                assert(readbackImageSubresourceLayout.size >= readbackImageSubresourceLayout.rowPitch*textureHeightCurrentMipLevel);
                assert(readbackImageSubresourceLayout.size%readbackImageSubresourceLayout.rowPitch == 0);
                assert(readbackImageSubresourceLayout.size / readbackImageSubresourceLayout.rowPitch == textureHeightCurrentMipLevel);

                for (size_t rowIndex = 0; rowIndex < textureHeightCurrentMipLevel; ++rowIndex)
                {
                    const size_t rowByteIndex = rowIndex*readbackImageSubresourceLayout.rowPitch;
                    ConstArraySafeRef<StreamingUnitByte> row(&bitmapToWrite[rowByteIndex], textureCurrentMipNumBytesInRowNoPadding);
#if NTF_DEBUG
                    bitmapToWrite[rowByteIndex + textureCurrentMipNumBytesInRowNoPadding - 1];//no overrunning the array
#endif//#if NTF_DEBUG
                    TextureSerializeImagePixels<SerializerCookerOut>(
                        f,
                        row,
                        nullptr, 
                        0, 
                        textureCurrentMipNumBytesInRowNoPadding,
                        nullptr);
                }

                //ArraySafe<char, 128> filename;
                //filename.Sprintf("E:\\readbackImageMip%i.bmp", mipLevel);
                //WriteR8G8B8A8ToBmpFile(
                //    bitmapToWrite,
                //    textureWidthCurrentMipLevel,
                //    textureHeightCurrentMipLevel,
                //    readbackImageSubresourceLayout.rowPitch,
                //    readbackImageSubresourceLayout.size,
                //    filename);
                vkInvalidateMappedMemoryRanges(m_device, 1, &mappedMemoryRange);
                vkUnmapMemory(m_device, memoryHandleTextureLinear);
                vkDestroyImage(m_device, readbackImage, GetVulkanAllocationCallbacks());
            }

            STBIImageFree(pixels, g_stbAllocator);
            vkDestroyBuffer(m_device, stagingBuffer, GetVulkanAllocationCallbacks());
            vkDestroyImage(m_device, textureImage, GetVulkanAllocationCallbacks());
        }

        //cook and write vertex and index buffers
        std::vector<Vertex> vertices;
        std::vector<IndexBufferValue> indices;

        ///@todo: replace OBJ with binary FBX loading
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        const bool loadObjResult = tinyobj::LoadObj(
            &attrib, 
            &shapes, 
            &materials, 
            &err, 
            (rootDirectoryWithTrailingBackslash + texturedGeometry.m_modelPathRelativeFromRoot).c_str());
        assert(loadObjResult);

        ///@todo: replace this STL with a good, static-memory hashmap
        //build index list and un-duplicate vertices
        std::unordered_map<Vertex, IndexBufferValue> uniqueVertices = {};

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex = {};

                vertex.pos =
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] //the origin of texture coordinates in Vulkan is the top-left corner, whereas the OBJ format assumes the bottom-left corner
                };

                vertex.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<IndexBufferValue>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
        if (m_flipNormals)
        {
            const size_t indicesSize = indices.size();
            assert(indicesSize % 3 == 0);
            for (size_t indexOfFirstElementOfTriplet = 0; indexOfFirstElementOfTriplet < indicesSize; indexOfFirstElementOfTriplet += 3)
            {
                const size_t indexOfLastElementOfTriplet = indexOfFirstElementOfTriplet + 2;
                const IndexBufferValue originalValueOfFirstElement = indices[indexOfFirstElementOfTriplet];
                indices[indexOfFirstElementOfTriplet] = indices[indexOfLastElementOfTriplet];
                indices[indexOfLastElementOfTriplet] = originalValueOfFirstElement;
            }
        }

        if (texturedGeometry.m_uniformScale != 1.f)
        {
            for (auto& vertex : vertices)
            {
                vertex.pos *= texturedGeometry.m_uniformScale;
            }
        }

        assert(vertices.size() > 0);
        assert(indices.size() > 0);
        StreamingUnitVerticesNum verticesNum = CastWithAssert<size_t, StreamingUnitVerticesNum>(vertices.size());
        const ConstArraySafeRef<Vertex> verticesArraySafe(&vertices[0], verticesNum);
        VertexBufferSerialize<SerializerCookerOut>(
            f,
            nullptr,
            nullptr,
            &verticesNum,
            verticesArraySafe,
            ArraySafeRef<StreamingUnitByte>(),
            nullptr,
            0);

        StreamingUnitIndicesNum indicesNum = CastWithAssert<size_t, StreamingUnitIndicesNum>(indices.size());
        const ConstArraySafeRef<IndexBufferValue> indicesArraySafe(&indices[0], indices.size());
        IndexBufferSerialize<SerializerCookerOut>(
            f, 
            nullptr, 
            nullptr, 
            &indicesNum, 
            indicesArraySafe, 
            ArraySafeRef<StreamingUnitByte>(),
            nullptr,
            0);
    }
    Fclose(f);
}

void StreamingUnitCooker::Clear()
{
    m_fileNameOutput.clear();
    m_texturedGeometries.clear();
}

void VulkanInitialize()
{
    const VkResult volkInitializeResult = volkInitialize();//load Vulkan dll; get Vulkan function pointer that gets other Vulkan function pointers
    NTF_VK_ASSERT_SUCCESS(volkInitializeResult);

    VectorSafe<const char*, NTF_VALIDATION_LAYERS_SIZE> validationLayers;
    ValidationLayersInitialize(&validationLayers);
    m_instance = InstanceCreate(validationLayers);

    volkLoadInstance(m_instance);//load Vulkan function pointers using Vulkan's loader dispatch code (supports multiple devices at the performance cost of additional indirection)
    m_callback = SetupDebugCallback(m_instance, validationLayers.size() > 0);
    
    VectorSafe<VkPhysicalDevice, 8> physicalDevices;
    PhysicalDevicesGet(&physicalDevices, m_instance);
    assert(physicalDevices.size() > 0);
    m_physicalDevice = physicalDevices[0];

    NTFVulkanInitialize(m_physicalDevice);

    VectorSafe<VkQueueFamilyProperties, 8> queueFamilyProperties;
    PhysicalDeviceQueueFamilyPropertiesGet(&queueFamilyProperties, m_physicalDevice);
    const QueueFamilyIndices::IndexDataType queueFamilyCount = CastWithAssert<size_t, QueueFamilyIndices::IndexDataType>(queueFamilyProperties.size());
    for (QueueFamilyIndices::IndexDataType queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
    {
        const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[queueFamilyIndex];
        if (queueFamilyProperty.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue] = 
                m_queueFamilyIndices.index[QueueFamilyIndices::Type::kComputeQueue] = 
                m_queueFamilyIndices.index[QueueFamilyIndices::Type::kTransferQueue] = 
                m_queueFamilyIndices.index[QueueFamilyIndices::Type::kPresentQueue] = queueFamilyIndex;
            break;
        }
    }
    assert(m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue] != -1);

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &supportedFeatures);
    assert(supportedFeatures.samplerAnisotropy);

    VectorSafe<const char*, NTF_DEVICE_EXTENSIONS_NUM> deviceExtensions(0);
    CreateLogicalDevice(
        &m_device,
        &m_graphicsQueue,
        &m_presentQueue,
        &m_transferQueue,
        deviceExtensions,
        validationLayers,
        m_queueFamilyIndices,
        m_physicalDevice);
    volkLoadDevice(m_device);//load Vulkan function pointers for the one-and-only Vulkan device for minimal indirection and maximum performance

    CreateCommandPool(&m_commandPoolPrimary, m_queueFamilyIndices.index[QueueFamilyIndices::Type::kGraphicsQueue], m_device, m_physicalDevice);
    m_deviceLocalMemoryTracker.Initialize(m_device, m_physicalDevice);
    AllocateCommandBuffers(
        ArraySafeRef<VkCommandBuffer>(&m_commandBufferPrimary, 1),
        m_commandPoolPrimary,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1,
        m_device);

    const size_t stagingBufferCpuToGpuSizeBytes = 128 * 1024 * 1024;
    const VkDeviceSize stagingBufferCpuToGpuSizeAligned = AlignToNonCoherentAtomSize(stagingBufferCpuToGpuSizeBytes);
    CreateBuffer(
        &m_stagingBufferGpu,
        &m_stagingBufferGpuMemory,
        &m_deviceLocalMemoryTracker,
        &m_offsetToFirstByteOfStagingBuffer,
        stagingBufferCpuToGpuSizeAligned,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        true,///<this buffer will be memory mapped, so respect alignment
        m_device,
        m_physicalDevice);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, m_stagingBufferGpu, &memRequirements);
    m_stagingBufferGpuAlignmentStandard = memRequirements.alignment;

    void* stagingBufferMemoryMapCpuToGpuPtr;
    MapMemory(&stagingBufferMemoryMapCpuToGpuPtr, m_stagingBufferGpuMemory, m_offsetToFirstByteOfStagingBuffer, stagingBufferCpuToGpuSizeAligned, m_device);
    m_stagingBufferMemoryMapCpuToGpu = ArraySafeRef<uint8_t>(reinterpret_cast<uint8_t*>(stagingBufferMemoryMapCpuToGpuPtr), stagingBufferCpuToGpuSizeBytes);

    FenceCreate(&m_fence, static_cast<VkFenceCreateFlagBits>(0), m_device);
}

int main()
{
    STBAllocatorCreate(&g_stbAllocator);
    VulkanInitialize();

    StreamingUnitCooker streamingUnitCooker;
    //,  /*"textures/HumanFighter_01_Diff.tga"*/ /*"textures/container_clean_diffuse01.jpeg"*//*"textures/appleD.jpg"*/,
    //const char*const m_modelPath[TODO_REFACTOR_NUM] = { /*"models/Orange.obj"*/, /*"models/Container_OBJ.obj",*/ /*"models/apple textured obj.obj"*/ };//#StreamingMemoryBasicModel
    //const float m_uniformScale[TODO_REFACTOR_NUM] = { /*0.5f,*//*,.0025f*//*.01f,*/ /*1.f*/ };//#StreamingMemoryBasicModel

    streamingUnitCooker.FileNameOutputSet(g_streamingUnitName_UnitTest0);
    streamingUnitCooker.m_flipNormals = true;

    streamingUnitCooker.TexturedGeometryAdd("textures/skull.jpg", "models/skull.obj", .05f);
    streamingUnitCooker.TexturedGeometryAdd("textures/Banana.jpg", "models/Banana.obj", .005f);

    streamingUnitCooker.Cook();
    streamingUnitCooker.Clear();


    streamingUnitCooker.FileNameOutputSet(g_streamingUnitName_UnitTest1);
    streamingUnitCooker.m_flipNormals = true;

    streamingUnitCooker.TexturedGeometryAdd("textures/chalet.jpg", "models/chalet.obj", 1.f);
    streamingUnitCooker.TexturedGeometryAdd("textures/cat_diff.tga", "models/cat.obj", 1.f);

    streamingUnitCooker.Cook();
    streamingUnitCooker.Clear();


    streamingUnitCooker.FileNameOutputSet(g_streamingUnitName_UnitTest2);
    streamingUnitCooker.m_flipNormals = true;

    streamingUnitCooker.TexturedGeometryAdd("textures/appleD.jpg", "models/apple textured obj.obj", .01f);
    streamingUnitCooker.TexturedGeometryAdd("textures/container_clean_diffuse01.jpeg", "models/Container_OBJ.obj", .002f);

    streamingUnitCooker.Cook();
    streamingUnitCooker.Clear();


    ///@todo: clean up "no texture"
    streamingUnitCooker.FileNameOutputSet(g_streamingUnitName_TriangleClockwise);
    streamingUnitCooker.m_flipNormals = false;
    streamingUnitCooker.TexturedGeometryAdd(StreamingUnitCookerString(""), "models/triangle_norm_away_clockwise.obj", 1.f);
    streamingUnitCooker.Cook();
    streamingUnitCooker.Clear();

    ///@todo: clean up "no texture"
    streamingUnitCooker.FileNameOutputSet(g_streamingUnitName_TriangleCounterClockwise);
    streamingUnitCooker.m_flipNormals = false;
    streamingUnitCooker.TexturedGeometryAdd(StreamingUnitCookerString(""), "models/triangle_norm_away_counterclockwise.obj", 1.f);
    streamingUnitCooker.Cook();
    streamingUnitCooker.Clear();

    STBAllocatorDestroy(&g_stbAllocator);
    return 0;//success
}

