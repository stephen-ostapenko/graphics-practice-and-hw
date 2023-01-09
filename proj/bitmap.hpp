#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>
#include <cstring>

#include "common_util.hpp"

#include "entity.hpp"

namespace bitmap {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const float A = 24.0;

const vec3 POSITIONS[4] = vec3[4](
    vec3(-A, -A, -A),
    vec3( A, -A, -A),
    vec3(-A, -A,  A),
    vec3( A, -A,  A)
);

const vec3 NORMALS[4] = vec3[4](
    vec3(0.0, -1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(0.0, -1.0, 0.0)
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

void main() {
    position = (model * vec4(POSITIONS[gl_VertexID], 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * NORMALS[gl_VertexID];
    texcoord = TEXCOORDS[gl_VertexID] * 8;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2DArray albedo_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;

uniform float time;
uniform int frames_cnt;
uniform float fps;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main() {
    float diffuse = max(0.0, dot(normalize(normal), light_direction));
    
    vec3 albedo = texture(albedo_texture, vec3(texcoord, mod(time * fps, frames_cnt) - 0.5)).rgb;
    vec3 color_correction = vec3(0.4);
    
    out_color = vec4(albedo * (light_color * diffuse + ambient_light_color + color_correction), 1.0);
}
)";

struct bitmap_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;
    
    GLuint texture_location;
    GLuint time_location, frames_cnt_location, fps_location;

    GLuint texture;

    const float scale = 1.f;
    const float correction_angle = -glm::pi<float>() / 2;
    
    const int frames_cnt = 648;
    const int fps = 24;
    const int bitmap_width = 480;
    const int bitmap_height = 440;

    bitmap_t(int object_index) {
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
        time_location = glGetUniformLocation(program, "time");
        frames_cnt_location = glGetUniformLocation(program, "frames_cnt");
        fps_location = glGetUniformLocation(program, "fps");

        glGenVertexArrays(1, &vao);

        std::string project_root = PROJECT_ROOT;
        std::string frames_path = project_root + "/models/bitmap/frames/";

        unsigned char *pixels = new unsigned char[frames_cnt * bitmap_width * bitmap_height * 4];
        assert(pixels);

        for (int i = 0; i < frames_cnt; i++) {
            std::string cur_frame_num = std::to_string(i + 1);
            cur_frame_num = std::string(3 - cur_frame_num.size(), '0') + cur_frame_num;

            std::string cur_frame_path = frames_path + cur_frame_num + ".jpg";

            int width, height, channels;
            unsigned char *frame = stbi_load(cur_frame_path.c_str(), &width, &height, &channels, 4);
            assert(frame);
            assert(width == bitmap_width && height == bitmap_height);

            std::memcpy(pixels + i * width * height * 4, frame, width * height * 4);

            stbi_image_free(frame);
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, bitmap_width, bitmap_height, frames_cnt, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        delete[] pixels;
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)camera_position;

        glm::mat4 model = glm::mat4(1.f);
        model = glm::rotate(model, correction_angle, {0.f, 1.f, 0.f});
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
        glUniform1f(time_location, time);
        glUniform1i(frames_cnt_location, frames_cnt);
        glUniform1f(fps_location, fps);
        glUniform1i(texture_location, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

}