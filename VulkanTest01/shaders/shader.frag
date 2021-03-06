#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler texSampler;
layout(binding = 2) uniform texture2D textures[2];

layout (push_constant) uniform PushConstants
{
    uint objectIndex;
} pushConstants;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() 
{
    //outColor = vec4(fragTexCoord, 0.0, 0.0);
    //texture indexing hack: assume first two objects are drawn with zeroth texture and second two objects are drawn with first texture
    outColor = texture(sampler2D(textures[pushConstants.objectIndex >> 1], texSampler), fragTexCoord);
}