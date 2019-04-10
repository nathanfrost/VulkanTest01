#include<assert.h>
#include<string>
#include<vector>
#include"MemoryUtil.h"
#include"ntf_vulkan.h"
#include"StreamingUnit.h"

//BEG_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
//BEG_#StbMemoryManagement
StackCpu* g_stbAllocator;
void* __cdecl stb_malloc(size_t _Size)
{
    assert(g_stbAllocator);

    void* memory;
    g_stbAllocator->PushAlloc(&memory, 0, _Size);
    assert(memory);
    return memory;
}
void* __cdecl stb_assertRealloc(void*  _Block, size_t _Size) { assert(false); return nullptr; }
void __cdecl stb_nullFree(void* const block) {}
#include"stb_image.h"

//BEG_#StbMemoryManagement
void STBAllocatorCreate(StackCpu**const stbAllocatorPtrPtr)
{
    NTF_REF(stbAllocatorPtrPtr, stbAllocatorPtr);

    stbAllocatorPtr = new StackCpu();
    const size_t sizeBytes = 128 * 1024 * 1024;
    stbAllocatorPtr->Initialize(reinterpret_cast<uint8_t*>(malloc(sizeBytes)), sizeBytes);
}
void STBAllocatorDestroy(StackCpu**const stbAllocatorPtrPtr)
{
    NTF_REF(stbAllocatorPtrPtr, stbAllocatorPtr);

    free(stbAllocatorPtr->GetMemory());
    stbAllocatorPtr->Destroy();
    delete stbAllocatorPtr;
    stbAllocatorPtr = nullptr;
}
//END_#StbMemoryManagement
//END_STB_IMAGE

using namespace std;

typedef string StreamingUnitCookerString;
typedef vector<StreamingUnitCookerString> StreamingUnitCookerPaths;
typedef vector<float> StreamingUnitCookerScalars;
struct StreamingUnitCooker
{
    void FileNameOutputSet(const char*const filenameNoExtension);
    void TexturedModelAdd(
        const StreamingUnitCookerString& texturePath,
        const StreamingUnitCookerString& modelPath,
        const float uniformScale);
    void Cook();
    void Clear();


    StreamingUnitCookerString m_fileNameOutput;
    StreamingUnitCookerPaths m_texturePaths;
    StreamingUnitCookerPaths m_modelPaths;
    StreamingUnitCookerScalars m_uniformScales;
};

void StreamingUnitCooker::FileNameOutputSet(const char*const filenameNoExtension)
{
    ArraySafe<char, 512> filenameExtension;
    filenameExtension.Snprintf("%s.%s", filenameNoExtension, StreamingUnitFilenameExtensionGet());
    m_fileNameOutput = StreamingUnitCookerString(filenameExtension.begin());
}

void StreamingUnitCooker::TexturedModelAdd(
    const StreamingUnitCookerString& texturePath,
    const StreamingUnitCookerString& modelPath,
    const float uniformScale)
{
    m_texturePaths.push_back(texturePath);
    m_modelPaths.push_back(modelPath);
    m_uniformScales.push_back(uniformScale);
}

void StreamingUnitCooker::Cook()
{
    const StreamingUnitVersion version = 0;
    const StreamingUnitCookerString rootDirectoryWithTrailingBackslash("..\\..\\VulkanTest01\\VulkanTest01\\");
    const StreamingUnitCookerString cookedFileDirectoryWithTrailingBackslash = 
        rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(CookedFileDirectoryGet()) + StreamingUnitCookerString("\\");
    FILE* f;
    StreamingUnitCookerString filePathOutput = cookedFileDirectoryWithTrailingBackslash + m_fileNameOutput;
    Fopen(&f, filePathOutput.c_str(), "wb");

    //BEG_GENERALIZE_READER_WRITER
    Fwrite(f, &version, sizeof(version), 1);
    const uint32_t texturesNum = Cast_size_t_uint32_t(m_texturePaths.size());
    Fwrite(f, &texturesNum, sizeof(texturesNum), 1);
    //END_GENERALIZE_READER_WRITER

    for (size_t textureIndex = 0; textureIndex < texturesNum; ++textureIndex)
    {
        const char*const texturePathRelativeFromRoot = m_texturePaths[textureIndex].c_str();
        const StreamingUnitCookerString texturePath = rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(texturePathRelativeFromRoot);
        int textureWidth, textureHeight, textureChannels;
        
        assert(g_stbAllocator->GetFirstByteFree() == 0);//ensure we can Clear() the whole stack correctly in STBIImageFree() (eg there's nothing already allocated in the stack)
        stbi_uc* pixels = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
        NTF_STATIC_ASSERT(sizeof(stbi_uc) == sizeof(StreamingUnitByte));
         assert(pixels);
        textureChannels = 4;///<this is true because we passed STBI_rgb_alpha; stbi_load() reports the number of textures actually present even as it respects this flag

        const StreamingUnitTextureDimension textureWidthCook = CastWithAssert<int,StreamingUnitTextureDimension>(textureWidth);
        const StreamingUnitTextureDimension textureHeightCook = CastWithAssert<int, StreamingUnitTextureDimension>(textureHeight);
        const StreamingUnitTextureChannels textureChannelsCook = CastWithAssert<int, StreamingUnitTextureChannels>(textureChannels);

        //BEG_GENERALIZE_READER_WRITER
        Fwrite(f, &textureWidthCook, sizeof(textureWidthCook), 1);
        Fwrite(f, &textureHeightCook, sizeof(textureHeightCook), 1);
        Fwrite(f, &textureChannelsCook, sizeof(textureChannelsCook), 1);
        Fwrite(f, pixels, sizeof(*pixels), ImageSizeBytesCalculate(textureWidthCook, textureHeightCook, textureChannelsCook));
        //END_GENERALIZE_READER_WRITER

        g_stbAllocator->Clear();
    }

    const uint32_t modelPathsNum = Cast_size_t_uint32_t(m_modelPaths.size());
    Fwrite(f, &modelPathsNum, sizeof(modelPathsNum), 1);
    //todo

    const uint32_t uniformScalesNum  = Cast_size_t_uint32_t(m_uniformScales.size());
    Fwrite(f, &uniformScalesNum, sizeof(uniformScalesNum), 1);
    //todo

    //todo: uniform buffer

    Fclose(f);
}

void StreamingUnitCooker::Clear()
{
    m_fileNameOutput.clear();
    m_texturePaths.clear();
    m_modelPaths.clear();
    m_uniformScales.clear();
}
int main()
{
    STBAllocatorCreate(&g_stbAllocator);

    StreamingUnitCooker streamingUnitCooker;
    //,  /*"textures/HumanFighter_01_Diff.tga"*/ /*"textures/container_clean_diffuse01.jpeg"*//*"textures/appleD.jpg"*/,/*"textures/cat_diff.tga"*//*,"textures/chalet.jpg"*/
    //streamingUnitCooker.TexturedModelAdd();///<@todo: once they're all working, use the convenience API
    streamingUnitCooker.FileNameOutputSet("unitTest0");
    streamingUnitCooker.m_texturePaths.push_back(StreamingUnitCookerString("textures/skull.jpg"));
    streamingUnitCooker.m_texturePaths.push_back(StreamingUnitCookerString("textures/Banana.jpg"));

    streamingUnitCooker.Cook();

    STBAllocatorDestroy(&g_stbAllocator);
    return 0;//success
}
