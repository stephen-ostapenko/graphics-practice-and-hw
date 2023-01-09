#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>

#include "common_util.hpp"

#include "entity.hpp"

namespace box {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const float A = 24.0;

const vec3 POSITIONS[4] = vec3[4](
    vec3(-A,  -A, 0.0),
    vec3( A,  -A, 0.0),
    vec3(-A, 0.0, 0.0),
    vec3( A, 0.0, 0.0)
);

const vec3 TANGENTS[4] = vec3[4](
    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0)
);

const vec3 NORMALS[4] = vec3[4](
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0)
);

const vec2 TEXCOORDS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

out vec3 position;
out vec3 tangent;
out vec3 normal;
out vec2 texcoord;

void main() {
    position = (model * vec4(POSITIONS[gl_VertexID], 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    tangent = mat3(model) * TANGENTS[gl_VertexID];
    normal = mat3(model) * NORMALS[gl_VertexID];
    texcoord = TEXCOORDS[gl_VertexID];
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo_texture;
uniform sampler2D normal_texture;
uniform sampler2D environment_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;
uniform vec3 camera_position;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

void main() {
    vec3 albedo = texture(albedo_texture, texcoord).rgb;

    vec3 camera_direction = normalize(camera_position - position);

    vec3 bitangent = cross(tangent, normal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 real_normal = tbn * (texture(normal_texture, texcoord).rgb * 2.0 - vec3(1.0));

    vec3 dir = 2 * real_normal * dot(real_normal, camera_direction) - camera_direction;
    float x = atan(dir.z, dir.x) / PI * 0.5 + 0.5;
    float y = -atan(dir.y, length(dir.xz)) / PI + 0.5;
    vec3 env_albedo = texture(environment_texture, vec2(x, y)).rgb;

    float lightness = max(0.0, dot(normalize(real_normal), light_direction));
    albedo = lightness * albedo + ambient_light_color;

    out_color = vec4((albedo + env_albedo) / 2.0, 1.0);
}
)";

struct box_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;
    
    GLuint albedo_texture_location, normal_texture_location, environment_texture_location;
    GLuint camera_position_location;

    GLuint albedo_texture, normal_texture, environment_texture;

    const float scale = 1.f;
    const float board_size = 24.f;

    box_t(int object_index) {
        (void)object_index;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        albedo_texture_location = glGetUniformLocation(program, "albedo_texture");
        normal_texture_location = glGetUniformLocation(program, "normal_texture");
        environment_texture_location = glGetUniformLocation(program, "environment_texture");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");
        camera_position_location = glGetUniformLocation(program, "camera_position");

        glGenVertexArrays(1, &vao);

        std::string project_root = PROJECT_ROOT;
        albedo_texture = load_texture(project_root + "/models/box/box_albedo.jpg");
        normal_texture = load_texture(project_root + "/models/box/box_normal.jpg");
        environment_texture = load_texture(project_root + "/models/box/environment.jpg");
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)time;

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(camera_position_location, 1, reinterpret_cast<const float*>(&camera_position));
        glUniform1i(albedo_texture_location, 0);
        glUniform1i(normal_texture_location, 1);
        glUniform1i(environment_texture_location, 2);

        for (int i = 0; i < 4; i++) {
            glm::mat4 model = glm::mat4(1.f);
            model = glm::rotate(model, glm::pi<float>() / 2.f * i, {0.f, 1.f, 0.f});
            model = glm::translate(model, glm::vec3(0.f, 0.f, board_size));
            model = glm::scale(model, glm::vec3(scale));

            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, albedo_texture);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, normal_texture);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, environment_texture);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }
};

}