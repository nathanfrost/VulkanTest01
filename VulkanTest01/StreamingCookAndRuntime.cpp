#include"StreamingCookAndRuntime.h"
#include<assert.h>
#include<functional>
#include <glm/gtx/hash.hpp>

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

size_t ImageSizeBytesCalculate(uint16_t textureWidth, uint16_t textureHeight, uint8_t textureChannels)
{
    assert(textureWidth > 0);
    assert(textureHeight > 0);
    assert(textureChannels > 0);
    return textureWidth * textureHeight * textureChannels;
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
