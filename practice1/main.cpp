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

const char kek[4] = "kek";

GLuint create_shader(GLenum shader_type, const char *shader_source) {
	GLuint sh = glCreateShader(shader_type);

	const GLchar * sources[1] = { shader_source };
	GLint len[1] = { (GLint)strlen(shader_source) };

	glShaderSource(sh, 1, sources, len);
	glCompileShader(sh);

	GLint result;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		GLchar info_log[1024] = "Shader compilation failed!\n";
		GLint log_len;

		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_len);
		glGetShaderInfoLog(sh, 1000, NULL, info_log + strlen(info_log));

		throw std::runtime_error(info_log);
	}

	return sh;
}

const char fragment_source[] = R"(
#version 330 core
layout (location = 0) out vec4 out_color;
in vec3 color;
in vec2 pos;
void main()
{
	// task 9
	float x = pos[0], y = pos[1];
	int C = 17;
	float col = float((int(floor(x * C)) + int(floor(y * C))) % 2);
	out_color = vec4(col, col, col, 1.0);

	// tasks 6, 7
	//out_color = vec4(color, 1.0);
}
)";

const char vertex_source[] = R"(
#version 330 core
const vec2 VERTICES[3] = vec2[3](
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0)
);
out vec3 color;
out vec2 pos;
void main()
{
	gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);

	// task 7
	//color = vec3(gl_VertexID / 2.0, 0.0, (2 - gl_VertexID) / 2.0);
	
	// task 9
	pos = VERTICES[gl_VertexID];
}
)";

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
	GLuint p = glCreateProgram();

	glAttachShader(p, vertex_shader);
	glAttachShader(p, fragment_shader);
	glLinkProgram(p);

	GLint result;
        glGetProgramiv(p, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
                GLchar info_log[1024] = "Program linking failed!\n";
                GLint log_len;

                glGetProgramiv(p, GL_INFO_LOG_LENGTH, &log_len);
                glGetProgramInfoLog(p, 1000, NULL, info_log + strlen(info_log));

                throw std::runtime_error(info_log);
        }
	
	return p;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 1",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    // tasks 1, 2
    // create_shader(GL_FRAGMENT_SHADER, kek);

    // tasks 3, 4, 5
    GLuint fsh = create_shader(GL_FRAGMENT_SHADER, fragment_source);
    GLuint vsh = create_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint prog = create_program(vsh, fsh);

    // task 8
    //glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    //glProvokingVertex(GL_LAST_VERTEX_CONVENTION);

    // tasks 6, 7
    GLuint arr;
    glGenVertexArrays(1, &arr);

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        }

        if (!running)
            break;

        glClear(GL_COLOR_BUFFER_BIT);

	// tasks 6, 7
	glUseProgram(prog);
	glBindVertexArray(arr);
	glDrawArrays(GL_TRIANGLES, 0, 3);

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
