#pragma once

#include "mesh.h"
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

#include <GL/glew.h>

#include <assimp/scene.h>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/ext/scalar_constants.hpp>

struct scene {
    std::vector <mesh> objects;
    std::vector <mesh::texture_params> tex_params;

    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;

    float x_size, y_size, z_size;
    float min_size, max_size, mean_size;

    float camera_x = 0, camera_y = 0, camera_z = 0;
    float camera_xz_angle = 0, camera_yz_angle = 0;
    float velocity;
    float near, far;

    scene(const aiScene *src, std::string object_path) {
        min_x = std::numeric_limits <float>::infinity(); max_x = -std::numeric_limits <float>::infinity();
        min_y = std::numeric_limits <float>::infinity(); max_y = -std::numeric_limits <float>::infinity();
        min_z = std::numeric_limits <float>::infinity(); max_z = -std::numeric_limits <float>::infinity();

        for (uint32_t i = 0; i < src->mNumMeshes; i++) {
            mesh obj(src->mMeshes[i]);
            objects.push_back(obj);

            min_x = std::min(min_x, obj.min_x); max_x = std::max(max_x, obj.max_x);
            min_y = std::min(min_y, obj.min_y); max_y = std::max(max_y, obj.max_y);
            min_z = std::min(min_z, obj.min_z); max_z = std::max(max_z, obj.max_z);
        }

        x_size = max_x - min_x;
        y_size = max_y - min_y;
        z_size = max_z - min_z;

        min_size = get_min_size();
        max_size = get_max_size();
        mean_size = get_mean_size();

        velocity = mean_size / 5;
        near = min_size / 10;
        far = max_size * 10;

        int CHANNELS[] = {0, GL_RED, GL_RG, GL_RGB, GL_RGBA};

        std::cerr << "trying to load " << src->mNumMaterials << " materials" << std::endl;
        for (int i = 0; i < src->mNumMaterials; i++) {
            aiString tmp;

            src->mMaterials[i]->GetTexture(aiTextureType_AMBIENT, 0, &tmp);
            std::string albedo_path(tmp.C_Str());
            std::replace(albedo_path.begin(), albedo_path.end(), '\\', '/');
            albedo_path = object_path + "/" + albedo_path;
            int width, height, ch_cnt;
            unsigned char *albedo_data = stbi_load(albedo_path.c_str(), &width, &height, &ch_cnt, 0);

            glActiveTexture(GL_TEXTURE0);
            GLuint albedo_tex;
            glGenTextures(1, &albedo_tex);
            if (albedo_data != nullptr) {
                if (src->mMaterials[i]->GetTextureCount(aiTextureType_AMBIENT) == 0) {
                    memset(albedo_data, 255, width * height);
                    std::cerr << "ambient texture for " << albedo_path << " is missing" << std::endl;
                } else {
                    std::cerr << "loaded texture "
                          << width << "x" << height << " with " << ch_cnt << " channels from "
                          << albedo_path << std::endl;
                }

                glBindTexture(GL_TEXTURE_2D, albedo_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, CHANNELS[ch_cnt], GL_UNSIGNED_BYTE, albedo_data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glGenerateMipmap(GL_TEXTURE_2D);
            } else {
                std::cerr << "ambient texture for " << albedo_path << " is missing" << std::endl;
            }
            stbi_image_free(albedo_data);

            src->mMaterials[i]->GetTexture(aiTextureType_OPACITY, 0, &tmp);
            std::string opacity_path(tmp.C_Str());
            std::replace(opacity_path.begin(), opacity_path.end(), '\\', '/');
            opacity_path = object_path + "/" + opacity_path;
            unsigned char *opacity_data = stbi_load(opacity_path.c_str(), &width, &height, &ch_cnt, 0);

            glActiveTexture(GL_TEXTURE1);
            GLuint opacity_tex;
            glGenTextures(1, &opacity_tex);
            if (opacity_data != nullptr) {
                if (src->mMaterials[i]->GetTextureCount(aiTextureType_OPACITY) == 0) {
                    memset(opacity_data, 255, width * height);
                    std::cerr << "opacity texture for " << opacity_path << " is missing" << std::endl;
                } else {
                    std::cerr << "loaded texture "
                          << width << "x" << height << " with " << ch_cnt << " channels from "
                          << opacity_path << std::endl;
                }

                glBindTexture(GL_TEXTURE_2D, opacity_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, CHANNELS[ch_cnt], GL_UNSIGNED_BYTE, opacity_data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            } else {
                std::cerr << "opacity texture for " << opacity_path << " is missing" << std::endl;
            }
            stbi_image_free(opacity_data);

            aiColor4D specular;
            src->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, specular);

            float power;
            src->mMaterials[i]->Get(AI_MATKEY_SHININESS, power);

            tex_params.push_back(mesh::texture_params(
                albedo_tex,
                opacity_tex,
                glm::vec3(specular.r, specular.g, specular.b),
                power
            ));
        }
    }

    void draw(GLuint glossiness_location, GLuint power_location) {
        for (auto &it : objects) {
            it.draw(tex_params, glossiness_location, power_location);
        }
    }

    float get_min_size() {
        return std::min({x_size, y_size, z_size});
    }

    float get_max_size() {
        return std::max({x_size, y_size, z_size});
    }

    float get_mean_size() {
        return (x_size + y_size + z_size) / 3.0f;
    }

    void move_camera_forward(float t) {
        camera_x -= sin(camera_xz_angle) * t * velocity;
        camera_z += cos(camera_xz_angle) * t * velocity;
    }

    void move_camera_backward(float t) {
        camera_x += sin(camera_xz_angle) * t * velocity;
        camera_z -= cos(camera_xz_angle) * t * velocity;
    }

    void move_camera_left(float t) {
        camera_x -= sin(camera_xz_angle - glm::pi<float>() / 2) * t * velocity;
        camera_z += cos(camera_xz_angle - glm::pi<float>() / 2) * t * velocity;
    }

    void move_camera_right(float t) {
        camera_x -= sin(camera_xz_angle + glm::pi<float>() / 2) * t * velocity;
        camera_z += cos(camera_xz_angle + glm::pi<float>() / 2) * t * velocity;
    }

    void move_camera_up(float t) {
        camera_y -= t * velocity;
    }

    void move_camera_down(float t) {
        camera_y += t * velocity;
    }

    void turn_camera_left(float t) {
        camera_xz_angle -= 2.f * t;
    }

    void turn_camera_right(float t) {
        camera_xz_angle += 2.f * t;
    }

    void turn_camera_up(float t) {
        camera_yz_angle -= 2.f * t;
        camera_yz_angle = std::max(camera_yz_angle, -glm::pi<float>() / 2);
    }

    void turn_camera_down(float t) {
        camera_yz_angle += 2.f * t;
        camera_yz_angle = std::min(camera_yz_angle, glm::pi<float>() / 2);
    }

    void slow_down() {
        velocity = mean_size / 24;
    }

    void speed_up() {
        velocity = mean_size / 5;
    }
};