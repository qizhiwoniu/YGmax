#pragma once

// ═══════════════════════════════════════════════════════════════
//  SceneShaders.h
//  Viewport3D 和 SceneRunnerWidget 共用的 GLSL shader 字符串
//  用 inline 避免多编译单元重复定义
// ═══════════════════════════════════════════════════════════════

inline const char* kMeshVert = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vNormal;
out vec3 vFragPos;

void main()
{
    vFragPos    = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = normalize(uNormalMat * aNormal);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

inline const char* kMeshFrag = R"GLSL(
#version 330 core
in  vec3 vNormal;
in  vec3 vFragPos;
out vec4 fragColor;

uniform vec3 uColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform bool uWireframe;
uniform bool uSelected;

void main()
{
    if (uWireframe) {
        vec3 c = uSelected ? vec3(1.0,0.65,0.0) : vec3(0.55,0.75,0.95);
        fragColor = vec4(c, 1.0);
        return;
    }

    vec3 ambient  = 0.20 * uColor;
    float diff    = max(dot(vNormal, uLightDir), 0.0);
    vec3 diffuse  = diff * uColor;

    vec3 viewDir  = normalize(uViewPos - vFragPos);
    vec3 halfDir  = normalize(uLightDir + viewDir);
    float spec    = pow(max(dot(vNormal, halfDir), 0.0), 48.0);
    vec3 specular = spec * vec3(0.30);

    float hemi = 0.5 + 0.5 * vNormal.y;
    vec3  sky  = mix(vec3(0.07,0.05,0.04), vec3(0.16,0.20,0.28), hemi) * 0.35;

    vec3 result = ambient + diffuse + specular + sky;

    if (uSelected) {
        float rim  = 1.0 - max(dot(viewDir, vNormal), 0.0);
        result    += pow(rim, 2.8) * vec3(1.0, 0.55, 0.05) * 0.9;
    }

    fragColor = vec4(result, 1.0);
}
)GLSL";
