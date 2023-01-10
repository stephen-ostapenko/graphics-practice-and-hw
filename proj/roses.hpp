#pragma once

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <string>
#include <array>

#include "common_util.hpp"
#include "gltf_loader.hpp"
#include "aabb.hpp"
#include "frustum.hpp"
#include "intersect.hpp"

#include "entity.hpp"

namespace roses {

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool use_instanced_translation;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in vec3 in_translation;

out vec3 normal;
out vec2 texcoord;

void main() {
    vec3 position;
    if (use_instanced_translation) {
        position = (model * vec4(in_position + in_translation, 1.0)).xyz;
    } else {
        position = (model * vec4(in_position, 1.0)).xyz;
    }

    gl_Position = projection * view * vec4(position, 1.0);
    normal = mat3(model) * in_normal;
    texcoord = in_texcoord;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo;
uniform vec4 color;
uniform int use_texture;

uniform vec3 light_direction;
uniform vec3 light_color;
uniform vec3 ambient_light_color;

layout (location = 0) out vec4 out_color;

in vec3 normal;
in vec2 texcoord;

void main() {
    vec4 albedo_color;

    if (use_texture == 1)
        albedo_color = texture(albedo, texcoord);
    else
        albedo_color = color;

    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color.rgb * (light_color * diffuse + ambient_light_color), albedo_color.a);
}
)";

struct roses_t : entity::entity {
    // GLuint vertex_shader, fragment_shader, program;
    // GLuint model_location, view_location, projection_location;
    // GLuint vao, vbo, ebo;
    // GLuint light_direction_location, light_color_location, ambient_light_color_location;
    // std::uint32_t indices_count;

    GLuint albedo_location, color_location, use_texture_location;
    GLuint use_instanced_translation_location;

    const float scale = .012f;
    const float board_size = 24.f;
    static const int roses_density = 32;
    static const int roses_cnt = (roses_density - 1) * (roses_density - 1);

    std::vector <std::array <gltf_mesh, 3>> flowers;
    std::map <std::string, GLuint> textures;
    std::vector <std::pair <glm::vec3, glm::vec3>> bounds;

    bool mask[roses_density][roses_density] = { false };
    papich::papich_t *papich_ptr;
    mouse::mouse_t *mouse_ptr;
    int roses_by_player = 0, roses_by_mouse = 0;

    GLuint translations_vbo;
    std::vector <std::vector <glm::vec3>> translations;

    roses_t(int object_index, papich::papich_t *papich, mouse::mouse_t *mouse) {
        (void)object_index;

        papich_ptr = papich;
        mouse_ptr = mouse;

        vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
        fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        program = create_program(vertex_shader, fragment_shader);

        model_location = glGetUniformLocation(program, "model");
        view_location = glGetUniformLocation(program, "view");
        projection_location = glGetUniformLocation(program, "projection");
        use_instanced_translation_location = glGetUniformLocation(program, "use_instanced_translation");
        albedo_location = glGetUniformLocation(program, "albedo");
        color_location = glGetUniformLocation(program, "color");
        use_texture_location = glGetUniformLocation(program, "use_texture");
        light_direction_location = glGetUniformLocation(program, "light_direction");
        light_color_location = glGetUniformLocation(program, "light_color");
        ambient_light_color_location = glGetUniformLocation(program, "ambient_light_color");

        std::string project_root = PROJECT_ROOT;
        const std::string model_path = project_root + "/models/rose/rose.gltf";

        gltf_model rose = load_gltf(model_path);
        
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, rose.buffer.size(), rose.buffer.data(), GL_STATIC_DRAW);

        translations.resize(rose.meshes.size() / 3, std::vector <glm::vec3> ());
        for (auto &tr : translations) {
            tr.reserve(roses_cnt);
        }

        glGenBuffers(1, &translations_vbo);

        auto setup_part = [&](const gltf_model::mesh &src) -> gltf_mesh {
            gltf_mesh result;

            glGenVertexArrays(1, &result.vao);
            glBindVertexArray(result.vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
            result.indices = src.indices;

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            setup_attribute(0, src.position);
            setup_attribute(1, src.normal);
            if (src.texcoord) {
                setup_attribute(2, src.texcoord.value());
            }

            glBindBuffer(GL_ARRAY_BUFFER, translations_vbo);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
            glVertexAttribDivisor(3, 1);

            result.material = src.material;

            return result;
        };

        for (std::size_t i = 0; i < rose.meshes.size(); i += 3) {
            gltf_mesh leaves = setup_part(rose.meshes[i]);
            gltf_mesh stalk = setup_part(rose.meshes[i + 1]);
            gltf_mesh flower = setup_part(rose.meshes[i + 2]);

            flowers.push_back({leaves, stalk, flower});
            bounds.push_back({
                glm::vec3(
                    std::min({rose.meshes[i].min.x, rose.meshes[i + 1].min.x, rose.meshes[i + 2].min.x}),
                    std::min({rose.meshes[i].min.y, rose.meshes[i + 1].min.y, rose.meshes[i + 2].min.y}),
                    std::min({rose.meshes[i].min.z, rose.meshes[i + 1].min.z, rose.meshes[i + 2].min.z})
                ) * scale,
                glm::vec3(
                    std::max({rose.meshes[i].max.x, rose.meshes[i + 1].max.x, rose.meshes[i + 2].max.x}),
                    std::max({rose.meshes[i].max.y, rose.meshes[i + 1].max.y, rose.meshes[i + 2].max.y}),
                    std::max({rose.meshes[i].max.z, rose.meshes[i + 1].max.z, rose.meshes[i + 2].max.z})
                ) * scale
            });
        }

        for (const auto &flower : flowers) {
            for (const auto &part : flower) {
                if (!part.material.texture_path) {
                    continue;
                }

                if (textures.contains(*part.material.texture_path)) {
                    continue;
                }

                auto path = std::filesystem::path(model_path).parent_path() / *part.material.texture_path;

                textures[*part.material.texture_path] = load_texture(path.string());
            }
        }
    }

    static bool in_bounds(glm::vec3 x, glm::vec3 a, glm::vec3 b) {
        return a.x <= x.x && x.x <= b.x && \
               a.y <= x.y && x.y <= b.y && \
               a.z <= x.z && x.z <= b.z;
    }

    void update_state(float time, float dt, std::map <SDL_Keycode, bool> &button_down) {
        (void)time; (void)dt; (void)button_down;

        for (int i = 1; i < roses_density; i++) {
            for (int j = 1; j < roses_density; j++) {
                if (mask[i][j]) {
                    continue;
                }

                float step = board_size / roses_density * 2;
                glm::vec3 offset(-board_size + i * step, 0.f, -board_size + j * step);

                if (in_bounds(mouse_ptr->position, bounds[0].first + offset - glm::vec3(1.f), bounds[0].second + offset + glm::vec3(1.f))) {
                    roses_by_mouse++;
                    mask[i][j] = true;
                    continue;
                }

                if (in_bounds(papich_ptr->position, bounds[0].first + offset - glm::vec3(.5f), bounds[0].second + offset + glm::vec3(.5f))) {
                    roses_by_player++;
                    mask[i][j] = true;
                    continue;
                }
            }
        }
    }

    void draw(
        const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &camera_position,
        const glm::vec3 &light_direction, const glm::vec3 &light_color, const glm::vec3 &ambient_light_color,
        float time
    ) {
        glm::mat4 model = glm::mat4(1.f);
        model = glm::scale(model, glm::vec3(scale));

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);

        glUseProgram(program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<const float*>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<const float*>(&projection));
        glUniform1i(use_instanced_translation_location, true);
        glUniform3fv(light_direction_location, 1, reinterpret_cast<const float*>(&light_direction));
        glUniform3fv(light_color_location, 1, reinterpret_cast<const float*>(&light_color));
        glUniform3fv(ambient_light_color_location, 1, reinterpret_cast<const float*>(&ambient_light_color));

        frustum fr(projection * view);

        for (int i = 1; i < roses_density; i++) {
            for (int j = 1; j < roses_density; j++) {
                if (mask[i][j]) {
                    continue;
                }

                float step = board_size / roses_density * 2;
                glm::vec3 offset(-board_size + i * step, 0.f, -board_size + j * step);

                float dist = glm::length(camera_position - offset);
                int lod = std::min((int)floor(dist / 4), (int)flowers.size() - 1);

                aabb ab(bounds[lod].first + offset, bounds[lod].second + offset);
                if (intersect(fr, ab)) {
                    translations[lod].push_back(offset / scale);
                }
            }
        }

        glActiveTexture(GL_TEXTURE0);

        for (std::size_t i = 0; i < flowers.size(); i++) {
            const auto &flower = flowers[i];
            
            if (translations[i].empty()) {
                continue;
            }

            glBindBuffer(GL_ARRAY_BUFFER, translations_vbo);
            glBufferData(GL_ARRAY_BUFFER, translations[i].size() * sizeof(glm::vec3), translations[i].data(), GL_STATIC_DRAW);

            for (const auto &part : flower) {
                if (part.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                if (part.material.texture_path) {
                    glBindTexture(GL_TEXTURE_2D, textures[*part.material.texture_path]);
                    glUniform1i(use_texture_location, 1);
                    glUniform1i(albedo_location, 0);
                } else if (part.material.color) {
                    glUniform1i(use_texture_location, 0);
                    glUniform4fv(color_location, 1, reinterpret_cast<const float*>(&(*part.material.color)));
                } else {
                    continue;
                }

                glBindVertexArray(part.vao);
                glDrawElementsInstanced(GL_TRIANGLES, part.indices.count, part.indices.type, reinterpret_cast<void*>(part.indices.view.offset), translations[i].size());
            }
        }

        auto count_instances = [&]() {
            int drawn_cnt = 0;
            for (int i = 0; i + 1 < flowers.size(); i++) {
                std::cerr << translations[i].size() << " + ";
                drawn_cnt += translations[i].size();
            }
            std::cerr << translations.back().size();
            drawn_cnt += translations.back().size();
            
            std::cerr << " = " << drawn_cnt << std::endl;
        };

        // count_instances(); // used for debug

        for (auto &tr : translations) {
            tr.clear();
        }

        // draw LODs for demonstration
        glUniform1i(use_instanced_translation_location, false);

        model = glm::translate(model, glm::vec3(board_size + 1.f, 10.f, -2.f) / scale);
        for (std::size_t i = 0; i < flowers.size(); i++) {
            const auto &flower = flowers[i];

            model = glm::translate(model, glm::vec3(0.f, 0.f, 1.f) / scale);
            glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<const float*>(&model));

            for (const auto &part : flower) {
                if (part.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                if (part.material.texture_path) {
                    glBindTexture(GL_TEXTURE_2D, textures[*part.material.texture_path]);
                    glUniform1i(use_texture_location, 1);
                    glUniform1i(albedo_location, 0);
                } else if (part.material.color) {
                    glUniform1i(use_texture_location, 0);
                    glUniform4fv(color_location, 1, reinterpret_cast<const float*>(&(*part.material.color)));
                } else {
                    continue;
                }

                glBindVertexArray(part.vao);
                glDrawElements(GL_TRIANGLES, part.indices.count, part.indices.type, reinterpret_cast<void*>(part.indices.view.offset));
            }
        }
    }
};

}