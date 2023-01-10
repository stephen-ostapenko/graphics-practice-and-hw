#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>

#include "common_util.hpp"

#include "entity.hpp"

namespace hud {

const char vertex_shader_source[] =
R"(#version 330 core

uniform vec2 position;
uniform vec2 width_height;

void main() {
    if (gl_VertexID == 0) {
        gl_Position = vec4(position, 0.0, 1.0);
    } else if (gl_VertexID == 1) {
        gl_Position = vec4(position.x + width_height.x, position.y, 0.0, 1.0);
    } else if (gl_VertexID == 2) {
        gl_Position = vec4(position.x, position.y + width_height.y, 0.0, 1.0);
    } else if (gl_VertexID == 3) {
        gl_Position = vec4(position + width_height, 0.0, 1.0);
    } else {
        return;
    }
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec3 color;

layout (location = 0) out vec4 out_color;

void main() {
    out_color = vec4(color, 0.5);
}
)";

struct hud_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    roses::roses_t *roses_ptr;

    GLuint position_location, width_height_location, color_location;

    hud_t(int object_index, roses::roses_t *roses) {
        (void)object_index;

        roses_ptr = roses;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        position_location = glGetUniformLocation(program, "position");
        width_height_location = glGetUniformLocation(program, "width_height");
        color_location = glGetUniformLocation(program, "color");

        glGenVertexArrays(1, &vao);
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)view; (void)projection; (void)camera_position;
        (void)light_direction; (void)light_color; (void)ambient_light_color;
        (void)time;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(program);
        
        glUniform2f(position_location, -.005f, 1.f - .15f);
        glUniform2f(width_height_location, .01f, .1f);
        glUniform3f(color_location, 0.f, 1.f, 0.f);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glUniform2f(position_location, -1.f, 1.f - .15f);
        glUniform2f(width_height_location, 2.f * roses_ptr->roses_by_player / (float)roses_ptr->roses_cnt, .1f);
        glUniform3f(color_location, 0.f, 0.f, 1.f);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glUniform2f(position_location, 1.f, 1.f - .15f);
        glUniform2f(width_height_location, -2.f * roses_ptr->roses_by_mouse / (float)roses_ptr->roses_cnt, .1f);
        glUniform3f(color_location, 1.f, 0.f, 0.f);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

}