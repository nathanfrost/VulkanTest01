#include<assert.h>
///@todo: replace STL with more performant datastructures
#include<string>
#include<vector>
#include"MemoryUtil.h"
#include"ntf_vulkan.h"
#include"StreamingUnit.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include"tiny_obj_loader.h"

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

///@todo: invoke this function rather than manually calling Clear() on the stbAllocator
/** use when you're done with the data returned from stbi_load(); never call stbi_image_free() directly; only use this function to clear all stack
allocations stbi made using the (global) stbAllocatorPtr */
void STBIImageFree(void*const retval_from_stbi_load, StackCpu*const stbAllocatorPtr)
{
    assert(stbAllocatorPtr);
    auto& stbAllocator = *stbAllocatorPtr;

    stbi_image_free(retval_from_stbi_load);
    stbAllocator.Clear();
}
//END_#StbMemoryManagement
//END_STB_IMAGE

using namespace std;

typedef string StreamingUnitCookerString;
typedef float StreamingUnitCookerScalar;
struct StreamingUnitCookerTexturedGeometry
{
    StreamingUnitCookerString m_texturePathRelativeFromRoot;
    StreamingUnitCookerString m_modelPathRelativeFromRoot;
    StreamingUnitCookerScalar m_uniformScale;
};
typedef vector<StreamingUnitCookerTexturedGeometry> StreamingUnitCookerTexturedGeometries;
struct StreamingUnitCooker
{
    void FileNameOutputSet(const char*const filenameNoExtension);
    void TexturedGeometryAdd(
        const StreamingUnitCookerString& texturePath,
        const StreamingUnitCookerString& modelPath,
        const float uniformScale);
    void Cook();
    void Clear();


    StreamingUnitCookerString m_fileNameOutput;
    StreamingUnitCookerTexturedGeometries m_texturedGeometries;
};

void StreamingUnitCooker::FileNameOutputSet(const char*const filenameNoExtension)
{
    ArraySafe<char, 512> filenameExtension;
    filenameExtension.Snprintf("%s.%s", filenameNoExtension, StreamingUnitFilenameExtensionGet());
    m_fileNameOutput = StreamingUnitCookerString(filenameExtension.begin());
}

void StreamingUnitCooker::TexturedGeometryAdd(
    const StreamingUnitCookerString& texturePath,
    const StreamingUnitCookerString& modelPath,
    const float uniformScale)
{
    StreamingUnitCookerTexturedGeometry texturedGeometry;
    texturedGeometry.m_texturePathRelativeFromRoot = texturePath;
    texturedGeometry.m_modelPathRelativeFromRoot = modelPath;
    texturedGeometry.m_uniformScale = uniformScale;
    m_texturedGeometries.push_back(texturedGeometry);
}

void StreamingUnitCooker::Cook()
{
    const StreamingUnitVersion version = 0;
    const StreamingUnitCookerString rootDirectoryWithTrailingBackslash("..\\..\\VulkanTest01\\VulkanTest01\\");
    const StreamingUnitCookerString cookedFileDirectoryWithTrailingBackslash = 
        rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(CookedFileDirectoryGet()) + StreamingUnitCookerString("\\");
    FILE* f;
    StreamingUnitCookerString filePathOutput = cookedFileDirectoryWithTrailingBackslash + m_fileNameOutput;
    Fopen(&f, filePathOutput.c_str(), "wb");///<@todo: make directory if necessary

    //BEG_GENERALIZE_READER_WRITER
    Fwrite(f, &version, sizeof(version), 1);
    const StreamingUnitTexturedGeometryNum texturedGeometryNum = CastWithAssert<size_t, StreamingUnitTexturedGeometryNum>(m_texturedGeometries.size());
    Fwrite(f, &texturedGeometryNum, sizeof(texturedGeometryNum), 1);
    //END_GENERALIZE_READER_WRITER

    for (size_t texturedGeometryIndex = 0; texturedGeometryIndex < texturedGeometryNum; ++texturedGeometryIndex)
    {
        const StreamingUnitCookerTexturedGeometry& texturedGeometry = m_texturedGeometries[texturedGeometryIndex];

        //cook and write texture
        const StreamingUnitCookerString texturePath = rootDirectoryWithTrailingBackslash + StreamingUnitCookerString(texturedGeometry.m_texturePathRelativeFromRoot.c_str());
        int textureWidth, textureHeight, textureChannels;
        
        assert(g_stbAllocator->GetFirstByteFree() == 0);//ensure we can Clear() the whole stack correctly in STBIImageFree() (eg there's nothing already allocated in the stack)
        NTF_STATIC_ASSERT(sizeof(stbi_uc) == sizeof(StreamingUnitByte));
        StreamingUnitByte* pixels = stbi_load(texturePath.c_str(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
        assert(pixels);
        textureChannels = 4;///<this is true because we passed STBI_rgb_alpha; stbi_load() reports the number of textures actually present even as it respects this flag

        StreamingUnitTextureDimension textureWidthCook = CastWithAssert<int,StreamingUnitTextureDimension>(textureWidth);
        StreamingUnitTextureDimension textureHeightCook = CastWithAssert<int, StreamingUnitTextureDimension>(textureHeight);
        StreamingUnitTextureChannels textureChannelsCook = CastWithAssert<int, StreamingUnitTextureChannels>(textureChannels);

        TextureSerialize0<SerializerCookerOut>(f, &textureWidthCook, &textureHeightCook, &textureChannelsCook, pixels);
        g_stbAllocator->Clear();


        //cook and write vertex and index buffers
        std::vector<Vertex> vertices;
        std::vector<IndexBufferValue> indices;

        ///@todo: replace OBJ with binary FBX loading
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        const bool loadObjResult = tinyobj::LoadObj(
            &attrib, 
            &shapes, 
            &materials, 
            &err, 
            (rootDirectoryWithTrailingBackslash + texturedGeometry.m_modelPathRelativeFromRoot).c_str());
        assert(loadObjResult);

        ///@todo: #StreamingMemory: replace this STL with a good, static-memory hashmap
        //build index list and un-duplicate vertices
        std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex = {};

                vertex.pos =
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] //the origin of texture coordinates in Vulkan is the top-left corner, whereas the OBJ format assumes the bottom-left corner
                };

                vertex.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
        if (texturedGeometry.m_uniformScale != 1.f)
        {
            for (auto& vertex : vertices)
            {
                vertex.pos *= texturedGeometry.m_uniformScale;
            }
        }

        assert(vertices.size() > 0);
        assert(indices.size() > 0);
        StreamingUnitVerticesNum verticesNum = CastWithAssert<size_t, StreamingUnitVerticesNum>(vertices.size());
        ArraySafeRef<Vertex> verticesArraySafe(&vertices[0], verticesNum);
        StreamingUnitIndicesNum indicesNum = CastWithAssert<size_t, StreamingUnitIndicesNum>(indices.size());
        ArraySafeRef<IndexBufferValue> indicesArraySafe(&indices[0], indices.size());
        ModelSerialize<SerializerCookerOut>(
            f, 
            nullptr, 
            0, 
            &verticesNum, 
            verticesArraySafe, 
            ArraySafeRef<StreamingUnitByte>(), 
            nullptr,
            &indicesNum, 
            indicesArraySafe, 
            ArraySafeRef<StreamingUnitByte>(),
            nullptr);
    }
    Fclose(f);
}

void StreamingUnitCooker::Clear()
{
    m_fileNameOutput.clear();
    m_texturedGeometries.clear();
}
int main()
{
    STBAllocatorCreate(&g_stbAllocator);

    StreamingUnitCooker streamingUnitCooker;
    //,  /*"textures/HumanFighter_01_Diff.tga"*/ /*"textures/container_clean_diffuse01.jpeg"*//*"textures/appleD.jpg"*/,/*"textures/cat_diff.tga"*//*,"textures/chalet.jpg"*/
    //const char*const m_modelPath[TODO_REFACTOR_NUM] = { /*"models/Orange.obj"*/, /*"models/Container_OBJ.obj",*/ /*"models/apple textured obj.obj"*//*"models/cat.obj"*//*,"models/chalet.obj"*/ };//#StreamingMemory
    //const float m_uniformScale[TODO_REFACTOR_NUM] = { /*0.5f,*//*,.0025f*//*.01f,*/ /*1.f*/ };//#StreamingMemory

    streamingUnitCooker.FileNameOutputSet("unitTest0");

    streamingUnitCooker.TexturedGeometryAdd("textures/skull.jpg", "models/skull.obj", .05f);
    streamingUnitCooker.TexturedGeometryAdd("textures/Banana.jpg", "models/Banana.obj", .005f);

    streamingUnitCooker.Cook();

    STBAllocatorDestroy(&g_stbAllocator);
    return 0;//success
}
