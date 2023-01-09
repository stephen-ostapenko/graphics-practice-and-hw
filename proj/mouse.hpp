#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <string>
#include <random>
#include <ctime>

#include "common_util.hpp"
#include "stb_image.h"
#include "gltf_loader.hpp"

#include "entity.hpp"

namespace mouse {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4x3 bones[64];

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in ivec4 in_joints;
layout (location = 4) in vec4 in_weights;

out vec3 position;
out vec3 normal;
out vec2 texcoord;
out vec4 weights;

void main() {
    weights = in_weights;

    mat4x3 average = bones[in_joints.x] * weights.x + \
                     bones[in_joints.y] * weights.y + \
                     bones[in_joints.z] * weights.z + \
                     bones[in_joints.w] * weights.w;

    average /= weights.x + weights.y + weights.z + weights.w;

    position = (model * mat4(average) * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * mat3(average) * in_normal;
    texcoord = in_texcoord;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo;
uniform sampler2D roughness_texture;
uniform vec4 color;
uniform int use_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;
uniform vec3 camera_position;

layout (location = 0) out vec4 out_color;

in vec3 position;
in vec3 normal;
in vec2 texcoord;
in vec4 weights;

float specular() {
    float roughness = texture(roughness_texture, texcoord).r / 2.0;
    float glossiness = 1.0 / roughness;
    float power = 1.0 / roughness / roughness - 1.0;

    vec3 reflected = 2.0 * normal * dot(normal, light_direction) - light_direction;

    vec3 view_direction = normalize(camera_position - position);
    return glossiness * pow(max(0.0, dot(reflected, view_direction)), power);
}

void main() {
    vec4 albedo_color;

    if (use_texture == 1)
        albedo_color = texture(albedo, texcoord);
    else
        albedo_color = color;

    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color.rgb * (light_color * diffuse + ambient_light_color + specular()), albedo_color.a);
}
)";

struct mouse_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    GLuint roughness_texture;

    GLuint camera_position_location;
    GLuint albedo_location, color_location, use_texture_location, roughness_texture_location;
    GLuint bones_location;

    gltf_model animodel;
    std::vector <gltf_mesh> meshes;
    std::map <std::string, GLuint> textures;

    const float scale = .5f;
    const float move_speed = 7.2f;
    const float eps = 1e-6f;
    const float board_size = 24.f;

    float angle = 0.f;
    glm::vec3 position{0.f, .04f, 0.f};

    glm::vec3 move_direction{0.f, 0.f, 0.f};
    float distance_left = 0.f;
    std::default_random_engine random_engine;
    std::uniform_real_distribution <float> angle_distr{0.f, glm::pi<float>() * 2.f};
    std::uniform_real_distribution <float> distance_distr{1.f, board_size / 2};
    std::uniform_real_distribution <float> random_pt_distr{0.f, board_size / 2};

    const float animation_start = 1.33333f;
    const float animation_stop = 2.125f;
    const float animation_speed = 3.f;

    mouse_t(int object_index) {
        (void)object_index;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        camera_position_location = glGetUniformLocation(program, "camera_position");
        albedo_location = glGetUniformLocation(program, "albedo");
        color_location = glGetUniformLocation(program, "color");
        use_texture_location = glGetUniformLocation(program, "use_texture");
        roughness_texture_location = glGetUniformLocation(program, "roughness_texture");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");
        bones_location = glGetUniformLocation(program, "bones");

        std::string project_root = PROJECT_ROOT;
        const std::string model_path = project_root + "/models/mouse/W_hlmaus.gltf";

        animodel = load_gltf(model_path);
        
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, animodel.buffer.size(), animodel.buffer.data(), GL_STATIC_DRAW);

        for (const auto &mesh : animodel.meshes) {
            auto &result = meshes.emplace_back();
            glGenVertexArrays(1, &result.vao);
            glBindVertexArray(result.vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
            result.indices = mesh.indices;

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            setup_attribute(0, mesh.position);
            setup_attribute(1, mesh.normal);
            if (mesh.texcoord) {
                setup_attribute(2, mesh.texcoord.value());
            }
            if (mesh.joints) {
                setup_attribute(3, mesh.joints.value(), true);
            }
            if (mesh.weights) {
                setup_attribute(4, mesh.weights.value());
            }

            result.material = mesh.material;
        }

        for (auto const & mesh : meshes) {
            if (!mesh.material.texture_path)
                continue;

            if (textures.contains(*mesh.material.texture_path))
                continue;

            auto path = std::filesystem::path(model_path).parent_path() / *mesh.material.texture_path;

            textures[*mesh.material.texture_path] = load_texture(path.string());
        }
        roughness_texture = load_texture(project_root + "/models/mouse/Feldmaus_Rough.png");

        random_engine.seed(std::time(0));
    }

    void update_moving_direction(bool random_angle = true) {
        if (!random_angle) {
            float x = random_pt_distr(random_engine);
            float z = random_pt_distr(random_engine);

            move_direction = glm::normalize(glm::vec3(x, position.y, z) - position);
            distance_left = distance_distr(random_engine);
            angle = acos(glm::dot(move_direction, (glm::vec3){0.f, 0.f, 1.f}));
            if (move_direction.x < 0) {
                angle = -angle;
            }

            return;
        }

        angle = angle_distr(random_engine);
        distance_left = distance_distr(random_engine);
        move_direction = glm::rotate((glm::vec3){0.f, 0.f, 1.f}, angle, (glm::vec3){0.f, 1.f, 0.f});
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)button_down;

        if (std::max(abs(position.x), abs(position.z)) > board_size) {
            update_moving_direction(false);
        }

        if (distance_left - eps < 0.f) {
            update_moving_direction();
        }

        position += move_direction * move_speed * dt;
        distance_left -= move_speed * dt;
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        glm::mat4 model = glm::mat4(1.f);
        model = glm::translate(model, position);
        model = glm::rotate(model, angle, {0.f, 1.f, 0.f});
        model = glm::scale(model, glm::vec3(scale));

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::vector <glm::mat4x3> bones(animodel.bones.size(), glm::mat3x4(scale));

        auto run_animation = animodel.animations.at("Gallopp 33-52");
        float phase = animation_start + std::fmod(time * animation_speed, animation_stop - animation_start);

        for (int i = 0; i < bones.size(); i++) {
            glm::mat4 translation = glm::translate(glm::mat4(1.f), run_animation.bones[i].translation(phase));
            glm::mat4 scale = glm::scale(glm::mat4(1.f), run_animation.bones[i].scale(phase));
            glm::mat4 rotation = glm::toMat4(run_animation.bones[i].rotation(phase));

            glm::mat4 transform = translation * rotation * scale;

            if (animodel.bones[i].parent != -1) {
                transform = bones[animodel.bones[i].parent] * transform;
            }

            bones[i] = transform;
        }
        for (int i = 0; i < bones.size(); i++) {
            bones[i] = bones[i] * animodel.bones[i].inverse_bind_matrix;
        }

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform3fv(camera_position_location, 1, reinterpret_cast<const float*>(&camera_position));
        glUniform1i(roughness_texture_location, 1);
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));
        glUniformMatrix4x3fv(bones_location, bones.size(), GL_FALSE, reinterpret_cast<const float*>(bones.data()));

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

                if (mesh.material.texture_path) {
                    glBindTexture(GL_TEXTURE_2D, textures[*mesh.material.texture_path]);
                    glUniform1i(use_texture_location, 1);
                    glUniform1i(albedo_location, 0);
                } else if (mesh.material.color) {
                    glUniform1i(use_texture_location, 0);
                    glUniform4fv(color_location, 1, reinterpret_cast<const float*>(&(*mesh.material.color)));
                } else
                    continue;

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indices.count, mesh.indices.type, reinterpret_cast<void*>(mesh.indices.view.offset));
            }
        };

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, roughness_texture);

        glActiveTexture(GL_TEXTURE0);
        draw_meshes(false);
        glDepthMask(GL_FALSE);
        draw_meshes(true);
        glDepthMask(GL_TRUE);
    }
};

}