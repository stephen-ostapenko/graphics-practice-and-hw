#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <string>

#include "entity.hpp"

namespace papich {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

out vec3 position;
out vec3 normal;
out vec2 texcoord;

void main() {
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * in_normal;
    texcoord = in_texcoord;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main() {
    float diffuse = max(0.0, dot(normalize(normal), light_direction));
    vec3 albedo = texture(albedo_texture, texcoord).rgb;
    
    out_color = vec4(albedo * (light_color * diffuse + ambient_light_color), 1.0);
}
)";

struct papich_t : entity::entity {
    using vertex = obj_data::vertex;

    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    GLuint texture;
    
    GLuint texture_location;

    const float scale = 1.f;
    const float std_turn_speed = 1.f;
    const float fast_turn_speed = 2.f;
    const float std_move_speed = 1.f;
    const float fast_move_speed = 2.f;

    float angle = -glm::pi<float>() / 2.f;
    glm::vec3 position{0.f, 0.f, 0.f};

    papich_t(int object_index) {
        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        texture_location = glGetUniformLocation(program, "albedo_texture");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");

        std::string project_root = PROJECT_ROOT;
        obj_data model = parse_obj(project_root + "/models/papich/papich.obj");

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, texcoord));

        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(vertex), model.vertices.data(), GL_STATIC_DRAW);

        indices_count = model.indices.size();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_count * sizeof(std::uint32_t), model.indices.data(), GL_STATIC_DRAW);

        std::string texture_path = project_root + "/models/papich/papich.jpg";
        glActiveTexture(GL_TEXTURE0);
        texture = load_texture(texture_path);
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time;

        float turn_speed = std_turn_speed;
        if (button_down[SDLK_LSHIFT]) {
            turn_speed = fast_turn_speed;
        }

        if (button_down[SDLK_a]) {
            angle += turn_speed * dt;
        }
        if (button_down[SDLK_d]) {
            angle -= turn_speed * dt;
        }

        float move_speed = std_move_speed;
        if (button_down[SDLK_LSHIFT]) {
            move_speed = fast_move_speed;
        }

        glm::vec3 move_direction = glm::rotate((glm::vec3){1.f, 0.f, 0.f}, angle, (glm::vec3){0.f, 1.f, 0.f});

        if (button_down[SDLK_w]) {
            position += move_direction * move_speed * dt;
        }
        if (button_down[SDLK_s]) {
            position -= move_direction * move_speed * dt;
        }
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        (void)camera_position; (void)time;

        glm::mat4 model = glm::mat4(1.f);
        model = glm::translate(model, position);
        model = glm::rotate(model, angle, {0.f, 1.f, 0.f});
        model = glm::scale(model, glm::vec3(scale));

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));

        glUniform1i(texture_location, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT, nullptr);
    }
};

}