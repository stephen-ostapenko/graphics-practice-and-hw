#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>

#include <GL/glew.h>

#include <assimp/scene.h>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>

using std::uint32_t;

struct mesh {
    struct vertex {
        std::array <float, 3> position;
        std::array <float, 3> normal;
        std::array <float, 3> texcoord;
    };

    struct texture_params {
        GLuint albedo_tex;
        GLuint opacity_tex;
        glm::vec3 glossiness;
        float power;

        texture_params(GLuint a_tex, GLuint o_tex, glm::vec3 gloss, float pwr) {
            albedo_tex = a_tex;
            opacity_tex = o_tex;
            glossiness = gloss;
            power = pwr;
        }
    };

    std::vector <vertex> vertices;
    std::vector <uint32_t> indices;

    GLuint vao, vbo, ebo;
    uint32_t material_id;

    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;

    float x_size, y_size, z_size;

    mesh(aiMesh *src = nullptr) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        if (src) {
            init(src);
        }
    }

    void init(aiMesh *src) {
        uint32_t n = src->mNumVertices;
        vertices.resize(n);

        min_x = std::numeric_limits <float>::infinity(); max_x = -std::numeric_limits <float>::infinity();
        min_y = std::numeric_limits <float>::infinity(); max_y = -std::numeric_limits <float>::infinity();
        min_z = std::numeric_limits <float>::infinity(); max_z = -std::numeric_limits <float>::infinity();

        for (uint32_t i = 0; i < n; i++) {
            vertices[i].position[0] = src->mVertices[i].x;
            vertices[i].position[1] = src->mVertices[i].y;
            vertices[i].position[2] = src->mVertices[i].z;

            vertices[i].normal[0] = src->mNormals[i].x;
            vertices[i].normal[1] = src->mNormals[i].y;
            vertices[i].normal[2] = src->mNormals[i].z;

            if (src->HasTextureCoords(0)) {
                vertices[i].texcoord[0] = src->mTextureCoords[0][i].x;
                vertices[i].texcoord[1] = src->mTextureCoords[0][i].y;
                vertices[i].texcoord[2] = src->mTextureCoords[0][i].z;
            }
        }

        auto cmp = [](int coord) {
            return [=](vertex &a, vertex &b) {
                return a.position[coord] < b.position[coord];
            };
        };

        min_x = std::min(min_x, std::min_element(vertices.begin(), vertices.end(), cmp(0))->position[0]);
        max_x = std::max(max_x, std::max_element(vertices.begin(), vertices.end(), cmp(0))->position[0]);

        min_y = std::min(min_y, std::min_element(vertices.begin(), vertices.end(), cmp(1))->position[1]);
        max_y = std::max(max_y, std::max_element(vertices.begin(), vertices.end(), cmp(1))->position[1]);

        min_z = std::min(min_z, std::min_element(vertices.begin(), vertices.end(), cmp(2))->position[2]);
        max_z = std::max(max_z, std::max_element(vertices.begin(), vertices.end(), cmp(2))->position[2]);

        x_size = max_x - min_x;
        y_size = max_y - min_y;
        z_size = max_z - min_z;

        indices.clear();
        for (uint32_t i = 0; i < src->mNumFaces; i++) {
            assert(src->mFaces[i].mNumIndices == 3);

            indices.push_back(src->mFaces[i].mIndices[0]);
            indices.push_back(src->mFaces[i].mIndices[1]);
            indices.push_back(src->mFaces[i].mIndices[2]);
        }

        material_id = src->mMaterialIndex;

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), (void*)(offsetof(mesh::vertex, mesh::vertex::position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), (void*)(offsetof(mesh::vertex, mesh::vertex::normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), (void*)(offsetof(mesh::vertex, mesh::vertex::texcoord)));
    }

    void draw(const std::vector <texture_params> &tex_params, GLuint glossiness_location, GLuint power_location) {
        glBindVertexArray(vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_params[material_id].albedo_tex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex_params[material_id].opacity_tex);

        glUniform3f(
            glossiness_location,
            tex_params[material_id].glossiness.x,
            tex_params[material_id].glossiness.y,
            tex_params[material_id].glossiness.z
        );

        glUniform1f(power_location, tex_params[material_id].power);

        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    }
};