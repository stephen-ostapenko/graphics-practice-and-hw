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
#include <atomic>
#include <unistd.h>

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum type, const char * source) {
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

// =====================================================================================

const char vertex_shader_source[] =
R"(#version 330 core

uniform float time;

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec4 in_col;

out vec4 color;

void main() {
    gl_Position = vec4(in_pos, 0.0, 1.0);
    color = in_col;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec4 color;

layout (location = 0) out vec4 out_col;

void main() {
    out_col = color;
}
)";

using std::cerr;
using std::endl;

struct color {
    uint8_t channel[4];

    color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 1) {
        channel[0] = red; channel[1] = green; channel[2] = blue; channel[3] = alpha;
    }
};

const int X1 = -4, X2 = 4;
const int Y1 = -3, Y2 = 3;

const size_t MAX_RES = 512;
const size_t G_WIDTH = 800, G_HEIGHT = 600;

typedef std::pair <float, float> v2;
std::vector<v2> point_pos;
std::vector<color> point_col;
std::vector <uint32_t> ind;

float sqr(float x) {
    return x * x;
}

// 0 <= f <= 1
float f(float x, float y, float t) {
    float x1 = cos(t / 5) + 2, y1 = sin(t / 5);
    float d1 = sqr(x - x1) + sqr(y - y1);
    float x2 = cos(t / 2) - 1, y2 = 2 * sin(t / 2);
    float d2 = sqr(x - x2) + sqr(y - y2);
    return 1 / (std::min(d1, d2) + 1.0);
    //return ((x * x + y * y) / 25 * sin(t / 5) + 1) / 2;
    //return sqrt(x * x + y * y) / 5;
    //return floor(abs(x + y)) / 10;
}

color calc_color(float x) {
    return color((uint8_t)(x * 255), 0, (uint8_t)((1 - x) * 255));
}

void update_pos_buffer(GLuint pos_vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v2) * point_pos.size(), point_pos.data(), GL_DYNAMIC_DRAW);
}

void update_col_buffer(GLuint col_vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, col_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(color) * point_col.size(), point_col.data(), GL_STREAM_DRAW);
}

void update_ebo(GLuint ebo) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * ind.size(), ind.data(), GL_DYNAMIC_DRAW);
}

std::vector <uint32_t> gen_indices(size_t W_RES, size_t H_RES) {
    std::vector <uint32_t> res;

    for (size_t i = 0; i < H_RES; i++) {
        for (size_t j = 0; j < W_RES; j++) {
            res.push_back(i * (W_RES + 1) + j);
            res.push_back(i * (W_RES + 1) + j + 1);
            res.push_back((i + 1) * (W_RES + 1) + j);

            res.push_back(i * (W_RES + 1) + j + 1);
            res.push_back((i + 1) * (W_RES + 1) + j);
            res.push_back((i + 1) * (W_RES + 1) + j + 1);
        }
    }

    return res;
}

void recalc_poses_and_colors(
    float x1, float y1, float x2, float y2, float t,
    size_t W_RES, size_t H_RES,
    GLuint pos_vbo, GLuint col_vbo, GLuint ebo,
    bool recalc_pos = false, bool recalc_col = true
) {
    float size_w = x2 - x1, size_h = y2 - y1;
    float cell_size_w = size_w / W_RES, cell_size_h = size_h / H_RES;
    float center_x = (x1 + x2) / 2, center_y = (y1 + y2) / 2;

    float cur_y = y1;
    for (size_t i = 0, ptr = 0; i <= H_RES; i++, cur_y += cell_size_h) {
        float cur_x = x1;
        for  (size_t j = 0; j <= W_RES; j++, cur_x += cell_size_w, ptr++) {
            if (recalc_pos) {
                point_pos[ptr] = {
                    (cur_x - center_x) / size_w * 2.0,
                    (cur_y - center_y) / size_h * 2.0
                };
            }

            if (recalc_col) {
                point_col[ptr] = calc_color(f(cur_x, cur_y, t));
            }
        }
    }

    if (recalc_pos) {
        update_pos_buffer(pos_vbo);
        
        ind = gen_indices(W_RES, H_RES);
        ind.shrink_to_fit();
        update_ebo(ebo);
    }
    if (recalc_col) {
        update_col_buffer(col_vbo);
    }
}

void recalc_grid(
    float x1, float y1, float x2, float y2, float t,
    size_t W_RES, size_t H_RES, size_t POINTS_CNT,
    GLuint pos_vbo, GLuint col_vbo, GLuint ebo
) {
    point_pos.resize(POINTS_CNT);
    point_pos.shrink_to_fit();

    point_col.resize(POINTS_CNT);
    point_col.shrink_to_fit();

    recalc_poses_and_colors(x1, y1, x2, y2, t, W_RES, H_RES, pos_vbo, col_vbo, ebo, true, true);
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window * window = SDL_CreateWindow("Graphics HW #1",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        G_WIDTH, G_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    size_t W_RES = 8, H_RES = 8;
    size_t POINTS_CNT = (W_RES + 1) * (H_RES + 1);

    glViewport(0, 0, G_WIDTH, G_HEIGHT);

    GLuint pos_vbo;
    glGenBuffers(1, &pos_vbo);
    GLuint col_vbo;
    glGenBuffers(1, &col_vbo);
    GLuint ebo;
    glGenBuffers(1, &ebo);
    
    recalc_grid(X1, Y1, X2, Y2, 0, W_RES, H_RES, POINTS_CNT, pos_vbo, col_vbo, ebo);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(v2), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, col_vbo);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(color), (void*)0);

    GLuint vertex_sh = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_sh = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint program = create_program(vertex_sh, fragment_sh);

    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    bool running = true;
    while (running) {
        bool recalc = false;

        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_d:
                if (W_RES > 2) {
                    W_RES /= 2;
                }
                recalc = true;
                break;
            case SDLK_f:
                if (W_RES < MAX_RES) {
                    W_RES *= 2;
                }
                recalc = true;
                break;
            case SDLK_j:
                if (H_RES > 2) {
                    H_RES /= 2;
                }
                recalc = true;
                break;
            case SDLK_k:
                if (H_RES < MAX_RES) {
                    H_RES *= 2;
                }
                recalc = true;
                break;
            }
            break;
        case SDL_WINDOWEVENT: switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                int width = event.window.data1;
                int height = event.window.data2;

                float aspect_ratio = G_WIDTH / (float)G_HEIGHT;
                int g_width = std::min(width, (int)round(height * aspect_ratio));
                int g_height = std::min(height, (int)round(width / aspect_ratio));

                int width_border = (width - g_width) / 2;
                int height_border = (height - g_height) / 2;

                glViewport(width_border, height_border, g_width, g_height);
                break;
            }
            break;
        }

        if (!running)
            break;

        float time = (float)clock() / CLOCKS_PER_SEC * 10;
        if (recalc) {
            POINTS_CNT = (W_RES + 1) * (H_RES + 1);
            recalc_grid(X1, Y1, X2, Y2, time, W_RES, H_RES, POINTS_CNT, pos_vbo, col_vbo, ebo);
        } else {
            recalc_poses_and_colors(X1, Y1, X2, Y2, time, W_RES, H_RES, pos_vbo, col_vbo, ebo);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBindBuffer(GL_ARRAY_BUFFER, col_vbo);

        glDrawElements(GL_TRIANGLES, ind.size(), GL_UNSIGNED_INT, (void*)0);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);

} catch (std::exception const & e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}