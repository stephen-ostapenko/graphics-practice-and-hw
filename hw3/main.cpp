#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <random>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "obj_parser.hpp"
#include "stb_image.h"
#include "gltf_loader.hpp"

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

// ========================================================================================================

const char sphere_vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_tangent;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_texcoord;

out vec3 position;
out vec3 tangent;
out vec3 normal;
out vec2 texcoord;

void main()
{
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    tangent = mat3(model) * in_tangent;
    normal = mat3(model) * in_normal;
    texcoord = in_texcoord;
}
)";

const char sphere_fragment_shader_source[] =
R"(#version 330 core

uniform vec3 light_direction;
uniform vec3 camera_position;

uniform sampler2D albedo_texture;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

void main()
{
    float ambient_light = 0.2;

    float lightness = ambient_light + max(0.0, dot(normalize(normal), light_direction));

    vec3 albedo = texture(albedo_texture, texcoord).rgb;

    //out_color = vec4(lightness * albedo, 1.0);
    //out_color = vec4(texcoord, 0.0, 1.0);

    if (abs(position.y) < 1e-6) {
        out_color = vec4(vec3(0.5 + 0.5 * sqrt(position.x * position.x + position.z * position.z)), 1.0);
    } else {
        out_color = vec4(vec3(0.5), 1.0);
    }
}
)";

// ========================================================================================================

const char statmodel_vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

out vec3 normal;
out vec2 texcoord;

void main()
{
    gl_Position = projection * view * model * vec4(in_position, 1.0);
    normal = mat3(model) * in_normal;
    texcoord = in_texcoord;
}
)";

const char statmodel_fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo;
uniform vec4 color;

uniform vec3 light_direction;

layout (location = 0) out vec4 out_color;

in vec3 normal;
in vec2 texcoord;

void main()
{
    vec4 albedo_color = texture(albedo, texcoord);

    float ambient = 0.4;
    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color.rgb * (ambient + diffuse), albedo_color.a);
}
)";

// ========================================================================================================

const char animodel_vertex_shader_source[] =
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

out vec3 normal;
out vec2 texcoord;

out vec4 weights;

void main()
{
    weights = in_weights;

    mat4x3 average = bones[in_joints.x] * weights.x + \
                     bones[in_joints.y] * weights.y + \
                     bones[in_joints.z] * weights.z + \
                     bones[in_joints.w] * weights.w;

    average /= weights.x + weights.y + weights.z + weights.w;

    gl_Position = projection * view * model * mat4(average) * vec4(in_position, 1.0);
    normal = mat3(model) * mat3(average) * in_normal;
    texcoord = in_texcoord;
}
)";

const char animodel_fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D albedo;
uniform vec4 color;
uniform int use_texture;

uniform vec3 light_direction;

layout (location = 0) out vec4 out_color;

in vec3 normal;
in vec2 texcoord;

in vec4 weights;

void main()
{
    vec4 albedo_color;

    if (use_texture == 1)
        albedo_color = texture(albedo, texcoord);
    else
        albedo_color = color;

    float ambient = 0.4;
    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color.rgb * (ambient + diffuse), albedo_color.a);
}
)";

// ========================================================================================================

const char particle_vertex_shader_source[] =
R"(#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in float size;
layout (location = 2) in float angle;

out float fig_size;
out float fig_angle;

void main()
{
    gl_Position = vec4(in_position, 1.0);
    fig_size = size;
    fig_angle = angle;
}
)";

const char particle_geometry_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_position;

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in float fig_size[];
in float fig_angle[];

out vec2 texcoord;

void main()
{
    vec3 center = gl_in[0].gl_Position.xyz;
    mat4 PVM = projection * view * model;

    vec3 camera_direction = camera_position - center;
    vec3 Y = vec3(0.0, 1.0, 0.0);
    vec3 X = cross(Y, camera_direction);
    Y = cross(camera_direction, X);
    X = normalize(X), Y = normalize(Y), camera_direction = normalize(camera_direction);

    float x = camera_direction.x, y = camera_direction.y, z = camera_direction.z, a = fig_angle[0];
    mat3 ROT = mat3(
        cos(a) + (1 - cos(a)) * x * x, (1 - cos(a)) * x * y - sin(a) * z, (1 - cos(a)) * x * z + sin(a) * y,
        (1 - cos(a)) * y * x + sin(a) * z, cos(a) + (1 - cos(a)) * y * y, (1 - cos(a)) * y * z - sin(a) * x,
        (1 - cos(a)) * z * x - sin(a) * y, (1 - cos(a)) * z * y + sin(a) * x, cos(a) + (1 - cos(a)) * z * z
    );

    X = ROT * X; Y = ROT * Y;

    gl_Position = PVM * vec4(center + (-X - Y) * fig_size[0], 1.0);
    texcoord = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = PVM * vec4(center + (-X + Y) * fig_size[0], 1.0);
    texcoord = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = PVM * vec4(center + (X - Y) * fig_size[0], 1.0);
    texcoord = vec2(1.0, 0.0);
    EmitVertex();

    gl_Position = PVM * vec4(center + (X + Y) * fig_size[0], 1.0);
    texcoord = vec2(1.0, 1.0);
    EmitVertex();

    EndPrimitive();
}

)";

const char particle_fragment_shader_source[] =
R"(#version 330 core

uniform sampler2D tex;

layout (location = 0) out vec4 out_color;

in vec2 texcoord;

void main()
{
    float a = texture(tex, texcoord).r;
    out_color = vec4(vec3(1.0), a);
}
)";

// ========================================================================================================

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

template <typename ... Shaders>
GLuint create_program(Shaders ... shaders)
{
    GLuint result = glCreateProgram();
    (glAttachShader(result, shaders), ...);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vertex
{
    glm::vec3 position;
    glm::vec3 tangent;
    glm::vec3 normal;
    glm::vec2 texcoords;
};

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> generate_sphere(float radius, int quality)
{
    std::vector<vertex> vertices;

    for (int latitude = -quality; latitude <= 0; ++latitude)
    {
        for (int longitude = 0; longitude <= 4 * quality; ++longitude)
        {
            float lat = (latitude * glm::pi<float>()) / (2.f * quality);
            float lon = (longitude * glm::pi<float>()) / (2.f * quality);

            auto & vertex = vertices.emplace_back();
            vertex.normal = {std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)};
            vertex.position = vertex.normal * radius;
            vertex.tangent = {-std::cos(lat) * std::sin(lon), 0.f, std::cos(lat) * std::cos(lon)};
            vertex.texcoords.x = (longitude * 1.f) / (4.f * quality);
            vertex.texcoords.y = (latitude * 1.f) / (2.f * quality) + 0.5f;
        }
    }

    std::vector<std::uint32_t> indices;

    for (int latitude = 0; latitude < 2 * quality; ++latitude)
    {
        for (int longitude = 0; longitude < 4 * quality; ++longitude)
        {
            std::uint32_t i0 = (latitude + 0) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i1 = (latitude + 1) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i2 = (latitude + 0) * (4 * quality + 1) + (longitude + 1);
            std::uint32_t i3 = (latitude + 1) * (4 * quality + 1) + (longitude + 1);

            indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
        }
    }

    return {std::move(vertices), std::move(indices)};
}

GLuint load_texture(std::string const & path)
{
    int width, height, channels;
    auto pixels = stbi_load(path.data(), &width, &height, &channels, 4);

    GLuint result;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);

    return result;
}

std::default_random_engine rng;

struct particle
{
    glm::vec3 position = glm::vec3(
        std::uniform_real_distribution<float>{-1.f, 1.f}(rng),
        1.5f,
        std::uniform_real_distribution<float>{-1.f, 1.f}(rng)
    );

    float size = std::uniform_real_distribution<float>{.005f, .02f}(rng);

    glm::vec3 velocity = glm::vec3(
        std::uniform_real_distribution<float>{-.1f, .1f}(rng),
        std::uniform_real_distribution<float>{-.1f, 0.f}(rng),
        std::uniform_real_distribution<float>{-.1f, .1f}(rng)
    );

    float rotation_angle = 0;
    float angular_velocity = std::uniform_real_distribution<float>{0.f, .01f}(rng);
};

int main() try
{
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

    SDL_Window * window = SDL_CreateWindow("Graphics course hw3",
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

    // ========================================================================================================

    auto sphere_vertex_shader = create_shader(GL_VERTEX_SHADER, sphere_vertex_shader_source);
    auto sphere_fragment_shader = create_shader(GL_FRAGMENT_SHADER, sphere_fragment_shader_source);
    auto sphere_program = create_program(sphere_vertex_shader, sphere_fragment_shader);

    GLuint sphere_model_location = glGetUniformLocation(sphere_program, "model");
    GLuint sphere_view_location = glGetUniformLocation(sphere_program, "view");
    GLuint sphere_projection_location = glGetUniformLocation(sphere_program, "projection");
    GLuint sphere_light_direction_location = glGetUniformLocation(sphere_program, "light_direction");
    GLuint sphere_camera_position_location = glGetUniformLocation(sphere_program, "camera_position");
    GLuint sphere_albedo_texture_location = glGetUniformLocation(sphere_program, "albedo_texture");

    GLuint sphere_vao, sphere_vbo, sphere_ebo;
    glGenVertexArrays(1, &sphere_vao);
    glBindVertexArray(sphere_vao);
    glGenBuffers(1, &sphere_vbo);
    glGenBuffers(1, &sphere_ebo);
    GLuint sphere_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f, 16);

        glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        sphere_index_count = indices.size();
    }
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, tangent));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, texcoords));

    std::string project_root = PROJECT_ROOT;
    GLuint sphere_albedo_texture = load_texture(project_root + "/textures/brick_albedo.jpg");

    // ========================================================================================================

    /*auto statmodel_vertex_shader = create_shader(GL_VERTEX_SHADER, statmodel_vertex_shader_source);
    auto statmodel_fragment_shader = create_shader(GL_FRAGMENT_SHADER, statmodel_fragment_shader_source);
    auto statmodel_program = create_program(statmodel_vertex_shader, statmodel_fragment_shader);

    GLuint statmodel_model_location = glGetUniformLocation(statmodel_program, "model");
    GLuint statmodel_view_location = glGetUniformLocation(statmodel_program, "view");
    GLuint statmodel_projection_location = glGetUniformLocation(statmodel_program, "projection");
    GLuint statmodel_albedo_location = glGetUniformLocation(statmodel_program, "albedo");
    GLuint statmodel_color_location = glGetUniformLocation(statmodel_program, "color");
    GLuint statmodel_use_texture_location = glGetUniformLocation(statmodel_program, "use_texture");
    GLuint statmodel_light_direction_location = glGetUniformLocation(statmodel_program, "light_direction");

    const std::string model_path = project_root + "/frank/scene.gltf";

    auto const input_model = load_gltf(model_path);
    GLuint statmodel_vbo;
    glGenBuffers(1, &statmodel_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, statmodel_vbo);
    glBufferData(GL_ARRAY_BUFFER, input_model.buffer.size(), input_model.buffer.data(), GL_STATIC_DRAW);

    struct mesh
    {
        GLuint vao;
        gltf_model::accessor indices;
        gltf_model::material material;
    };

    auto setup_attribute = [](int index, gltf_model::accessor const & accessor, bool integer = false)
    {
        glEnableVertexAttribArray(index);
        if (integer)
            glVertexAttribIPointer(index, accessor.size, accessor.type, 0, reinterpret_cast<void *>(accessor.view.offset));
        else
            glVertexAttribPointer(index, accessor.size, accessor.type, GL_FALSE, 0, reinterpret_cast<void *>(accessor.view.offset));
    };

    std::vector<mesh> meshes;
    for (auto const & mesh : input_model.meshes)
    {
        auto & result = meshes.emplace_back();
        glGenVertexArrays(1, &result.vao);
        glBindVertexArray(result.vao);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, statmodel_vbo);
        result.indices = mesh.indices;

        setup_attribute(0, mesh.position);
        setup_attribute(1, mesh.normal);
        setup_attribute(2, mesh.texcoord);
        setup_attribute(3, mesh.joints, true);
        setup_attribute(4, mesh.weights);

        result.material = mesh.material;
    }

    std::map<std::string, GLuint> statmodel_textures;
    for (auto const & mesh : meshes)
    {
        if (!mesh.material.texture_path) continue;
        if (statmodel_textures.contains(*mesh.material.texture_path)) continue;

        auto path = std::filesystem::path(model_path).parent_path() / *mesh.material.texture_path;

        int width, height, channels;
        auto data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        assert(data);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);

        statmodel_textures[*mesh.material.texture_path] = texture;
    }*/

    // ========================================================================================================

    auto animodel_vertex_shader = create_shader(GL_VERTEX_SHADER, animodel_vertex_shader_source);
    auto animodel_fragment_shader = create_shader(GL_FRAGMENT_SHADER, animodel_fragment_shader_source);
    auto animodel_program = create_program(animodel_vertex_shader, animodel_fragment_shader);

    GLuint animodel_model_location = glGetUniformLocation(animodel_program, "model");
    GLuint animodel_view_location = glGetUniformLocation(animodel_program, "view");
    GLuint animodel_projection_location = glGetUniformLocation(animodel_program, "projection");
    GLuint animodel_albedo_location = glGetUniformLocation(animodel_program, "albedo");
    GLuint animodel_color_location = glGetUniformLocation(animodel_program, "color");
    GLuint animodel_use_texture_location = glGetUniformLocation(animodel_program, "use_texture");
    GLuint animodel_light_direction_location = glGetUniformLocation(animodel_program, "light_direction");

    GLuint animodel_bones_location = glGetUniformLocation(animodel_program, "bones");

    const std::string animodel_model_path = project_root + "/wolf/Wolf-Blender-2.82a.gltf";

    auto const animodel_input_model = load_gltf(animodel_model_path);
    GLuint animodel_vbo;
    glGenBuffers(1, &animodel_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, animodel_vbo);
    glBufferData(GL_ARRAY_BUFFER, animodel_input_model.buffer.size(), animodel_input_model.buffer.data(), GL_STATIC_DRAW);

    /*
    */
    struct mesh
    {
        GLuint vao;
        gltf_model::accessor indices;
        gltf_model::material material;
    };

    auto setup_attribute = [](int index, gltf_model::accessor const & accessor, bool integer = false)
    {
        glEnableVertexAttribArray(index);
        if (integer)
            glVertexAttribIPointer(index, accessor.size, accessor.type, 0, reinterpret_cast<void *>(accessor.view.offset));
        else
            glVertexAttribPointer(index, accessor.size, accessor.type, GL_FALSE, 0, reinterpret_cast<void *>(accessor.view.offset));
    };

    std::vector<mesh> animodel_meshes;
    for (auto const & mesh : animodel_input_model.meshes)
    {
        auto & result = animodel_meshes.emplace_back();
        glGenVertexArrays(1, &result.vao);
        glBindVertexArray(result.vao);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, animodel_vbo);
        result.indices = mesh.indices;

        setup_attribute(0, mesh.position);
        setup_attribute(1, mesh.normal);
        setup_attribute(2, mesh.texcoord);
        setup_attribute(3, mesh.joints, true);
        setup_attribute(4, mesh.weights);

        result.material = mesh.material;
    }

    std::map<std::string, GLuint> animodel_textures;
    for (auto const & mesh : animodel_meshes)
    {
        if (!mesh.material.texture_path) continue;
        if (animodel_textures.contains(*mesh.material.texture_path)) continue;

        auto path = std::filesystem::path(animodel_model_path).parent_path() / *mesh.material.texture_path;

        int width, height, channels;
        auto data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        assert(data);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);

        animodel_textures[*mesh.material.texture_path] = texture;
    }

    // ========================================================================================================

    auto particle_vertex_shader = create_shader(GL_VERTEX_SHADER, particle_vertex_shader_source);
    auto particle_geometry_shader = create_shader(GL_GEOMETRY_SHADER, particle_geometry_shader_source);
    auto particle_fragment_shader = create_shader(GL_FRAGMENT_SHADER, particle_fragment_shader_source);
    auto particle_program = create_program(particle_vertex_shader, particle_geometry_shader, particle_fragment_shader);

    GLuint particle_model_location = glGetUniformLocation(particle_program, "model");
    GLuint particle_view_location = glGetUniformLocation(particle_program, "view");
    GLuint particle_projection_location = glGetUniformLocation(particle_program, "projection");
    GLuint particle_camera_position_location = glGetUniformLocation(particle_program, "camera_position");

    std::vector<particle> particles;

    GLuint particle_vao, particle_vbo;
    glGenVertexArrays(1, &particle_vao);
    glBindVertexArray(particle_vao);

    glGenBuffers(1, &particle_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, particle_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(particle), (void*)offsetof(particle, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void*)offsetof(particle, size));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void*)offsetof(particle, rotation_angle));

    const std::string particle_texture_path = project_root + "/textures/particle.png";
    int tex_width, tex_height, col_ch_cnt;
    unsigned char *tex_data = stbi_load(particle_texture_path.c_str(), &tex_width, &tex_height, &col_ch_cnt, 4);
    
    GLuint particle_texture;
    glGenTextures(1, &particle_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, particle_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(tex_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // ========================================================================================================

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    float view_elevation = glm::radians(30.f);
    float view_azimuth = 0.f;
    float camera_distance = 2.f;

    float interp_param = 0.f;
    bool paused = false;

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            if (event.key.keysym.sym == SDLK_SPACE)
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

        if (button_down[SDLK_w])
            view_elevation += 1.f * dt;
        if (button_down[SDLK_s])
            view_elevation -= 1.f * dt;

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

        glm::vec3 light_direction = glm::normalize(glm::vec3(1.f, 2.f, 3.f));

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        // ========================================================================================================

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ========================================================================================================

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(sphere_program);
        glUniformMatrix4fv(sphere_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(sphere_view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(sphere_projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(sphere_light_direction_location, 1, reinterpret_cast<float *>(&light_direction));
        glUniform3fv(sphere_camera_position_location, 1, reinterpret_cast<float *>(&camera_position));
        glUniform1i(sphere_albedo_texture_location, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sphere_albedo_texture);

        glBindVertexArray(sphere_vao);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, nullptr);

        // ========================================================================================================

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::vector <glm::mat4x3> bones(animodel_input_model.bones.size());

        auto run_animation = animodel_input_model.animations.at("01_Run");
        auto walk_animation = animodel_input_model.animations.at("02_walk");

        for (int i = 0; i < bones.size(); i++) {
            glm::mat4 translation = glm::translate(
                glm::mat4(1.f),
                glm::lerp(
                    walk_animation.bones[i].translation(std::fmod(time, walk_animation.max_time)),
                    run_animation.bones[i].translation(std::fmod(time, run_animation.max_time)),
                    interp_param
                )
            );
            glm::mat4 scale = glm::scale(
                glm::mat4(1.f),
                glm::lerp(
                    walk_animation.bones[i].scale(std::fmod(time, walk_animation.max_time)),
                    run_animation.bones[i].scale(std::fmod(time, run_animation.max_time)),
                    interp_param
                )
            );
            glm::mat4 rotation = glm::toMat4(
                glm::slerp(
                    walk_animation.bones[i].rotation(std::fmod(time, walk_animation.max_time)),
                    run_animation.bones[i].rotation(std::fmod(time, run_animation.max_time)),
                    interp_param
                )
            );

            glm::mat4 transform = translation * rotation * scale;

            if (animodel_input_model.bones[i].parent != -1) {
                transform = bones[animodel_input_model.bones[i].parent] * transform;
            }

            bones[i] = transform;
        }
        for (int i = 0; i < bones.size(); i++) {
            bones[i] = bones[i] * animodel_input_model.bones[i].inverse_bind_matrix;
        }

        float animodel_scale = 0.5;
        float animodel_speed = 0.25;

        model = glm::scale(model, glm::vec3(animodel_scale));
        model = glm::translate(model, 1.5f * glm::vec3(sin(time * animodel_speed), 0, cos(time * animodel_speed)));
        model = glm::rotate(model, time * animodel_speed + glm::pi<float>() / 2.f, {0, 1, 0});

        glUseProgram(animodel_program);
        glUniformMatrix4fv(animodel_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(animodel_view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(animodel_projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(animodel_light_direction_location, 1, reinterpret_cast<float *>(&light_direction));

        glUniformMatrix4x3fv(animodel_bones_location, bones.size(), GL_FALSE, reinterpret_cast<float *>(bones.data()));

        auto draw_meshes = [&](bool transparent)
        {
            for (auto const & mesh : animodel_meshes)
            {
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

                if (mesh.material.texture_path)
                {
                    glBindTexture(GL_TEXTURE_2D, animodel_textures[*mesh.material.texture_path]);
                    glUniform1i(animodel_use_texture_location, 1);
                }
                else if (mesh.material.color)
                {
                    glUniform1i(animodel_use_texture_location, 0);
                    glUniform4fv(animodel_color_location, 1, reinterpret_cast<const float *>(&(*mesh.material.color)));
                }
                else
                    continue;

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indices.count, mesh.indices.type, reinterpret_cast<void *>(mesh.indices.view.offset));
            }
        };

        draw_meshes(false);
        glDepthMask(GL_FALSE);
        draw_meshes(true);
        glDepthMask(GL_TRUE);

        // ========================================================================================================


        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        model = glm::mat4(1.f);

        int particles_max_cnt = 1024;
        float g = 0.02;
        if (!paused) {
            if (particles.size() < particles_max_cnt) {
                particles.push_back(particle());
            }

            for (auto &it : particles) {
                it.velocity.y -= g * dt;
                it.position += it.velocity * dt;

                it.rotation_angle += it.angular_velocity;

                if (it.position.y <= 0.f) {
                    it = particle();
                }
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, particle_vbo);
        glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(particle), particles.data(), GL_STATIC_DRAW);

        glUseProgram(particle_program);

        glUniformMatrix4fv(particle_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(particle_view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(particle_projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(particle_camera_position_location, 1, reinterpret_cast<float *>(&camera_position));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, particle_texture);

        glBindVertexArray(particle_vao);
        glDrawArrays(GL_POINTS, 0, particles.size());

        // ========================================================================================================

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
