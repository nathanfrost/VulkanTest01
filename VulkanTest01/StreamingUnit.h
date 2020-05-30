#pragma once
#include<assert.h>
#include<stdint.h>
#include <vulkan/vulkan.h>
#include"QueueCircular.h"
#include"StackNTF.h"
#include"stdArrayUtility.h"
#include"StreamingCookAndRuntime.h"

#define NTF_UNIT_TEST_STREAMING 1

class VulkanPagedStackAllocator;
class StreamingCommandQueueManager;

typedef uint32_t StreamingUnitVersion;
typedef uint8_t StreamingUnitByte;
typedef uint32_t StreamingUnitTexturedGeometryNum;
typedef uint32_t StreamingUnitVerticesNum;
typedef uint32_t StreamingUnitIndicesNum;
typedef uint16_t StreamingUnitTextureDimension;
typedef uint8_t StreamingUnitTextureChannels;

typedef uint32_t IndexBufferValue;

struct TexturedGeometry
{
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indicesSize;
    VkImage textureImage;

    bool Valid() const
    {
        return indicesSize > 0;
    }
};

class AssetLoadingArguments;
struct AssetLoadingPersistentResources;

enum class StreamingCommand : size_t
{
	kLoad, kFirstValidValue = kLoad,
	kUnload, kLastValidValue = kUnload
};
inline void AssertValid(const StreamingCommand command)
{
	assert(command >= StreamingCommand::kFirstValidValue);
	assert(command <= StreamingCommand::kLastValidValue);
}
class StreamingUnitRuntime
{
public:
    StreamingUnitRuntime();
    void Initialize(const VkDevice& device);
    void Free(
        ArraySafeRef<bool> deviceLocalMemoryStreamingUnitsAllocated, 
        RTL_CRITICAL_SECTION*const deviceLocalMemoryCriticalSection,
        const ConstVectorSafeRef<VulkanPagedStackAllocator>& deviceLocalMemoryStreamingUnits,
        const bool deallocateBackToGpu,
        const VkDevice& device);
    void Destroy(const VkDevice& device);

    void AssertValid() const;

    RTL_CRITICAL_SECTION m_stateCriticalSection;
    enum class State:size_t {kUnloaded, kLoading, kLoaded} m_state;
    State StateCriticalSection();

    ConstArraySafeRef<char> m_filenameNoExtension;
    VkSampler m_textureSampler;
#define TODO_REFACTOR_NUM 2//is NTF_OBJECTS_NUM -- todo: generalize #StreamingMemoryBasicModel
    ArraySafe<TexturedGeometry, TODO_REFACTOR_NUM> m_texturedGeometries;
    ArraySafe<VkImageView, TODO_REFACTOR_NUM> m_textureImageViews;

    /*@todo NTF:
    1. Make StreamingUnitRuntime be entirely allocated from a StackNTF by methods like AddTexturePaths(const char*const* texturePaths, const size_t texturePathsNum) so that it can contain a variable amount of everything up to the stack limit.  Use ArraySafe<>'s to index each container subset within the bytestream
    2. Pull texture and model loading code into cooking module that accepts StreamingUnitOld ("StreamingUnitTemplate") and spits out StreamingUnitNew, which is still allocated from a StackNTF and uses ArraySafe<>'s but contains the ready-to-pass-to-Vulkan model and texture data as well as the other variables
    */

    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;///<automatically freed when the VkDescriptorPool is destroyed or reset

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformBufferGpuMemory;
    VkDeviceSize m_uniformBufferOffsetToGpuMemory;
    ArraySafeRef<uint8_t> m_uniformBufferCpuMemory;

    VkDeviceSize m_uniformBufferSizeAligned;//single uniform buffer that contains all uniform information for this streaming unit
    size_t m_uniformBufferSizeUnaligned;///<currently only exists to generate the same value but aligned -January 12, 2020

    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;

    VulkanPagedStackAllocator* m_deviceLocalMemory;

    //BEG_#FrameNumber: if you change one of these constructs, make sure the rest are synchronized
    typedef uint16_t FrameNumber;
    typedef int32_t FrameNumberSigned;
    enum { kFrameNumberSignedMinimum = -2147483647 - 1};//avoid Visual Studio 2015 compiler warning C4146, which incorrectly claims a 32bit signed integer can't store -2^31
    //END_#FrameNumber
    FrameNumber m_lastSubmittedCpuFrame;

    VkFence m_transferQueueFinishedFence, m_graphicsQueueFinishedFence;
};

void StreamingUnitAddToLoadCriticalSection(
	StreamingUnitRuntime*const streamingUnitToLoadPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSection);
void StreamingUnitsAddToLoadCriticalSection(
	VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToLoad,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToLoad,
    RTL_CRITICAL_SECTION*const streamingUnitsAddToLoadCriticalSection);
enum class AssetLoadingArgumentsThreadCommand;
void AssetLoadingThreadExecuteLoad(AssetLoadingArgumentsThreadCommand*const threadCommandPtr, const HANDLE assetLoadingThreadWakeHandle);

void StreamingUnitAddToUnload(
    StreamingUnitRuntime*const streamingUnitToUnloadPtr,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload);
void StreamingUnitsAddToUnload(
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsToAddToUnload,///<not ConstVectorSafeRef because I want to force the passer to use '&', since semantically want to emphasize that the streaming units contained in the vector will be modified, even if the vector itself will not
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsRenderable,
    VectorSafeRef<StreamingUnitRuntime*> streamingUnitsAddToUnload);

class SerializerCookerOut
{
public:
    template<class T>
    inline static void Execute(FILE*const file, T*const data)
    {
        assert(file);
        assert(data);
        Fwrite(file, data, sizeof(*data), 1);
    }
    template<class T>
    inline static void Execute(FILE*const file, ConstArraySafeRef<T> arraySafe, const size_t elementsNum)
    {
        assert(file);
        assert(elementsNum > 0);
        arraySafe.Fwrite(file, elementsNum);
    }
    template<class ElementType, class ElementNumType>
    inline static void Execute(
        FILE*const file,
        const ElementNumType arrayNum,
        ConstArraySafeRef<ElementType> arrayCookerOut,
        ArraySafeRef<StreamingUnitByte>,
        StackCpu<VkDeviceSize>*const,
        const VkDeviceSize,
        size_t*const,
        VkDeviceSize*const)
    {
        assert(arrayNum > 0);
        Execute(file, arrayCookerOut, arrayNum);
    }
};
class SerializerRuntimeIn
{
public:
    template<class T>
    inline static void Execute(FILE*const file, T*const data)
    {
        assert(file);
        assert(data);
        Fread(file, data, sizeof(*data), 1);
    }
    template<class T>
    inline static void Execute(FILE*const file, ArraySafeRef<T> arraySafe, const size_t elementsNum)
    {
        assert(file);
        assert(elementsNum > 0);
        arraySafe.MemcpyFromFread(file, elementsNum);
    }
    template<class ElementType, class ElementNumType>
    inline static void Execute(
        FILE*const file, 
        const ElementNumType arrayNum,
        ConstArraySafeRef<ElementType> arrayCookerOut,
        ArraySafeRef<StreamingUnitByte> bufferRuntimeIn,
        StackCpu<VkDeviceSize>*const stagingBufferMemoryMapCpuToGpuRuntimeIn,
        const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn,
        size_t*const bufferSizeBytesRuntimeIn,
        VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockRuntimeIn)
    {
        assert(file);
        assert(arrayNum > 0);
        assert(stagingBufferMemoryMapCpuToGpuRuntimeIn);
        assert(stagingBufferGpuAlignmentRuntimeIn > 0);
        assert((stagingBufferGpuAlignmentRuntimeIn % 2) == 0);
        
        const size_t bufferSizeBytes = arrayNum * sizeof(arrayCookerOut[0]);
        const bool pushAllocResult = stagingBufferMemoryMapCpuToGpuRuntimeIn->PushAlloc(
            &bufferRuntimeIn, 
            stagingBufferGpuOffsetToAllocatedBlockRuntimeIn,
            CastWithAssert<VkDeviceSize, size_t>(stagingBufferGpuAlignmentRuntimeIn), 
            bufferSizeBytes);
        assert(pushAllocResult);
        Execute(file, bufferRuntimeIn, bufferSizeBytes);

        if (bufferSizeBytesRuntimeIn)
        {
            *bufferSizeBytesRuntimeIn = bufferSizeBytes;
        }
    }
};

template<class Serializer>
inline void TextureSerialize0(
    FILE*const file,
    StreamingUnitTextureDimension*const textureWidth,
    StreamingUnitTextureDimension*const textureHeight,
    StreamingUnitTextureChannels*const textureChannels)
{
    assert(file);
    assert(textureWidth);
    assert(textureHeight);
    assert(textureChannels);

    Serializer::Execute(file, textureWidth);
    Serializer::Execute(file, textureHeight);
    Serializer::Execute(file, textureChannels);
}
template<class Serializer>
inline void TextureSerialize1(   
    FILE*const file,
    ConstArraySafeRef<StreamingUnitByte> pixelsCookOut,
    StackCpu<VkDeviceSize>* stagingBufferMemoryMapCpuToGpuRuntimeIn,
    const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn,
    const size_t bufferSizeBytes,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockRuntimeIn)
{
    ArraySafeRef<StreamingUnitByte> pixelBufferRuntimeIn;
    Serializer::Execute(
        file,
        bufferSizeBytes,
		pixelsCookOut,
		pixelBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        nullptr,
        stagingBufferGpuOffsetToAllocatedBlockRuntimeIn);
}

template<class Serializer>
inline void VertexBufferSerialize(
    FILE*const file,
    StackCpu<VkDeviceSize>*const stagingBufferMemoryMapCpuToGpuRuntimeIn,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockRuntimeIn,
    StreamingUnitVerticesNum*const verticesNum,
    ConstArraySafeRef<Vertex> verticesCookerOut,
    ArraySafeRef<StreamingUnitByte> vertexBufferRuntimeIn,
    size_t*const vertexBufferSizeBytesRuntimeIn,
    const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn)
{
    assert(file);
    assert(verticesNum);

    Serializer::Execute(file, verticesNum);
    Serializer::Execute(
        file,
        *verticesNum,
        verticesCookerOut,
        vertexBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        vertexBufferSizeBytesRuntimeIn,
        stagingBufferGpuOffsetToAllocatedBlockRuntimeIn);
}

template<class Serializer>
inline void IndexBufferSerialize(
    FILE*const file,
    StackCpu<VkDeviceSize>*const stagingBufferMemoryMapCpuToGpuRuntimeIn,
    VkDeviceSize*const stagingBufferGpuOffsetToAllocatedBlockRuntimeIn,
    StreamingUnitIndicesNum*const indicesNum,
    ConstArraySafeRef<IndexBufferValue> indicesCookerOut,
    ArraySafeRef<StreamingUnitByte> indexBufferRuntimeIn,
    size_t*const indexBufferSizeBytesRuntimeIn,
    const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn)
{
    assert(file);
    assert(indicesNum);
    
    Serializer::Execute(file, indicesNum);
    Serializer::Execute(
        file,  
        *indicesNum, 
        indicesCookerOut, 
        indexBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        indexBufferSizeBytesRuntimeIn,
        stagingBufferGpuOffsetToAllocatedBlockRuntimeIn);
}
