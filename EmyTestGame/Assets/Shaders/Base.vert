#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	vec3 viewPos;
	vec3 lightDir;
	vec2 lightSetting;
	vec3 lightColor;
} ubo;

// In
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;

//Out
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragVertexColor;
layout(location = 2) out vec2 fragTexCoord;

layout(location = 3) out mat4 modelMatrix;
layout(location = 7) out mat4 viewMatrix;
layout(location = 11) out vec3 viewPosition;
layout(location = 12) out mat3 TBN; // tangent bi normal
layout(location = 15) out vec3 lightDir;
layout(location = 16) out vec3 fragPos;
layout(location = 17) out vec2 lightSetting;
layout(location = 18) out vec3 lightColor;


void main()
{
	vec4 vertexPosition = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    gl_Position = vertexPosition;
	fragPos =  (ubo.model * vec4(inPosition, 1.0)).xyz;

    fragVertexColor = inColor;
	fragTexCoord = inTexCoord;
	fragNormal = inNormal;
	modelMatrix = ubo.model;
	viewMatrix = ubo.view;
	viewPosition = ubo.viewPos;
	lightDir = ubo.lightDir;
	lightSetting = ubo.lightSetting;
	lightColor = ubo.lightColor;

	vec3 T = normalize(vec3(ubo.model * vec4(inTangent,   0.0)));
    vec3 B = normalize(vec3(ubo.model * vec4(inBiTangent, 0.0)));
    vec3 N = normalize(vec3(ubo.model * vec4(inNormal,    0.0)));
	TBN = mat3(T, B, N);
}