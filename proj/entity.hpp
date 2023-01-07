#pragma once

#include <GL/glew.h>

#include "gltf_loader.hpp"

namespace entity {

struct entity {
    struct gltf_mesh {
        GLuint vao;
        gltf_model::accessor indices;
        gltf_model::material material;
    };

    void setup_attribute(int index, gltf_model::accessor const & accessor, bool integer = false) {
        glEnableVertexAttribArray(index);
        if (integer) {
            glVertexAttribIPointer(index, accessor.size, accessor.type, 0, reinterpret_cast<void*>(accessor.view.offset));
        } else {
            glVertexAttribPointer(index, accessor.size, accessor.type, GL_FALSE, 0, reinterpret_cast<void*>(accessor.view.offset));
        }
    };

    GLuint vertex_shader, fragment_shader, program;
    GLuint model_location, view_location, projection_location;
    GLuint vao, vbo, ebo;

    std::uint32_t indices_count;

    GLuint light_direction_location, light_color_location, ambient_light_color_location;

    virtual void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) = 0;

    virtual void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) = 0;
};

}