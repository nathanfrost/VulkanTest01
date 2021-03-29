#pragma once

#include "stdArrayUtility.h"

void WriteR8G8B8A8ToBmpFile(
    const ConstArraySafeRef<uint8_t>& vulkanFormatR8G8B8A8,
    const size_t textureWidth,
    const size_t textureHeight,
    const size_t textureRowPitch,
    const size_t textureSizeBytes,
    const ConstArraySafeRef<char>& bmpFilePathFull);