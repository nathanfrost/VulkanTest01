#pragma once
#include<assert.h>
#include<stdint.h>
#include <vulkan/vulkan.h>
#include"StackNTF.h"
#include"stdArrayUtility.h"
#include"StreamingCookAndRuntime.h"

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

class StreamingUnitRuntime
{
public:
    StreamingUnitRuntime();
    void Initialize();
    void Free(const VkDevice& device);
    void Destroy();

    enum State {    kFirstValidValue, kNotLoaded = kFirstValidValue,
                    kLoading, kReady, 
                    kLastValidValue, kUnloading = kLastValidValue};
    State StateMutexed() const;
    void StateMutexed(const State state);
    void AssertValid() const;


    ArraySafe<char,128> m_filenameNoExtension;///<@todo NTF: consider creating a string database for this
    VkSampler m_textureSampler;
#define TODO_REFACTOR_NUM 2//is NTF_OBJECTS_NUM -- todo: generalize #StreamingMemory
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
    size_t m_uniformBufferSizeUnaligned;///<@todo: only exists to generate the same value but aligned

    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;

    //BEG_#FrameNumber: if you change one of these constructs, make sure the rest are synchronized
    typedef uint16_t FrameNumber;
    typedef int32_t FrameNumberSigned;
    enum { kFrameNumberSignedMinimum = -2147483647 - 1};//avoid Visual Studio 2015 compiler warning C4146, which incorrectly claims a 32bit signed integer can't store -2^31
    //END_#FrameNumber
    FrameNumber m_lastSubmittedCpuFrame;

private:
    ///@todo: data should only be accessed by the main thread when in the appropriate m_state is (typically kNotLoaded or kReady) -- I could enforce this with methods
    
    class StateMutexed
    {
    public:
        StateMutexed();
        void Initialize();
        void Destroy();
        State Get() const;
        void Set(const State state);
        void AssertValid() const;

    private:
#if NTF_DEBUG
        bool m_initialized;
#endif//#if NTF_DEBUG
        HANDLE m_mutex;
        State m_state;
    } m_stateMutexed;

    friend DWORD WINAPI AssetLoadingThread(void* arg);
    VkFence m_transferQueueFinishedFence, m_graphicsQueueFinishedFence;
};

void StreamingUnitLoadStart(StreamingUnitRuntime*const streamingUnitPtr, const HANDLE assetLoadingThreadWakeHandle);

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
    inline static void Execute(FILE*const file, ArraySafeRef<T> arraySafe, const size_t elementsNum)
    {
        assert(file);
        assert(elementsNum > 0);
        arraySafe.Fwrite(file, elementsNum);
    }
    template<class ElementType, class ElementNumType>
    inline static void Execute(
        FILE*const file,
        const ElementNumType arrayNum,
        ArraySafeRef<ElementType> arrayCookerOut,
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
        ArraySafeRef<ElementType> arrayCookerOut,
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
    ArraySafeRef<StreamingUnitByte> pixelsCookOut,
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
    ArraySafeRef<Vertex> verticesCookerOut,
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
    ArraySafeRef<IndexBufferValue> indicesCookerOut,
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
