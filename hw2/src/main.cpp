#include "scene.h"
#include "shaders.h"

#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>

#include <GL/glew.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

int main(int argc, char **argv) try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Graphics course hw #2",
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

    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");
    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");

    GLuint albedo_texture_location = glGetUniformLocation(program, "albedo_texture");
    GLuint opacity_texture_location = glGetUniformLocation(program, "opacity_texture");
    
    GLuint ambient_light_location = glGetUniformLocation(program, "ambient_light");

    GLuint sun_direction_location = glGetUniformLocation(program, "sun_direction");
    GLuint sun_color_location = glGetUniformLocation(program, "sun_color");

    GLuint point_light_position_location = glGetUniformLocation(program, "point_light_position");
    GLuint point_light_color_location = glGetUniformLocation(program, "point_light_color");
    GLuint point_light_attenuation_location = glGetUniformLocation(program, "point_light_attenuation");

    GLuint glossiness_location = glGetUniformLocation(program, "glossiness");
    GLuint power_location = glGetUniformLocation(program, "power");

    std::string project_root = PROJECT_ROOT;
    std::string model_path = project_root + "/objects/" + argv[1];

    Assimp::Importer Importer;
    const aiScene* pScene = Importer.ReadFile(
        model_path.c_str(),
        iProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices
    );
    scene sc(pScene, project_root + "/objects");
    
    auto last_frame_start = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;
    bool slow_mode = false;

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
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

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        if (button_down[SDLK_w]) {
            sc.move_camera_forward(dt);
        }
        if (button_down[SDLK_s]) {
            sc.move_camera_backward(dt);
        }
        if (button_down[SDLK_a]) {
            sc.move_camera_left(dt);
        }
        if (button_down[SDLK_d]) {
            sc.move_camera_right(dt);
        }
        if (button_down[SDLK_UP]) {
            sc.move_camera_up(dt);
        }
        if (button_down[SDLK_DOWN]) {
            sc.move_camera_down(dt);
        }

        if (button_down[SDLK_LEFT]) {
            sc.turn_camera_left(dt);
        }
        if (button_down[SDLK_RIGHT]) {
            sc.turn_camera_right(dt);
        }
        if (button_down[SDLK_COMMA]) {
            sc.turn_camera_down(dt);
        }
        if (button_down[SDLK_PERIOD]) {
            sc.turn_camera_up(dt);
        }

        if (button_down[SDLK_LCTRL]) {
            if (slow_mode) {
                sc.speed_up();
            } else {
                sc.slow_down();
            }
            slow_mode = !slow_mode;
        }

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.8f, 0.8f, 1.f, 0.f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::rotate(view, sc.camera_yz_angle, {1.f, 0.f, 0.f});
        view = glm::rotate(view, sc.camera_xz_angle, {0.f, 1.f, 0.f});
        view = glm::translate(view, {sc.camera_x, sc.camera_y, sc.camera_z});

        float aspect = height / (float)width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, width / (float)height, sc.near, sc.far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float*>(&projection));
        glUniform3fv(camera_position_location, 1, (float*)(&camera_position));
        
        glUniform3f(ambient_light_location, .2f, .2f, .4f);

        glUniform3f(sun_color_location, 1.f, .5f, .5f);
        glm::vec3 sun_direction = glm::normalize(glm::vec3(
            3.f * sc.get_mean_size() * sin(time),
            3.f * sc.get_mean_size() * cos(time),
            3.f * sc.get_mean_size()
        ));
        glUniform3f(sun_direction_location, sun_direction.x, sun_direction.y, sun_direction.z);

        glUniform3f(
            point_light_position_location,
            (5 * sc.get_min_size() + 10) * sin(time * 2) / 10,
            (sc.get_min_size() + 10) * sin(time) / 10 + sc.get_min_size() / 3,
            (sc.get_min_size() + 10) * cos(time * 3) / 10
        );
        glUniform3f(point_light_color_location, 0.f, 1.f, 0.f);
        glUniform3f(
            point_light_attenuation_location,
            sc.get_mean_size() / 1e3,
            0.f,
            sc.get_mean_size() / 1e9
        );

        model = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.f});
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));

        glUniform1i(albedo_texture_location, 0);
        glUniform1i(opacity_texture_location, 1);

        sc.draw(glossiness_location, power_location);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
