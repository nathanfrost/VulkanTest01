#pragma once
#include<assert.h>
#include<stdint.h>

const char* StreamingUnitFilenameExtensionGet();
void StreamingUnitFilenameExtensionAppend(char*const filenameNoExtension, const size_t filenameNoExtensionSizeBytes);
size_t ImageSizeBytesCalculate(uint16_t textureWidth, uint16_t textureHeight, uint8_t textureChannels);
const char* CookedFileDirectoryGet();

typedef uint32_t StreamingUnitVersion;
typedef uint8_t StreamingUnitByte;
typedef uint32_t StreamingUnitTexturesNum;
typedef uint16_t StreamingUnitTextureDimension;
typedef uint8_t StreamingUnitTextureChannels;