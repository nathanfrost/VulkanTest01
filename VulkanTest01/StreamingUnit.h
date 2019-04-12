#pragma once
#include<assert.h>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include<stdint.h>
#include <vulkan/vulkan.h>
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

class SerializerWrite
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
    inline static void Execute(
        FILE*const file, 
        VectorSafeRef<StreamingUnitByte> dataRead, 
        const StreamingUnitByte*const dataWrite, 
        const size_t bytesNum)
    {
        assert(file);
        assert(dataWrite);
        assert(bytesNum > 0);
        Fwrite(file, dataWrite, bytesNum, 1);
    }
};
class SerializerRead
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
    inline static void Execute(
        FILE*const file,
        VectorSafeRef<StreamingUnitByte> dataRead,
        const StreamingUnitByte*const dataWrite,
        const size_t bytesNum)
    {
        assert(file);
        assert(bytesNum > 0);
        dataRead.MemcpyFromFread(file, bytesNum);
    }
};

template<class Serializer>
inline void TextureSerialize(
    FILE*const file,
    StreamingUnitTextureDimension*const textureWidth,
    StreamingUnitTextureDimension*const textureHeight,
    StreamingUnitTextureChannels*const textureChannels,
    VectorSafeRef<StreamingUnitByte> pixelsRead,
    const StreamingUnitByte*const pixelsWrite)
{
    assert(file);
    assert(textureWidth);
    assert(textureHeight);
    assert(textureChannels);
    //pixelsWrite is allowed to be nullptr when Serializer==SerializerRead

    Serializer::Execute(file, textureWidth);
    Serializer::Execute(file, textureHeight);
    Serializer::Execute(file, textureChannels);
    Serializer::Execute(file, pixelsRead, pixelsWrite, ImageSizeBytesCalculate(*textureWidth, *textureHeight, *textureChannels));
}
template<class Serializer>
inline void ModelSerialize(
    FILE*const file,
    ArraySafeRef<Vertex> vertices,
    StreamingUnitVerticesNum*const verticesNum,
    ArraySafeRef<IndexBufferValue> indices, 
    StreamingUnitIndicesNum*const indicesNum)
{
    assert(file);
    assert(verticesNum);
    assert(indicesNum);

    Serializer::Execute(file, verticesNum);
    Serializer::Execute(file, vertices, *verticesNum);
    
    Serializer::Execute(file, indicesNum);
    Serializer::Execute(file, indices, *indicesNum);
}
