#pragma once
#include<assert.h>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include<stdint.h>
#include <vulkan/vulkan.h>
#include"StackNTF.h"
#include"stdArrayUtility.h"

typedef uint32_t StreamingUnitVersion;
typedef uint8_t StreamingUnitByte;
typedef uint32_t StreamingUnitTexturedGeometryNum;
typedef uint32_t StreamingUnitVerticesNum;
typedef uint32_t StreamingUnitIndicesNum;
typedef uint16_t StreamingUnitTextureDimension;
typedef uint8_t StreamingUnitTextureChannels;

typedef uint32_t IndexBufferValue;
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();
    static void GetAttributeDescriptions(VectorSafeRef<VkVertexInputAttributeDescription> attributeDescriptions);

    bool operator==(const Vertex& other) const;
};
namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const;
    };
}

struct TexturedGeometry
{
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indicesSize;
    VkImage textureImage;
    VkDeviceMemory textureBufferMemory;

    bool Valid() const
    {
        return indicesSize > 0;
    }
};

const char* StreamingUnitFilenameExtensionGet();
void StreamingUnitFilenameExtensionAppend(char*const filenameNoExtension, const size_t filenameNoExtensionSizeBytes);
size_t ImageSizeBytesCalculate(uint16_t textureWidth, uint16_t textureHeight, uint8_t textureChannels);
const char* CookedFileDirectoryGet();

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
        StackCpu*const,
        const VkDeviceSize,
        size_t*const)
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
        StackCpu*const stagingBufferMemoryMapCpuToGpuRuntimeIn,
        const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn,
        size_t*const bufferSizeBytesRuntimeIn)
    {
        assert(file);
        assert(arrayNum > 0);
        assert(stagingBufferMemoryMapCpuToGpuRuntimeIn);
        assert(stagingBufferGpuAlignmentRuntimeIn > 0);
        assert((stagingBufferGpuAlignmentRuntimeIn % 2) == 0);
        
        const size_t bufferSizeBytes = arrayNum * sizeof(arrayCookerOut[0]);
        const bool pushAllocResult = stagingBufferMemoryMapCpuToGpuRuntimeIn->PushAlloc(
            &bufferRuntimeIn, 
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
    StackCpu* stagingBufferMemoryMapCpuToGpuRuntimeIn,
    const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn,
    const size_t bufferSizeBytes)
{
    ArraySafeRef<StreamingUnitByte> pixelBufferRuntimeIn;
    Serializer::Execute(
        file,
        bufferSizeBytes,
        pixelsCookOut,
        pixelBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        nullptr);
}

template<class Serializer>
inline void ModelSerialize(
    FILE*const file,
    StackCpu* stagingBufferMemoryMapCpuToGpuRuntimeIn,
    const VkDeviceSize stagingBufferGpuAlignmentRuntimeIn,
    StreamingUnitVerticesNum*const verticesNum,
    ArraySafeRef<Vertex> verticesCookerOut,
    ArraySafeRef<StreamingUnitByte> vertexBufferRuntimeIn,
    size_t*const vertexBufferSizeBytesRuntimeIn,
    StreamingUnitIndicesNum*const indicesNum,
    ArraySafeRef<IndexBufferValue> indicesCookerOut,
    ArraySafeRef<StreamingUnitByte> indexBufferRuntimeIn,
    size_t*const indexBufferSizeBytesRuntimeIn)
{
    assert(file);
    assert(verticesNum);
    assert(indicesNum);

    Serializer::Execute(file, verticesNum);
    Serializer::Execute(
        file, 
        *verticesNum, 
        verticesCookerOut, 
        vertexBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        vertexBufferSizeBytesRuntimeIn);
    
    Serializer::Execute(file, indicesNum);
    Serializer::Execute(
        file,  
        *indicesNum, 
        indicesCookerOut, 
        indexBufferRuntimeIn,
        stagingBufferMemoryMapCpuToGpuRuntimeIn,
        stagingBufferGpuAlignmentRuntimeIn,
        indexBufferSizeBytesRuntimeIn);
}
