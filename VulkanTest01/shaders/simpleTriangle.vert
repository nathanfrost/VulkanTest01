#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject 
{
    mat4 modelToClip[4];//#StreamingMemoryBasicModel -- really only need one for the one single triangle, but this uniform buffer has space for more
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragColor;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = ubo.modelToClip[0] * vec4(inPosition, 1.0);
    fragColor = vec3(1.0, 0.0, 0.0);
}