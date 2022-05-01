#include <stdio.h>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Defines.hpp"
#include "Helpers.hpp"
#include "Camera.hpp"

constexpr uint32_t VIEWER_WIDTH  = 500u;
constexpr uint32_t VIEWER_HEIGHT = 500u;
constexpr uint32_t PATCH_RESOLUTION = 64u;
constexpr uint32_t CLIPMAP_LEVELS = 5u;

enum
{
    PROGRAM_DEFAULT = 0,
    PROGRAM_COUNT
};

enum
{
    TEXTURE_COUNT
};

enum 
{
    VERTEXARRAY_PATCH = 0,
    VERTEXARRAY_COUNT
};

enum 
{
    BUFFER_VERTEX_PATCH = 0,
    BUFFER_COUNT
};

struct OpenGLManager {
    GLuint programs[PROGRAM_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
} g_gl;

struct AppManager {
    std::vector<float> verts;
} g_app;

struct CameraManager {
    float pitch; // radians
    float yaw;   // radians

    glm::mat4 projection;
} g_camera;

void updateCameraMatrix()
{
    float c1 = cos(g_camera.pitch);
    float s1 = sin(g_camera.pitch);
    float c2 = cos(g_camera.yaw);
    float s2 = sin(g_camera.yaw);

    glm::vec3 dirA {
        c2 * c1,
        s1,
        s2 * c1
    };
    // dirA = glm::normalize(dirA);

    LOG("dirA: %f %f %f\n", dirA.x, dirA.y, dirA.z);

    glm::vec3 dirB {
        c2,
        s1,
        s2
    };
    dirB = glm::normalize(dirB);

    LOG("dirB: %f %f %f\n", dirB.x, dirB.y, dirB.z);

    // glm::mat3 matrix {
    //     c1 ,  s1 * s2, c2 * s1, 
    //     0.0,       c2,     -s2,
    //     -s1,  c1 * s2, c1 * c2
    // };

    // for (int i = 0; i < 3; ++i)
    //     LOG("row %d: %f %f %f\n", i, matrix[i][0], matrix[i][1], matrix[i][2]); 
}

/**
 * @brief Recieves cursor position, measured in screen coordinates relative to
 * the top-left corner of the window.
 * 
 * @param window active window
 * @param x screen coordinate w.r.t. top left-corner
 * @param y screen coordinate w.r.t. top-left corner
 */
void cursorPosCallback(GLFWwindow* window, double x, double y)
{
    static float x0 = 0, y0 = 0;
    const float dx = x - x0;
    const float dy = y - y0;

    LOG("x: %f  y: %f\n", x, y);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        const static float scalar = 5e-3;
        g_camera.yaw   -= scalar * dx;
        g_camera.pitch -= scalar * dy;

        updateCameraMatrix();
    }

    x0 = x;
    y0 = y;
}



void init()
{
    createProgram("../shaders/default.vert", "../shaders/default.frag", "default", g_gl.programs[PROGRAM_DEFAULT]);

    // g_camera.projection = glm::perspective(45.0f, 1.0f, 0.01f, 100.0f);

    // Path Buffer
    {
    for (uint32_t j = 0u; j < PATCH_RESOLUTION+1; ++j)
    {
        for (uint32_t i = 0u; i < PATCH_RESOLUTION+2; ++i)
        {
            for (uint32_t k = 0u; k < ((i == 0) ? 2 : 1); ++k)
            {
                g_app.verts.push_back(float(i)/PATCH_RESOLUTION);
                g_app.verts.push_back(float(j)/PATCH_RESOLUTION);
                g_app.verts.push_back(0);
            }

            ++j;

            for (uint32_t k = 0u; k < ((i == PATCH_RESOLUTION+1) ? 2 : 1); ++k)
            {
                g_app.verts.push_back(float(i)/PATCH_RESOLUTION);
                g_app.verts.push_back(float(j)/PATCH_RESOLUTION);
                g_app.verts.push_back(0);
            }

            --j;
        }
    }

    glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_PATCH]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_PATCH]);

    glGenBuffers(1, &g_gl.buffers[BUFFER_VERTEX_PATCH]);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_VERTEX_PATCH]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * g_app.verts.size(), (void*)g_app.verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0u);
    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3,
                        (void*)(0));

    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    glBindVertexArray(0u);
    }
}

void render()
{
    glClearColor(0.12f, 0.68f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_gl.programs[PROGRAM_DEFAULT]);

    // set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_projMatrix", g_camera.projection);
    // set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_viewMatrix", g_camera.camera.GetViewMatrix());

    float scalar = 2;

    // for (uint32_t i = 0; i < CLIPMAP_LEVELS; ++i)
    // {
	// 	float ox=(int(g_camera.pos.x*(1<<i))&511)/float(512*PATCH_RESOLUTION);
	// 	float oy=(int(g_camera.pos.z*(1<<i))&511)/float(512*PATCH_RESOLUTION);

    //     glm::vec4 scale { scalar * 0.25, scalar * 0.25, 1, 1 };
    //     set_uni_vec4(g_gl.programs[PROGRAM_DEFAULT], "u_scale", scale);

    //     for (int j = -2; j < 2; ++j)
    //     {
    //         for (int k = -2; k < 2; ++k)
    //         {
    //             if ((i != CLIPMAP_LEVELS - 1) && (j == -1 || j == 0) && (k == -1 || k == 0))
    //                 continue;

    //             glm::vec4 offset { ox + float(k), oy + float(j), 0, 0 };
    //             if (j>=0) offset.y -= 1.0 / float(PATCH_RESOLUTION);
    //             if (k>=0) offset.x -= 1.0 / float(PATCH_RESOLUTION);

    //             set_uni_vec4(g_gl.programs[PROGRAM_DEFAULT], "u_offset", offset);

    //             glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_PATCH]);
    //             // glDrawArrays( GL_TRIANGLE_STRIP, 0, g_app.verts.size() / 3);
    //             glDrawArrays( GL_LINES, 0, g_app.verts.size() / 3);
    //         }
    //     }

    //     scalar *= 0.5;
    // }

    glDrawArrays( GL_TRIANGLE_STRIP, 0, g_app.verts.size() / 3);

    glUseProgram(0u);
}

void release()
{
    glDeleteBuffers(BUFFER_COUNT, g_gl.buffers);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Create the Window
    GLFWwindow* window = glfwCreateWindow(
        VIEWER_WIDTH, VIEWER_HEIGHT,
        "Geometry Clipmaps Demo", nullptr, nullptr 
    );
    if (window == nullptr) {
        LOG("=> Failure <=\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);


    // tell GLFW to capture our mouse
    glfwSetCursorPosCallback(window, &cursorPosCallback);

    // // glfwSetKeyCallback(window, &keyboardCallback);
    // // glfwSetMouseButtonCallback(window, &mouseButtonCallback);
    // glfwSetScrollCallback(window, &mouseScrollCallback);
    // // glfwSetWindowSizeCallback(window, &resizeCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}\n");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("gladLoadGLLoader failed\n");
        return -1;
    }

    LOG("-- Begin -- Demo\n");

    LOG("-- Begin -- Init\n");
    init();
    LOG("-- End -- Init\n");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // render();

        glfwSwapBuffers(window);
    }

    release();

    return 0;
}