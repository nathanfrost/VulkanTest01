#include"StreamingUnit.h"

size_t std::hash<Vertex>::operator()(Vertex const& vertex) const
{
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
}

/*static*/VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//not using instanced rendering, so index vertex attributes by vertex, not instance

    return bindingDescription;
}

/*static*/ void Vertex::GetAttributeDescriptions(VectorSafeRef<VkVertexInputAttributeDescription> attributeDescriptions)
{
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;///<mirrored in the vertex shader
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;//equivalent to vec3 layout
    attributeDescriptions[0].offset = offsetof(Vertex, pos);//defines address of first byte of the relevant datafield

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
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
