#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>

#include "common_util.hpp"

#include "entity.hpp"

namespace board {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const float A = 24.0;

const vec3 POSITIONS[4] = vec3[4](
    vec3(-A, 0.0, -A),
    vec3( A, 0.0, -A),
    vec3(-A, 0.0,  A),
    vec3( A, 0.0,  A)
);

const vec3 NORMALS[4] = vec3[4](
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0)
);

const vec2 TEXCOORDS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

out vec3 position;
out vec3 normal;
out vec2 texcoord;

out float board_size;

void main() {
    position = (model * vec4(POSITIONS[gl_VertexID], 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * NORMALS[gl_VertexID];
    texcoord = TEXCOORDS[gl_VertexID];
    board_size = A;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

in float board_size;

layout (location = 0) out vec4 out_color;

const float C = 0.5;

void main() {
    float diffuse = max(0.0, dot(normalize(normal), light_direction));
    
    vec3 albedo = texture(albedo_texture, texcoord).rgb;
    if (albedo.r < 0.5) {
        float f = pow(min(1.0, length(position) / board_size), 2) / 2;
        albedo = vec3(0.5 + f, 1.0 - f, 1.0);
    }
    
    out_color = vec4(albedo * (light_color * diffuse + ambient_light_color), 1.0);
}
)";

struct board_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;
    
    GLuint texture_location;

    GLuint texture;

    const float scale = 1.f;
    const int size = 64;

    board_t(int object_index) {
        (void)object_index;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        texture_location = glGetUniformLocation(program, "albedo_texture");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");

        glGenVertexArrays(1, &vao);

        std::vector<std::uint32_t> pixels(size * size);
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                std::uint32_t colors[] = {0x000000u, 0xFFFFFFFFu};
                pixels[i * size + j] = colors[(i + j) % 2];
            }
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)camera_position; (void)time;

        glm::mat4 model = glm::mat4(1.f);
        model = glm::scale(model, glm::vec3(scale));

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));
        glUniform1i(texture_location, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

}