#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "../VulkanShaderHeaders/ShaderBuffers.glsl"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in flat uint materialTag;
layout(location = 3) in vec3 modelPos;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout (location = 0) out vec4 finalColor;

void main()
{
    // Get the material from the material buffer based on the material tag that was passed from the vertex shader
    Material currentMaterial = bufferAddrs.materialBuffer.materials[materialTag];

    // Diffuse lighting calculations
    float diffuseFactor = max(dot(normal, -shaderData.sunDir), 0.0);
    vec4 diffuseSampler = texture(textures[currentMaterial.diffuseMapIndex], uv);
    vec4 diffuse = vec4(vec3(shaderData.sunColor * diffuseFactor), diffuseSampler.a);
    diffuse *= diffuseSampler;

    // Specular light calculations
    vec3 viewDirection = normalize(shaderData.viewPosition - modelPos);
    vec3 halfDirection = normalize(viewDirection - shaderData.sunDir);
    float specularFactor = pow(max(dot(halfDirection, normal), 0), currentMaterial.shininess);
    vec4 specularSampler = texture(textures[currentMaterial.specularMapIndex], uv);
    vec4 specular = vec4(vec3(shaderData.sunColor * specularFactor), specularSampler.a);
    specular *= specularSampler;

    // Not optimal but I am going to keep this for the moment
    if(materialTag != 0)
        finalColor = diffuseSampler;
    else
        finalColor = vec4(normal, 1);
}