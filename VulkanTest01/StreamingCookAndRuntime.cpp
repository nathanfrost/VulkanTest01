#include"StreamingCookAndRuntime.h"
#include<assert.h>
#include<functional>
#include<glm/gtx/hash.hpp>

size_t std::hash<Vertex>::operator()(Vertex const& vertex) const
{
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
}

bool Vertex::operator==(const Vertex& other) const
{
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
}

const char* StreamingUnitFilenameExtensionGet()
{
    static const char*const ret = "streamingUnit";
    return ret;
}

///@todo: uint16_t -> uint32_t across Streaming system and everything
size_t ImageSizeBytesCalculate(uint16_t textureWidth, uint16_t textureHeight, uint8_t textureChannels)
{
    assert(textureWidth > 0);
    assert(textureHeight > 0);
    assert(textureChannels > 0);
    return textureWidth * textureHeight * textureChannels;
}

uint32_t MipsLevelsCalculate(const uint32_t textureWidth, const uint32_t textureHeight)
{
    assert(textureWidth > 0);
    assert(textureHeight > 0);

    const uint32_t mipsLevels = 
        static_cast<uint32_t>(std::floor(std::log2(std::max<uint32_t>(textureWidth, textureHeight)))) + 1;//+1 to ensure the original image gets a mip level
    assert(mipsLevels >= 1);
    return mipsLevels;
}

const char* CookedFileDirectoryGet()
{
    static const char*const cookedFileDirectory = "cooked";
    return cookedFileDirectory;
}

/*static*/VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//not using instanced rendering, so index vertex attributes by vertex, not instance

    return bindingDescription;
}
