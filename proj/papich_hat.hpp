#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>
#include <cmath>

#include "entity.hpp"
#include "papich.hpp"

namespace papich_hat {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 position;
out vec3 normal;

void main() {
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * in_normal;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec4 albedo;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;

in vec3 position;
in vec3 normal;

layout (location = 0) out vec4 out_color;

void main() {
    float diffuse = max(0.0, dot(normalize(normal), light_direction));
    
    out_color = vec4(albedo.rgb * (light_color * diffuse + ambient_light_color), albedo.a);
}
)";

struct papich_hat_t : entity::entity {
    using vertex = obj_data::vertex;

    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    GLuint albedo_location;

    const float scale = .25f;
    const float correction_angle = glm::pi<float>() / 2.f;

    papich::papich_t *papich_ptr;

    gltf_model hat;
    std::vector <gltf_mesh> meshes;

    papich_hat_t(int object_index, papich::papich_t *papich) {
        papich_ptr = papich;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        albedo_location = glGetUniformLocation(program, "albedo");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");

        std::string project_root = PROJECT_ROOT;
        const std::string model_path = project_root + "/models/papich_hat/hat.gltf";

        hat = load_gltf(model_path);
        
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, hat.buffer.size(), hat.buffer.data(), GL_STATIC_DRAW);

        for (auto const &mesh : hat.meshes) {
            auto &result = meshes.emplace_back();
            glGenVertexArrays(1, &result.vao);
            glBindVertexArray(result.vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
            result.indices = mesh.indices;

            setup_attribute(0, mesh.position);
            setup_attribute(1, mesh.normal);

            result.material = mesh.material;
        }
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)camera_position;

        glm::mat4 model = glm::mat4(1.f);
        model = glm::translate(model, papich_ptr->position + glm::vec3(0.f, 1.f + sin(2 * time) / 3.f, 0.f));
        model = glm::rotate(model, papich_ptr->angle + correction_angle, {0.f, 1.f, 0.f});
        model = glm::scale(model, glm::vec3(scale));

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));

        auto draw_meshes = [&](bool transparent) {
            for (auto const & mesh : meshes) {
                if (mesh.material.transparent != transparent)
                    continue;

                if (mesh.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                if (transparent)
                    glEnable(GL_BLEND);
                else
                    glDisable(GL_BLEND);

                if (mesh.material.color) {
                    glUniform4fv(albedo_location, 1, reinterpret_cast<const float*>(&(*mesh.material.color)));
                } else {
                    continue;
                }

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indices.count, mesh.indices.type, reinterpret_cast<void*>(mesh.indices.view.offset));
            }
        };

        draw_meshes(false);
        glDepthMask(GL_FALSE);
        draw_meshes(true);
        glDepthMask(GL_TRUE);
    }
};

}