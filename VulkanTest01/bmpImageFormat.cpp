#include"bmpImageFormat.h"
#include"StreamingCookAndRuntime.h"

static unsigned char s_bmpHeader[] = // All values are little-endian
{
    0x42, 0x4D,             // Signature 'BM'
    0xaa, 0x00, 0x00, 0x00, // Size: 170 bytes
    0x00, 0x00,             // Unused
    0x00, 0x00,             // Unused
    0x8a, 0x00, 0x00, 0x00, // Offset to image data

    0x7c, 0x00, 0x00, 0x00, // DIB header size (124 bytes)
    0x04, 0x00, 0x00, 0x00, // Width (4px)
    0x02, 0x00, 0x00, 0x00, // Height (2px)
    0x01, 0x00,             // Planes (1)
    0x20, 0x00,             // Bits per pixel (32)
    0x03, 0x00, 0x00, 0x00, // Format (bitfield = use bitfields | no compression)
    0x20, 0x00, 0x00, 0x00, // Image raw size (32 bytes)
    0x13, 0x0B, 0x00, 0x00, // Horizontal print resolution (2835 = 72dpi * 39.3701)
    0x13, 0x0B, 0x00, 0x00, // Vertical print resolution (2835 = 72dpi * 39.3701)
    0x00, 0x00, 0x00, 0x00, // Colors in palette (none)
    0x00, 0x00, 0x00, 0x00, // Important colors (0 = all)
    0x00, 0x00, 0xFF, 0x00, // R bitmask (00FF0000)
    0x00, 0xFF, 0x00, 0x00, // G bitmask (0000FF00)
    0xFF, 0x00, 0x00, 0x00, // B bitmask (000000FF)
    0x00, 0x00, 0x00, 0xFF, // A bitmask (FF000000)
    0x42, 0x47, 0x52, 0x73, // sRGB color space
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Unused R, G, B entries for color space
    0x00, 0x00, 0x00, 0x00, // Unused Gamma X entry for color space
    0x00, 0x00, 0x00, 0x00, // Unused Gamma Y entry for color space
    0x00, 0x00, 0x00, 0x00, // Unused Gamma Z entry for color space

    0x00, 0x00, 0x00, 0x00, // Unknown
    0x00, 0x00, 0x00, 0x00, // Unknown
    0x00, 0x00, 0x00, 0x00, // Unknown
    0x00, 0x00, 0x00, 0x00, // Unknown

    //// Image data comes next -- something like:
    //0xFF, 0x00, 0x00, 0x7F, // Bottom left pixel
    //0x00, 0xFF, 0x00, 0x7F,
    //0x00, 0x00, 0xFF, 0x7F,
    //0xFF, 0xFF, 0xFF, 0x7F, // Bottom right pixel
    //0xFF, 0x00, 0x00, 0xFF, // Top left pixel
    //0x00, 0xFF, 0x00, 0xFF,
    //0x00, 0x00, 0xFF, 0xFF,
    //0xFF, 0xFF, 0xFF, 0xFF  // Top right pixel
};

void WriteR8G8B8A8ToBmpFile(
    const ConstArraySafeRef<uint8_t>& vulkanFormatR8G8B8A8,
    const size_t textureWidth,
    const size_t textureHeight,
    const size_t textureRowPitch,
    const size_t textureSizeBytes,
    const ConstArraySafeRef<char>& bmpFilePathFull)
{
    const size_t bytesInPixel = 4;
    const size_t textureNumBytesInRowNoPadding = textureWidth*bytesInPixel;
    assert(textureWidth > 0);
    assert(textureHeight > 0);
    assert(textureRowPitch >= textureNumBytesInRowNoPadding);
    assert(textureSizeBytes > 0);
    assert(textureSizeBytes >= textureRowPitch*textureHeight);
    assert(textureSizeBytes%textureRowPitch == 0);
    assert(textureSizeBytes / textureRowPitch == textureHeight);

    FILE* f;
    fopen_s(&f, bmpFilePathFull.begin(), "wb");

    const size_t sizeofBmpHeader = sizeof(s_bmpHeader);
    const size_t imageSizeBytes = 
        ImageSizeBytesCalculate(CastWithAssert<size_t,uint16_t>(textureWidth), CastWithAssert<size_t, uint16_t>(textureHeight), bytesInPixel);
    *reinterpret_cast<uint32_t*>(&s_bmpHeader[2]) = CastWithAssert<size_t,uint32_t>(imageSizeBytes + sizeofBmpHeader);
    *reinterpret_cast<uint32_t*>(&s_bmpHeader[18]) = CastWithAssert<size_t, uint32_t>(textureWidth);
    *reinterpret_cast<uint32_t*>(&s_bmpHeader[22]) = CastWithAssert<size_t, uint32_t>(textureHeight);
    fwrite(s_bmpHeader, 1, sizeofBmpHeader, f);

    for (size_t rowIndex = 0; rowIndex < textureHeight; ++rowIndex)
    {
        const size_t rowByteIndex = rowIndex*textureRowPitch;
        ConstArraySafeRef<const uint8_t> row(&vulkanFormatR8G8B8A8[rowByteIndex], textureNumBytesInRowNoPadding);
#if NTF_DEBUG
        vulkanFormatR8G8B8A8[rowByteIndex + textureNumBytesInRowNoPadding - 1];//no overrunning the array///TODO_NEXT: factor into another constructor
#endif//#if NTF_DEBUG
        for (size_t pixelIndex = 0; pixelIndex < textureWidth; ++pixelIndex)
        {
            ConstArraySafeRef<const uint8_t> pixel(&row[pixelIndex*bytesInPixel], bytesInPixel);
#if NTF_DEBUG
            vulkanFormatR8G8B8A8[rowByteIndex + bytesInPixel - 1];//no overrunning the array
#endif//#if NTF_DEBUG
            //endian-swap RGB channels
            fwrite(&pixel[2], 1, 1, f);
            fwrite(&pixel[1], 1, 1, f);
            fwrite(&pixel[0], 1, 1, f);
            fwrite(&pixel[3], 1, 1, f);//alpha channel maintains its position, for reasons unknown
        }
    }
    fclose(f);
}
