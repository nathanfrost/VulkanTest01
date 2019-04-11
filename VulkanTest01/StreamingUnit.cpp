#include"StreamingUnit.h"

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
