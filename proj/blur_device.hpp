#pragma once

#include <GL/glew.h>

#include <string>

#include "common_util.hpp"

namespace blur_device {

const char vertex_shader_source[] =
R"(#version 330 core

out vec2 texcoord;

vec2 vertices[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main() {
    vec2 vertex = vertices[gl_VertexID];
    gl_Position = vec4(vertex, 0.0, 1.0);
    texcoord = vertex / 2.0 + vec2(0.5);
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D render_result;
uniform int mode;
uniform float time;

in vec2 texcoord;

layout (location = 0) out vec4 out_color;

vec4 gaussian_blur() {
    vec4 sum = vec4(0.0);
    float sum_w = 0.0;
    const int N = 7;
    float radius = 5.0;
    for (int x = -N; x <= N; ++x) {
        for (int y = -N; y <= N; ++y) {
            vec2 pt = vec2(x, y);
            pt.x += sin((x + y) * 50.0 + time) * 50.0;
            pt.y += sin((x + y) * 20.0 - time) * 50.0;
            
            float c = exp(-float(x * x + y * y) / (radius * radius));
            sum += c * texture(render_result, texcoord + pt / vec2(textureSize(render_result, 0)));
            sum_w += c;
        }
    }
    return sum / sum_w;
}

void main() {
    out_color = gaussian_blur();
}
)";

struct blur_device_t {
    GLuint texture, render_buffer, frame_buffer;
    GLuint vertex_shader, fragment_shader, program;
    GLuint render_result_location, mode_location, time_location;
    GLuint vao;

    blur_device_t(int width, int height) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glGenRenderbuffers(1, &render_buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, render_buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        glGenFramebuffers(1, &frame_buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buffer);

        assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        render_result_location = glGetUniformLocation(program, "render_result");
        mode_location = glGetUniformLocation(program, "mode");
        time_location = glGetUniformLocation(program, "time");

        glGenVertexArrays(1, &vao);
    }

    void update_size(int width, int height) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, render_buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    }

    void init() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
    }

    void show_output(int width, int height, float time) {
        (void)width; (void)height;

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glUseProgram(program);
        glUniform1i(render_result_location, 0);
        glUniform1i(mode_location, 0);
        glUniform1f(time_location, time);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};

}