#pragma once
#include<assert.h>
#include<stdint.h>
#include"stdArrayUtility.h"

typedef uint32_t StreamingUnitVersion;
typedef uint8_t StreamingUnitByte;
typedef uint32_t StreamingUnitTexturesNum;
typedef uint16_t StreamingUnitTextureDimension;
typedef uint8_t StreamingUnitTextureChannels;

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
