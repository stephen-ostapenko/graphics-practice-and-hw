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

#include "environment.hpp"
#include "board.hpp"
#include "box.hpp"
#include "bitmap.hpp"
#include "papich.hpp"
#include "papich_hat.hpp"
#include "mouse.hpp"
#include "roses.hpp"
#include "cloud.hpp"
#include "blur_device.hpp"

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
    glViewport(0, 0, width, height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    environment::environment_t environment(0);
    board::board_t board(1);
    box::box_t box(2);
    bitmap::bitmap_t bitmap(3);
    papich::papich_t papich(4);
    papich_hat::papich_hat_t papich_hat(5, &papich);
    mouse::mouse_t mouse(6);
    roses::roses_t roses(7, &papich, &mouse);
    cloud::cloud_t cloud(8);

    blur_device::blur_device_t blur(width, height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map <SDL_Keycode, bool> button_down;

    float view_elevation = glm::radians(30.f);
    float view_azimuth = 0.f;
    float camera_distance = 2.f;

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

                blur.update_size(width, height);

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
            camera_distance -= 8.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 8.f * dt;

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

        if (!paused) {
            environment.update_state(time, dt, button_down);
            board.update_state(time, dt, button_down);
            box.update_state(time, dt, button_down);
            bitmap.update_state(time, dt, button_down);
            papich.update_state(time, dt, button_down);
            papich_hat.update_state(time, dt, button_down);
            mouse.update_state(time, dt, button_down);
            roses.update_state(time, dt, button_down);
            cloud.update_state(time, dt, button_down);
        }

        float near = 0.1f;
        float far = 100.f;
        float top = near;
        float right = (top * width) / height;

        glm::mat4 view(1.f);
        if (button_down[SDLK_m]) {
            view = glm::translate(view, {0.f, 0.f, -camera_distance / 3.f});
            view = glm::rotate(view, view_elevation, {1.f, 0.f, 0.f});
            view = glm::rotate(view, view_azimuth, {0.f, 1.f, 0.f});
            view = glm::translate(view, -glm::vec3(mouse.position.x, 0.f, mouse.position.z));
        } else {
            view = glm::translate(view, {0.f, 0.f, -camera_distance});
            view = glm::rotate(view, view_elevation, {1.f, 0.f, 0.f});
            view = glm::rotate(view, view_azimuth, {0.f, 1.f, 0.f});
        }

        glm::mat4 projection = glm::mat4(1.f);
        projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 light_direction = glm::normalize(glm::vec3(2.f * sin(-time), 3.f, 2.f * cos(-time * 2.f)));
        glm::vec3 light_color(.7f, .3f + (1.f + sin(time)) / 4.f, .7f);
        glm::vec3 ambient_light_color(.3f);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (button_down[SDLK_b]) {
            blur.init();
        }

        environment.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        board.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        box.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        bitmap.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        papich.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        papich_hat.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        mouse.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        roses.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);
        cloud.draw(view, projection, camera_position, light_direction, light_color, ambient_light_color, time);

        if (button_down[SDLK_b]) {
            blur.show_output(width, height, time);
        }

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
