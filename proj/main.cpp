#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <stdexcept>
#include <iostream>
#include <chrono>
#include <map>
#include <cmath>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "common_util.hpp"
#include "obj_parser.hpp"
#include "stb_image.h"
#include "gltf_loader.hpp"

#include "papich.hpp"
#include "papich_hat.hpp"
#include "mouse.hpp"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

int main() try {
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

    SDL_Window * window = SDL_CreateWindow("Graphics course final project",
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

    papich::papich_t papich(1);
    papich_hat::papich_hat_t papich_hat(2, &papich);
    mouse::mouse_t mouse(3);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map <SDL_Keycode, bool> button_down;

    float view_elevation = glm::radians(30.f);
    float view_azimuth = 0.f;
    float camera_distance = 2.f;

    /*
    float camera_x = 0, camera_y = 0, camera_z = 0;
    float camera_xz_angle = 0, camera_yz_angle = 0;
    */

    bool paused = false;

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            if (event.key.keysym.sym == SDLK_p)
                paused = !paused;
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
        if (!paused) {
            time += dt;
        }

        if (button_down[SDLK_UP])
            camera_distance -= 4.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 4.f * dt;

        if (button_down[SDLK_LEFT])
            view_azimuth += 2.f * dt;
        if (button_down[SDLK_RIGHT])
            view_azimuth -= 2.f * dt;

        if (button_down[SDLK_COMMA]) {
            view_elevation += 1.f * dt;
        }
        if (button_down[SDLK_PERIOD]) {
            view_elevation -= 1.f * dt;
        }

        papich.update_state(time, dt, button_down);
        papich_hat.update_state(time, dt, button_down);
        mouse.update_state(time, dt, button_down);

        float near = 0.1f;
        float far = 100.f;
        float top = near;
        float right = (top * width) / height;

        glm::mat4 model = glm::mat4(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_elevation, {1.f, 0.f, 0.f});
        view = glm::rotate(view, view_azimuth, {0.f, 1.f, 0.f});

        glm::mat4 projection = glm::mat4(1.f);
        projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 light_direction = glm::normalize(glm::vec3(2 * sin(-time), 1 + 2 * sin(3 * time), 2 * cos(-time)));
        glm::vec3 light_color(.9f, .5f + (1 + sin(time)) / 4.f, .9f);
        glm::vec3 ambient_light_color(.3f);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        // ========================================================================================================

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        papich.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        papich_hat.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        mouse.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);

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
