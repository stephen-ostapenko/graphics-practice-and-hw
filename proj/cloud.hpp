#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>
#include <fstream>

#include "common_util.hpp"

#include "entity.hpp"

namespace cloud {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 view;
uniform mat4 projection;

uniform vec3 bbox_min;
uniform vec3 bbox_max;

layout (location = 0) in vec3 in_position;

out vec3 position;

void main()
{
    position = bbox_min + in_position * (bbox_max - bbox_min);
    gl_Position = projection * view * vec4(position, 1.0);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 bbox_min;
uniform vec3 bbox_max;
uniform sampler3D tex;

layout (location = 0) out vec4 out_color;

in vec3 position;

void sort(inout float x, inout float y)
{
    if (x > y)
    {
        float t = x;
        x = y;
        y = t;
    }
}

float vmin(vec3 v)
{
    return min(v.x, min(v.y, v.z));
}

float vmax(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

vec2 intersect_bbox(vec3 origin, vec3 direction)
{
    vec3 tmin = (bbox_min - origin) / direction;
    vec3 tmax = (bbox_max - origin) / direction;

    sort(tmin.x, tmax.x);
    sort(tmin.y, tmax.y);
    sort(tmin.z, tmax.z);

    return vec2(vmax(tmin), vmin(tmax));
}

const float PI = 3.1415926535;

vec3 to_tex(vec3 p) {
    return (p - bbox_min) / (bbox_max - bbox_min);
}

void main()
{
    vec3 dir = normalize(position - camera_position);
    vec2 bounds = intersect_bbox(camera_position, dir);
    float tmin = max(0.0, bounds.x), tmax = bounds.y;
    float dt = (tmax - tmin) / 16;

    float absorption = 0.8;
    float scattering = 4.0;
    float extinction = absorption + scattering;
    
    vec3 actual_light_color = light_color * 24.0;
    vec3 color = vec3(0.0);
    float optical_depth = 0.0;

    for (int i = 0; i < 16; i++) {
        float t = tmin + (i + 0.5) * dt;
        vec3 p = camera_position + t * dir;
        float density = texture(tex, to_tex(p)).x;
        optical_depth += extinction * density * dt;

        vec2 l_bounds = intersect_bbox(p, light_direction);
        float l_tmin = max(0.0, l_bounds.x), l_tmax = l_bounds.y;
        float l_dt = (l_tmax - l_tmin) / 8;
        
        float light_optical_depth = 0.0;
        for (int j = 0; j < 8; j++) {
            float l_t = l_tmin + (j + 0.5) * l_dt;
            vec3 l_p = p + l_t * light_direction;
            float l_density = texture(tex, to_tex(l_p)).x;
            light_optical_depth += extinction * l_density * l_dt;
        }

        color += actual_light_color * exp(-light_optical_depth) * exp(-optical_depth) \
            * dt * density * scattering / 4.0 / PI;
    }
    
    float opacity = 1.0 - exp(-optical_depth);
    out_color = vec4(color, opacity);
}
)";

static glm::vec3 cube_vertices[] {
    {0.f, 0.f, 0.f},
    {1.f, 0.f, 0.f},
    {0.f, 1.f, 0.f},
    {1.f, 1.f, 0.f},
    {0.f, 0.f, 1.f},
    {1.f, 0.f, 1.f},
    {0.f, 1.f, 1.f},
    {1.f, 1.f, 1.f},
};

static std::uint32_t cube_indices[] {
    // -Z
    0, 2, 1,
    1, 2, 3,
    // +Z
    4, 5, 6,
    6, 5, 7,
    // -Y
    0, 1, 4,
    4, 1, 5,
    // +Y
    2, 6, 3,
    3, 6, 7,
    // -X
    0, 4, 2,
    2, 4, 6,
    // +X
    1, 3, 5,
    5, 3, 7,
};

struct cloud_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    const float scale = 1.f;
    const glm::vec3 cloud_bbox_min{-9.f,  7.f, -6.f};
    const glm::vec3 cloud_bbox_max{-1.f, 10.f, -2.f};

    GLuint bbox_min_location, bbox_max_location;
    GLuint camera_position_location;

    cloud_t(int object_index) {
        (void)object_index;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        bbox_min_location = glGetUniformLocation(program, "bbox_min");
        bbox_max_location = glGetUniformLocation(program, "bbox_max");
        camera_position_location = glGetUniformLocation(program, "camera_position");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        const std::string project_root = PROJECT_ROOT;
        const std::string cloud_data_path = project_root + "/models/cloud/cloud.data";

        std::vector <char> pixels(128 * 64 * 64);
        std::ifstream input(cloud_data_path, std::ios::binary);
        input.read(pixels.data(), pixels.size());

        GLuint tex;
        glGenTextures(1, &tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, 128, 64, 64, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)ambient_light_color;

        glm::vec3 translation = glm::vec3(sin(time * .3f + glm::pi<float>()) * 2.f, 0.f, cos(time * .1f + glm::pi<float>()) * 3.f) * 5.f;
        glm::vec3 cur_cloud_bbox_min = cloud_bbox_min + translation;
        glm::vec3 cur_cloud_bbox_max = cloud_bbox_max + translation;

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(bbox_min_location, 1, reinterpret_cast<const float*>(&cur_cloud_bbox_min));
        glUniform3fv(bbox_max_location, 1, reinterpret_cast<const float*>(&cur_cloud_bbox_max));
        glUniform3fv(camera_position_location, 1, reinterpret_cast<const float*>(&camera_position));
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, std::size(cube_indices), GL_UNSIGNED_INT, nullptr);
    }
};

}