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
#include <cassert>

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

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec4 in_col;

out vec4 color;

void main() {
    gl_Position = vec4(in_pos, 0.0, 1.0);
    color = in_col;
}
)";

const char isol_vertex_shader_source[] =
R"(#version 330 core

layout (location = 0) in vec2 in_pos;

out vec4 color;

void main() {
    gl_Position = vec4(in_pos, 0.0, 1.0);
    color = vec4(0.0, 1.0, 0.0, 1.0);
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

const float X1 = -4, X2 = 4;
const float Y1 = -3, Y2 = 3;

const size_t MAX_RES = 512;
const size_t G_WIDTH = 800, G_HEIGHT = 600;

typedef std::pair <float, float> v2;
#define x first
#define y second

std::vector<v2> point_pos, isol_point_pos;
std::vector<color> point_col;
std::vector <uint32_t> ind, isol_ind;
std::vector <float> isol_vals = { 0.25, 0.5, 0.33, 0.9, 0.75, 0.1 };
std::vector <std::array <uint32_t, 5>> isol_point_id;

float sqr(float x) {
    return x * x;
}

// 0 \le f \le 1
float f(float x, float y, float t) {
    float x1 = cos(t / 5) + 2, y1 = sin(t / 5);
    float d1 = sqr(x - x1) + sqr(y - y1);

    float x2 = cos(t / 2) - 1, y2 = 2 * sin(t / 2);
    float d2 = sqr(x - x2) + sqr(y - y2);

    float x3 = 2 * cos(t / 3), y3 = 1.5 * sin(t / 4);
    float d3 = sqr(x - x3) + sqr(y - y3);

    return 1 / (std::min({ d1, d2, d3 }) + 1.0);
    
    //return ((x * x + y * y) / 25 * sin(t / 5) + 1) / 2;
    
    //return sqrt(x * x + y * y) / 5;
    
    //return floor(abs(x + y)) / 10;
}

color calc_color(float x) {
    return color((uint8_t)(x * 255), 0, (uint8_t)((1 - x) * 255));
}

template <typename T>
void update_vbo(GLuint vbo, std::vector <T> &data) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(T) * data.size(), data.data(), GL_STREAM_DRAW);
}

void update_ebo(GLuint ebo, std::vector <uint32_t> &ind) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * ind.size(), ind.data(), GL_STREAM_DRAW);
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

void recalc_positions_and_colors(
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
        update_vbo(pos_vbo, point_pos);
        
        ind = gen_indices(W_RES, H_RES);
        ind.shrink_to_fit();
        update_ebo(ebo, ind);
    }
    if (recalc_col) {
        update_vbo(col_vbo, point_col);
    }
}

void recalc_isoline_indices(size_t W_RES, size_t H_RES) {
    size_t ptr = 0;
    uint32_t nxt = 0;

    isol_point_id[ptr][0] = nxt++;
    isol_point_id[ptr][1] = nxt++;
    isol_point_id[ptr][2] = nxt++;
    isol_point_id[ptr][3] = nxt++;
    isol_point_id[ptr][4] = nxt++;
    ptr++;

    for (size_t i = 1; i < W_RES; i++, ptr++) {
        isol_point_id[ptr][0] = nxt++;
        isol_point_id[ptr][1] = isol_point_id[ptr - 1][3];
        isol_point_id[ptr][2] = nxt++;
        isol_point_id[ptr][3] = nxt++;
        isol_point_id[ptr][4] = nxt++;
    }

    for (size_t i = 1; i < H_RES; i++) {
        isol_point_id[ptr][0] = nxt++;
        isol_point_id[ptr][1] = nxt++;
        isol_point_id[ptr][2] = isol_point_id[ptr - W_RES][0];
        isol_point_id[ptr][3] = nxt++;
        isol_point_id[ptr][4] = nxt++;
        ptr++;

        for (size_t j = 1; j < W_RES; j++, ptr++) {
            isol_point_id[ptr][0] = nxt++;
            isol_point_id[ptr][1] = isol_point_id[ptr - 1][3];
            isol_point_id[ptr][2] = isol_point_id[ptr - W_RES][0];
            isol_point_id[ptr][3] = nxt++;
            isol_point_id[ptr][4] = nxt++;
        }
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

    recalc_positions_and_colors(x1, y1, x2, y2, t, W_RES, H_RES, pos_vbo, col_vbo, ebo, true, true);

    isol_point_pos.resize(W_RES * (H_RES + 1) + (W_RES + 1) * H_RES + W_RES * H_RES);
    isol_point_pos.shrink_to_fit();

    isol_point_id.resize(W_RES * H_RES);
    isol_point_id.shrink_to_fit();

    recalc_isoline_indices(W_RES, H_RES);
}

bool eq(float a, float b) {
    return (a - b) == 0.0f;
}

bool le(float a, float b) {
    return (b - a) > 0;
}

v2 interpolate(v2 a1, float b1, v2 a2, float b2, float val) {
    if (eq(b1, b2)) {
        return { (a1.x + a2.x) / 2, (a1.y + a2.y) / 2 };
    }

    float k = (val - b1) / (b2 - b1);
    return { a1.x + (a2.x - a1.x) * k, a1.y + (a2.y - a1.y) * k };
}

void update_isoline_triangle(
    v2 p1, v2 p2, v2 p3,
    uint32_t ind12, uint32_t ind23, uint32_t ind31,
    float x1, float y1, float x2, float y2,
    float val, float t, std::vector <uint32_t> &ans
) {
    float center_x = (x1 + x2) / 2, center_y = (y1 + y2) / 2;
    float size_w = x2 - x1, size_h = y2 - y1;
    p1 = { center_x + p1.x * size_w / 2, center_y + p1.y * size_h / 2 };
    p2 = { center_x + p2.x * size_w / 2, center_y + p2.y * size_h / 2 };
    p3 = { center_x + p3.x * size_w / 2, center_y + p3.y * size_h / 2 };

    uint8_t mask = 0;
    if (le(val, f(p1.x, p1.y, t))) {
        mask |= 1;
    }
    if (le(val, f(p2.x, p2.y, t))) {
        mask |= 2;
    }
    if (le(val, f(p3.x, p3.y, t))) {
        mask |= 4;
    }

    v2 &ip12 = isol_point_pos[ind12];
    v2 &ip23 = isol_point_pos[ind23];
    v2 &ip31 = isol_point_pos[ind31];

    if (mask == 0b000 || mask == 0b111) {
        return;
    }

    if (mask == 0b001 || mask == 0b110) {
        ip12 = interpolate(p1, f(p1.x, p1.y, t), p2, f(p2.x, p2.y, t), val);
        ip31 = interpolate(p1, f(p1.x, p1.y, t), p3, f(p3.x, p3.y, t), val);

        ans.push_back(ind12); ans.push_back(ind31);
    }

    if (mask == 0b101 || mask == 0b010) {
        ip12 = interpolate(p1, f(p1.x, p1.y, t), p2, f(p2.x, p2.y, t), val);
        ip23 = interpolate(p2, f(p2.x, p2.y, t), p3, f(p3.x, p3.y, t), val);

        ans.push_back(ind12); ans.push_back(ind23);
    }

    if (mask == 0b100 || mask == 0b011) {
        ip23 = interpolate(p3, f(p3.x, p3.y, t), p2, f(p2.x, p2.y, t), val);
        ip31 = interpolate(p1, f(p1.x, p1.y, t), p3, f(p3.x, p3.y, t), val);

        ans.push_back(ind23); ans.push_back(ind31);
    }

    ip12 = { (ip12.x - center_x) / size_w * 2, (ip12.y - center_y) / size_h * 2 };
    ip23 = { (ip23.x - center_x) / size_w * 2, (ip23.y - center_y) / size_h * 2 };
    ip31 = { (ip31.x - center_x) / size_w * 2, (ip31.y - center_y) / size_h * 2 };
}

std::vector <uint32_t> build_isoline(
    float x1, float y1, float x2, float y2,
    float val, float t,
    size_t W_RES, size_t H_RES
) {
    std::vector <uint32_t> ans;

    for (size_t i = 0, p_ptr = 0; i < H_RES; i++) {
        for (size_t j = 0; j < W_RES; j++, p_ptr++) {
            size_t w[4] = { p_ptr, p_ptr + 1, p_ptr + (W_RES + 1), p_ptr + (W_RES + 2) };
            size_t cell_ptr = i * W_RES + j;

            update_isoline_triangle(
                point_pos[w[0]], point_pos[w[1]], point_pos[w[2]],
                isol_point_id[cell_ptr][2], isol_point_id[cell_ptr][4], isol_point_id[cell_ptr][1],
                x1, y1, x2, y2,
                val, t, ans
            );

            update_isoline_triangle(
                point_pos[w[1]], point_pos[w[2]], point_pos[w[3]],
                isol_point_id[cell_ptr][4], isol_point_id[cell_ptr][0], isol_point_id[cell_ptr][3],
                x1, y1, x2, y2,
                val, t, ans
            );
        }

        p_ptr++;
    }

    return ans;
}

void draw_isolines(
    size_t isolines_cnt,
    float x1, float y1, float x2, float y2, float t,
    size_t W_RES, size_t H_RES,
    GLuint isol_pos_vbo, GLuint isol_ebo, GLuint isol_vao,
    GLuint isol_program
) {
    for (size_t i = 0; i < isolines_cnt; i++) {
        float val = isol_vals[i];
        isol_ind = build_isoline(x1, y1, x2, y2, val, t, W_RES, H_RES);

        update_vbo(isol_pos_vbo, isol_point_pos);
        update_ebo(isol_ebo, isol_ind);

        glUseProgram(isol_program);
        glBindVertexArray(isol_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, isol_ebo);
        glDrawElements(GL_LINES, isol_ind.size(), GL_UNSIGNED_INT, (void*)0);
    }
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

    size_t W_RES = 16, H_RES = 16;
    size_t POINTS_CNT = (W_RES + 1) * (H_RES + 1);
    size_t isolines_cnt = 1;

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

    GLuint isol_pos_vbo;
    glGenBuffers(1, &isol_pos_vbo);
    GLuint isol_ebo;
    glGenBuffers(1, &isol_ebo);

    GLuint isol_vao;
    glGenVertexArrays(1, &isol_vao);
    glBindVertexArray(isol_vao);
    glBindBuffer(GL_ARRAY_BUFFER, isol_pos_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(v2), (void*)0);
    
    GLuint vertex_sh = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_sh = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    GLuint isol_vertex_sh = create_shader(GL_VERTEX_SHADER, isol_vertex_shader_source);
    GLuint program = create_program(vertex_sh, fragment_sh);
    GLuint isol_program = create_program(isol_vertex_sh, fragment_sh);

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
            case SDLK_o:
                if (isolines_cnt > 0) {
                    isolines_cnt--;
                }
                break;
            case SDLK_p:
                if (isolines_cnt < isol_vals.size()) {
                    isolines_cnt++;
                }
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

        float time = (float)clock() / CLOCKS_PER_SEC * 5;
        if (recalc) {
            POINTS_CNT = (W_RES + 1) * (H_RES + 1);
            recalc_grid(X1, Y1, X2, Y2, time, W_RES, H_RES, POINTS_CNT, pos_vbo, col_vbo, ebo);
        } else {
            recalc_positions_and_colors(X1, Y1, X2, Y2, time, W_RES, H_RES, pos_vbo, col_vbo, ebo);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glDrawElements(GL_TRIANGLES, ind.size(), GL_UNSIGNED_INT, (void*)0);

        draw_isolines(
            isolines_cnt,
            X1, Y1, X2, Y2, time,
            W_RES, H_RES,
            isol_pos_vbo, isol_ebo, isol_vao,
            isol_program
        );

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);

} catch (std::exception const & e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}