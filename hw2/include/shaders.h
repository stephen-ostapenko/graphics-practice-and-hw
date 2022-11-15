#pragma once

#include <GL/glew.h>

GLuint create_shader(GLenum type, const char *source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }

    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

const char vertex_shader_source[] = R"(
#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

out vec3 position;
out vec3 normal;
out vec2 texcoord;

void main() {
    position = (model * vec4(in_position, 1.0)).xyz;
    normal = normalize(mat3(model) * in_normal);
    texcoord = in_texcoord;

    gl_Position = projection * view * vec4(position, 1.0);
}
)";

const char fragment_shader_source[] = R"(
#version 330 core

uniform sampler2D albedo_texture;
uniform sampler2D opacity_texture;

uniform vec3 camera_position;
uniform vec3 ambient_light;

uniform vec3 sun_direction;
uniform vec3 sun_color;

uniform vec3 point_light_position;
uniform vec3 point_light_color;
uniform vec3 point_light_attenuation;

uniform vec3 glossiness;
uniform float power;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction, vec3 albedo) {
    return albedo * max(0.0, dot(normal, direction));
}

vec3 specular(vec3 direction, vec3 albedo) {
    vec3 reflected = 2 * normal * dot(normal, direction) - direction;
    vec3 view_direction = normalize(camera_position - position);
    return glossiness * albedo * pow(max(0.01, dot(reflected, view_direction)), power);
}

void main() {
    vec3 albedo = vec3(texture(albedo_texture, texcoord));

    vec3 ambient = albedo * ambient_light;

    vec3 sun_light = (diffuse(sun_direction, albedo) + specular(sun_direction, albedo)) * sun_color;

    vec3 direct_vec = point_light_position - position;
    float r = length(direct_vec);
    direct_vec = normalize(direct_vec);
    vec3 point_light = (diffuse(direct_vec, albedo) + specular(direct_vec, albedo)) \
                          * point_light_color \
                          / (point_light_attenuation[0] + point_light_attenuation[1] * r + point_light_attenuation[2] * r * r);

    vec3 color = ambient + sun_light + point_light;
    
    out_color = vec4(color, 1.0);

    vec3 opac = vec3(texture(opacity_texture, texcoord));
    float opacity_level = (opac.r * opac.r + opac.g * opac.g + opac.b * opac.b) / 3;
    if (opacity_level < 0.001) {
        discard;
    }
}
)";