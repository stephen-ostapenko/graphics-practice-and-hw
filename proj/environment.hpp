#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>

#include "common_util.hpp"

#include "entity.hpp"

namespace environment {

const char vertex_shader_source[] =
R"(#version 330 core

const vec2 VERTICES[4] = vec2[4](
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, -1.0)
);

uniform mat4 view;
uniform mat4 projection;

out vec3 position;

void main() {
    vec2 vertex = VERTICES[gl_VertexID];
    
    mat4 view_projection_inverse = inverse(projection * view);
    
    vec4 ndc = vec4(vertex, 0.0, 1.0);
    vec4 clip_space = view_projection_inverse * ndc;
    position = clip_space.xyz / clip_space.w;
    
    gl_Position = vec4(vertex, 0.0, 1.0);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec3 camera_position;
uniform vec3 ambient_light_color;
uniform sampler2D environment_texture;

in vec3 position;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

void main() {
    vec3 pixel_direction = normalize(position - camera_position);
    
    float x = atan(pixel_direction.z, pixel_direction.x) / PI * 0.5 + 0.5;
    float y = -atan(pixel_direction.y, length(pixel_direction.xz)) / PI + 0.5;
    vec3 env_albedo = texture(environment_texture, vec2(x, y)).rgb;
    
    out_color = vec4(env_albedo * 0.9 + ambient_light_color * 0.1, 1.0);
}
)";

struct environment_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;
    
    GLuint camera_position_location, ambient_light_color_location, environment_texture_location;

    GLuint environment_texture;

    environment_t(int object_index) {
        (void)object_index;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        camera_position_location = glGetUniformLocation(program, "camera_position");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");
        environment_texture_location = glGetUniformLocation(program, "environment_texture");

        glGenVertexArrays(1, &vao);

        std::string project_root = PROJECT_ROOT;
        environment_texture = load_texture(project_root + "/models/environment/HDR_040_Field_Bg.jpg");
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)light_direction; (void)light_color; (void)time;

        glDisable(GL_DEPTH_TEST);

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(camera_position_location, 1, reinterpret_cast<const float*>(&camera_position));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));
        glUniform1i(environment_texture_location, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, environment_texture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glClear(GL_DEPTH_BUFFER_BIT);
    }
};

}