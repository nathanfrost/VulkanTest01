#pragma once
#include"volk.h"
#include"glmNTF.h"
#include"stdArrayUtility.h"

//streaming unit names
const char*const g_streamingUnitName_UnitTest0 = "unitTest0";
const char*const g_streamingUnitName_UnitTest1 = "unitTest1";
const char*const g_streamingUnitName_UnitTest2 = "unitTest2";
const char*const g_streamingUnitName_TriangleCounterClockwise = "triangleCounterClockwise";
const char*const g_streamingUnitName_TriangleClockwise = "triangleClockwise";

typedef uint32_t StreamingUnitVersion;
typedef uint8_t StreamingUnitByte;
typedef uint32_t StreamingUnitTexturedGeometryNum;
typedef uint32_t StreamingUnitVerticesNum;
typedef uint32_t StreamingUnitIndicesNum;
typedef uint32_t StreamingUnitTextureDimension;
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

const char* StreamingUnitFilenameExtensionGet();
size_t ImageSizeBytesCalculate(
    StreamingUnitTextureDimension textureWidth, 
    StreamingUnitTextureDimension textureHeight, 
    StreamingUnitTextureChannels textureChannels);
uint32_t MipsLevelsCalculate(const uint32_t textureWidth, const uint32_t textureHeight);
const char* CookedFileDirectoryGet();
