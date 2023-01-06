#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cstring>

#include "obj_parser.hpp"

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 view;
uniform mat4 transform;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 normal;

void main()
{
    gl_Position = view * transform * vec4(in_position, 1.0);
    normal = mat3(transform) * in_normal;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec3 normal;

layout (location = 0) out vec4 out_color;

void main()
{
    float lightness = 0.5 + 0.5 * dot(normalize(normal), normalize(vec3(1.0, 2.0, 3.0)));
    out_color = vec4(vec3(lightness), 1.0);
}
)";

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

#define vertex obj_data::vertex

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 4",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint transform_location = glGetUniformLocation(program, "transform");

    std::string project_root = PROJECT_ROOT;
    obj_data bunny = parse_obj(project_root + "/bunny.obj");

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));

    glBufferData(GL_ARRAY_BUFFER, bunny.vertices.size() * sizeof(vertex), bunny.vertices.data(), GL_DYNAMIC_DRAW);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bunny.indices.size() * sizeof(std::uint32_t), bunny.indices.data(), GL_DYNAMIC_DRAW);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    float bunny_x = 0.f;
    float bunny_y = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }

        if (!running)
            break;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        // glCullFace(GL_FRONT);

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        float speed = 5.f;

        if (button_down[SDLK_LEFT]) {
            bunny_x -= speed * dt;
        }
        if (button_down[SDLK_RIGHT]) {
            bunny_x += speed * dt;
        }
        if (button_down[SDLK_UP]) {
            bunny_y += speed * dt;
        }
        if (button_down[SDLK_DOWN]) {
            bunny_y -= speed * dt;
        }

        float scale = .7f;
        float angle = time;

        float near = .01f;
        float far = 100.f;

        float right = sqrt(3) * near;
        float top = height / (float)width * right;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float view[16] = {
            near / right, 0.f, 0.f, 0.f,
            0.f, near / top, 0.f, 0.f,
            0.f, 0.f, -(far + near) / (far - near), -2 * far * near / (far - near),
            0.f, 0.f, -1.f, 1.f,
        };

        float transform_base[16] = {
            scale, scale, scale, 0.f,
            scale, scale, scale, 0.f,
            scale, scale, scale, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        float transform1[16], transform2[16], transform3[16];
        std::memcpy(transform1, transform_base, 16 * sizeof(float));
        std::memcpy(transform2, transform_base, 16 * sizeof(float));
        std::memcpy(transform3, transform_base, 16 * sizeof(float));

        transform1[5] *= cos(angle);
        transform1[6] *= -sin(angle);
        transform1[9] *= sin(angle);
        transform1[10] *= cos(angle);
        transform1[1] = transform1[2] = transform1[4] = transform1[8] = 0.f;

        transform2[0] *= cos(angle);
        transform2[2] *= sin(angle);
        transform2[8] *= -sin(angle);
        transform2[10] *= cos(angle);
        transform2[1] = transform2[4] = transform2[6] = transform2[9] = 0.f;

        transform3[0] *= cos(angle);
        transform3[1] *= -sin(angle);
        transform3[4] *= sin(angle);
        transform3[5] *= cos(angle);
        transform3[2] = transform3[6] = transform3[8] = transform3[9] = 0.f;

        transform1[3] += bunny_x;
        transform1[7] += bunny_y;
        transform1[11] += -3;

        transform2[3] += bunny_x + 1;
        transform2[7] += bunny_y - 2;
        transform2[11] += -5;

        transform3[3] += bunny_x - 4;
        transform3[7] += bunny_y + 2;
        transform3[11] += -7;

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        glBindVertexArray(vao);
        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform1);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, 0);

        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform2);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, 0);

        glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform3);
        glDrawElements(GL_TRIANGLES, bunny.indices.size(), GL_UNSIGNED_INT, 0);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
