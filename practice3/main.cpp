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

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
layout (location = 2) in float dist;

out vec4 color;
out float distance;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    color = in_color;
    distance = dist;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform bool dashed;
uniform float offset;

in vec4 color;
in float distance;

layout (location = 0) out vec4 out_color;

void main()
{
    if (!dashed) {
        out_color = color;
        return;
    }

    if (mod(distance + offset, 40.0) < 20) {
        out_color = color;
    } else {
        discard;
    }
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

struct vec2
{
    float x;
    float y;
};

struct vertex
{
    vec2 position;
    std::uint8_t color[4];
    float dist;
};

vec2 bezier(std::vector<vertex> const & vertices, float t)
{
    std::vector<vec2> points(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;

    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

void rebuild_bezier(std::vector <vertex> &vertices, std::vector <vertex> &bezier_vertices, int quality) {
    bezier_vertices.clear();

    for (int i = 0; i < vertices.size() * quality; i++) {
        vec2 cur = bezier(vertices, i / (float)(vertices.size() * quality - 1));
        float dist = 0.f;

        if (bezier_vertices.size()) {
            vec2 prev = bezier_vertices.back().position;
            dist = std::hypot(cur.x - prev.x, cur.y - prev.y) + bezier_vertices.back().dist;
        }

        bezier_vertices.push_back(
            vertex{
                cur,
                {255, 0, 0, 255},
                dist
            }
        );
    }
}

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

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
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

    //SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint dashed_location = glGetUniformLocation(program, "dashed");
    GLuint offset_location = glGetUniformLocation(program, "offset");

    std::vector <vertex> vertices;

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *)offsetof(vertex, color));

    std::vector <vertex> bezier_vertices;

    GLuint bezier_vao;
    glGenVertexArrays(1, &bezier_vao);
    glBindVertexArray(bezier_vao);

    GLuint bezier_vbo;
    glGenBuffers(1, &bezier_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *)offsetof(vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, dist));

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    int quality = 4;

    bool running = true;
    while (running)
    {
        bool updated = false;

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
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;
                
                vertices.push_back(
                    vertex{
                        vec2{(float)mouse_x, (float)mouse_y},
                        {0, 0, 255, 255},
                        0.f
                    }
                );

                updated = true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (vertices.size()) {
                    vertices.pop_back();

                    updated = true;
                }
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_LEFT) {
                if (quality > 1) {
                    quality--;

                    updated = true;
                }
            }
            else if (event.key.keysym.sym == SDLK_RIGHT) {
                quality++;

                updated = true;
            }
            break;
        }

        if (!running)
            break;

        if (updated) {
            rebuild_bezier(vertices, bezier_vertices, quality);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, bezier_vbo);
            glBufferData(GL_ARRAY_BUFFER, bezier_vertices.size() * sizeof(vertex), bezier_vertices.data(), GL_DYNAMIC_DRAW);
        }

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] = {
            2.f / width, 0.f, 0.f, -1.f,
            0.f, -2.f / height, 0.f, 1.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        glBindVertexArray(vao);
        glUniform1i(dashed_location, 0);

        glLineWidth(2.f);
        glDrawArrays(GL_LINE_STRIP, 0, vertices.size());

        glPointSize(10.f);
        glDrawArrays(GL_POINTS, 0, vertices.size());

        glBindVertexArray(bezier_vao);
        glUniform1i(dashed_location, 1);
        glUniform1f(offset_location, sin(time * 2) * 100);

        glLineWidth(5.f);
        glDrawArrays(GL_LINE_STRIP, 0, bezier_vertices.size());

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
