#include <stdio.h>
#include <vector>

// #define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "Defines.hpp"
#include "Helpers.hpp"
#include "Camera.hpp"

constexpr uint32_t VIEWER_WIDTH  = 700u;
constexpr uint32_t VIEWER_HEIGHT = 700u;
constexpr uint32_t PATCH_RESOLUTION = 64u;
constexpr uint32_t TERRAIN_WIDTH = 1024; // we will render terrain in 1024x1024 grid
constexpr uint32_t TILE_DIM = 2u;       // can only be multiple of 2
constexpr uint32_t CLIPMAP_LEVELS = 10u;

enum
{
    PROGRAM_DEFAULT = 0,
    PROGRAM_TEST    = 1,
    PROGRAM_COUNT
};

enum
{
    TEXTURE_HEIGHTMAP = 0,
    TEXTURE_COUNT
};

enum 
{
    VERTEXARRAY_TEST_TILE     = 0,
    VERTEXARRAY_PATCH         = 1,
    VERTEXARRAY_TILE          = 2,
    VERTEXARRAY_COUNT
};

enum 
{
    BUFFER_TEST_TILE_VERTEX = 0,
    BUFFER_TEST_TILE_INDEX  = 1,
    BUFFER_VERTEX_PATCH         = 2,
    BUFFER_VERTEX_TILE          = 3,
    BUFFER_INDEX_TILE           = 4,
    BUFFER_COUNT
};

struct OpenGLManager {
    GLuint programs[PROGRAM_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
} g_gl;

struct AppManager {
    size_t tileIndexCount = 0;
    size_t testTileIndexCount = 0;
    glm::vec2 heightMapDim { 0.0f, 0.0f };

    bool showTest = false;
    bool showWireframe = false;
    bool morphEnabled = true;
    bool showMorphDebug = false;
    float heightScale = 1.0f;
} g_app;

struct CameraManager {
    Camera camera;

    float pitch; // radians
    float yaw;   // radians


    glm::vec3 pos { 0.0, 0.0, 0.0 };
    glm::vec3 forward { 0.0f, 0.0f, -1.0f };
    glm::mat4 view;

    glm::mat4 projection;
} g_camera;

void updateCameraMatrix()
{
    g_camera.view = glm::lookAt(g_camera.pos, g_camera.pos + g_camera.forward, { 0.0f, 1.0f, 0.0f });
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
    static float x0 = x, y0 = y;
    const float dx = x - x0;
    const float dy = y0 - y;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        const static float scalar = 5e-2;
        static float pitch = 0.0f;
        static float yaw   = 90.0f;

        pitch += scalar * dy; 
        yaw   += scalar * dx; 

        pitch = std::clamp(pitch, -89.0f, 89.0f);

        g_camera.forward = { 
            glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)),
            glm::sin(glm::radians(pitch)),
            -glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch)),
        };
        g_camera.forward = glm::normalize(g_camera.forward);

        updateCameraMatrix();
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        g_camera.pos.y -= dy;
        updateCameraMatrix();
    }

    x0 = x;
    y0 = y;
}

void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    const float scalar = 10.0f;
    g_camera.pos -= g_camera.forward * static_cast<float>(yoffset) * scalar;// * static_cast<float>(5e-2);

    updateCameraMatrix();
}

void loadDisplacementMap(std::string pathToFile)
{
    stbi_set_flip_vertically_on_load(true); 
    int tex_width, tex_height, tex_num_chan;
    const uint16_t* tex_data = (const uint16_t*)stbi_load(pathToFile.c_str(), &tex_width, &tex_height, &tex_num_chan, 0);

    if (!tex_data)
        EXIT("Failed to load texture " + pathToFile);

    std::vector<uint16_t> dmap(tex_width * tex_height *2);

    for (int y = 0; y < tex_height; ++y)
    {
        for (int x = 0; x < tex_width; ++x)
        {
            const int idx = y * tex_width + x;
            dmap[idx] = tex_data[idx];
        }
    }

    glGenTextures(1, &g_gl.textures[TEXTURE_HEIGHTMAP]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_HEIGHTMAP]);
    glTexStorage2D(GL_TEXTURE_2D, glm::log2((float)std::max(tex_width, tex_height)), GL_RED, tex_width, tex_height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height, GL_RED, GL_UNSIGNED_SHORT, &dmap[0]);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);

    glGenerateMipmap(GL_TEXTURE_2D);

   stbi_image_free((void*)tex_data);
}

GLuint create_texture_2d(const std::string tex_filepath)
{
   GLuint tex_handle;
   glGenTextures(1, &tex_handle);
   glBindTexture(GL_TEXTURE_2D, tex_handle);

   // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   stbi_set_flip_vertically_on_load(true); 
   int tex_width, tex_height, tex_num_chan;
   unsigned char* tex_data = stbi_load(tex_filepath.c_str(), &tex_width, &tex_height, &tex_num_chan, 4);

   if (!tex_data)
      EXIT("Failed to load texture " + tex_filepath);

   uint64_t format = 0x0;
   switch (tex_num_chan)
   {
      case 1:
         format = GL_RGBA;
         break;
      case 3:
         format = GL_RGB;
         break;
      case 4:
         format = GL_RGBA;
         break;
      default:
         EXIT("Failed to load texture " + tex_filepath);
   }

   glTexImage2D(GL_TEXTURE_2D, 0, format, tex_width, tex_height, 0, format, GL_UNSIGNED_BYTE, tex_data);
//    glGenerateMipmap(GL_TEXTURE_2D);

   g_app.heightMapDim.x = tex_width;
   g_app.heightMapDim.y = tex_height;
   stbi_image_free(tex_data);

   return tex_handle;
}


void init()
{
    g_gl.programs[PROGRAM_DEFAULT] = createProgram("../shaders/default.vert", "../shaders/default.frag", "default");
    g_gl.programs[PROGRAM_TEST   ] = createProgram("../shaders/test.vert"   , "../shaders/default.frag", "default");

    g_camera.camera.setPerspectiveProjection(glm::radians(50.f), VIEWER_WIDTH /  VIEWER_HEIGHT, 0.1f, 20000.f);
    updateCameraMatrix();

    g_gl.textures[TEXTURE_HEIGHTMAP] =  create_texture_2d("../assets/test3.png");

    {
        // (0,0) is the bottom left of the plane

        struct Vertex {
            float pos[3];

            Vertex(float x, float y, float z)
                : pos {x, y, z}
            {}
        };

        std::vector<Vertex> vertices;

        // const float step = 1.0f / TILE_DIM;

        for (size_t y = 0; y < (TILE_DIM + 1u); ++y)
        {
            // const float y_val = y * step;
            for (size_t x = 0; x < (TILE_DIM + 1u); ++x)
            {
                // const float x_val = x * step;
                vertices.emplace_back(x, 0.0f, y);
            }
        }

        // constexpr size_t numIndiciesPerRow = 2u * (TILE_DIM + 1u);
        // constexpr size_t numIndices = 2u * (TILE_DIM * TILE_DIM + 2 * TILE_DIM - 1);
        // std::vector<uint32_t> indices(numIndices);

        // for (uint32_t y = 0u; y < TILE_DIM; ++y)
        // {
        //     const size_t rowStartIndex = y * numIndiciesPerRow + y * 2u;
        //     const size_t rowEndIndex   = rowStartIndex + numIndiciesPerRow; 

        //     for (uint32_t i = rowStartIndex, j = 0u; i < rowEndIndex; i+=2u, ++j)
        //     {
        //         indices[i] = (TILE_DIM + 1u) * y + j;
        //     }

        //     for (uint32_t i = rowStartIndex + 1u; i < rowEndIndex; i+=2u)
        //     {
        //         indices[i] = indices[i-1u] + TILE_DIM + 1u;
        //     }

        //     // Degenerate Triangles
        //     if (y != TILE_DIM - 1u)
        //     {
        //         indices[rowEndIndex] = indices[rowEndIndex-1];
        //         indices[rowEndIndex+1] = indices[rowStartIndex+1];
        //     }
        // }    

        constexpr size_t numIndices = 6u * TILE_DIM * TILE_DIM;
        std::vector<uint32_t> indices(numIndices);

        size_t idx = 0;
        for (uint32_t y = 0u; y < TILE_DIM; ++y)
        {
            const uint32_t start = y * TILE_DIM + y;
            const uint32_t end   = start + TILE_DIM + 1u;
            for (uint32_t x = 0u; x < TILE_DIM; ++x)
            {
                indices[idx++] = start + x;
                indices[idx++] = end + x;
                indices[idx++] = start + 1u + x;

                indices[idx++] = end + x;
                indices[idx++] = end + 1u + x;
                indices[idx++] = start + 1u + x;

                // LOG("%d %d %d \t %d %d %d\n", start + x, end + x, start + 1u + x, end + x, end + 1u + x, start + 1u + x);
            }
        }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_TILE]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_VERTEX_TILE]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_INDEX_TILE]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TILE]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_VERTEX_TILE]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_INDEX_TILE]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.tileIndexCount = numIndices;
    }

    // Test 1024x1024 tile
    {
        struct Vertex {
            float pos[3];

            Vertex(float x, float y, float z)
                : pos {x, y, z}
            {}
        };


        const uint32_t testTileDim = 255;
        std::vector<Vertex> vertices;

        // const float step = 1.0f / TILE_DIM;

        for (size_t y = 0; y < (testTileDim + 1u); ++y)
        {
            // const float y_val = y * step;
            for (size_t x = 0; x < (testTileDim + 1u); ++x)
            {
                // const float x_val = x * step;
                vertices.emplace_back(x, 0.0f, y);
            }
        }

        constexpr size_t numIndices = 6u * testTileDim * testTileDim;
        std::vector<uint32_t> indices(numIndices);

        size_t idx = 0;
        for (uint32_t y = 0u; y < testTileDim; ++y)
        {
            const uint32_t start = y * testTileDim + y;
            const uint32_t end   = start + testTileDim + 1u;
            for (uint32_t x = 0u; x < testTileDim; ++x)
            {
                indices[idx++] = start + x;
                indices[idx++] = end + x;
                indices[idx++] = start + 1u + x;

                indices[idx++] = end + x;
                indices[idx++] = end + 1u + x;
                indices[idx++] = start + 1u + x;

                // LOG("%d %d %d \t %d %d %d\n", start + x, end + x, start + 1u + x, end + x, end + 1u + x, start + 1u + x);
            }
        }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_TEST_TILE]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TEST_TILE_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TEST_TILE_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TEST_TILE]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_TEST_TILE_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_TEST_TILE_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.testTileIndexCount = numIndices;
    }
}

void render()
{
    glClearColor(0.12f, 0.68f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_app.showTest)
    {

        glUseProgram(g_gl.programs[PROGRAM_TEST]);
        set_uni_mat4(g_gl.programs[PROGRAM_TEST], "u_projMatrix"    , g_camera.camera.getProjection());
        set_uni_mat4(g_gl.programs[PROGRAM_TEST], "u_viewMatrix"    , g_camera.view);
        set_uni_float(g_gl.programs[PROGRAM_TEST], "u_heightScale", g_app.heightScale);

        glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_HEIGHTMAP]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TEST_TILE]);
        // glDrawElements(GL_TRIANGLE_STRIP, g_app.tileIndexCount, GL_UNSIGNED_INT, nullptr);
        glDrawElements(GL_TRIANGLES, g_app.testTileIndexCount, GL_UNSIGNED_INT, nullptr);

        // return;
    }

    glUseProgram(g_gl.programs[PROGRAM_DEFAULT]);

    // glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_HEIGHTMAP]);

    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_projMatrix"    , g_camera.camera.getProjection());
    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_viewMatrix"    , g_camera.view);
    set_uni_vec2(g_gl.programs[PROGRAM_DEFAULT], "u_samplerDim"    , g_app.heightMapDim);
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_tileResolution", TILE_DIM);
    set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_morphEnable", g_app.morphEnabled);
    set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_showMorphDebug", g_app.showMorphDebug);
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_heightScale", g_app.heightScale);
    // set_uni_vec3(g_gl.programs[PROGRAM_DEFAULT], "u_color", {1.0f, 0.0f, 0.0f}};


    if (g_app.showWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const GLenum drawMode = GL_TRIANGLE_STRIP;
    static constexpr float tileDim = static_cast<float>(TILE_DIM);

    enum BlendDir
    {
        TOP   = 1,
        RIGHT = 2,
        BOT   = 4,
        LEFT  = 8,
        NONE  = 16
    };


    auto renderTile = [drawMode](glm::vec2 translation, float level, float scale, int blendDir) {
        // glm::vec3 snapped_pos = glm::floor(g_camera.pos / scale) * scale;
        // glm::vec3 snapped_pos = glm::floor(g_camera.pos);
        glm::vec3 snapped_pos = g_camera.pos;
        glm::vec3 nextLvlSnappedPos = glm::floor(g_camera.pos / (scale * 2)) * (scale * 2);
        snapped_pos.y = 0.0f;
        nextLvlSnappedPos.y = 0.0f;

        // LOG("scale: %.2f  pos: %.2f %.2f  snapped_pos: %.2f %.2f  coarse_pos: %.2f %.2f  translation: %.2f %.2f\n", (float)scale, g_camera.pos.x, g_camera.pos.z, snapped_pos.x, snapped_pos.z, nextLvlSnappedPos.x, nextLvlSnappedPos.z, translation.x, translation.y);

        glm::vec4 u_tileInfo { translation.x, translation.y, level, scale };
        glm::vec2 u_viewPos  { g_camera.pos.x, g_camera.pos.z };
        LOG("tileInfo: %.1f %1.f %d %d\n", u_tileInfo.x, u_tileInfo.y, (int)u_tileInfo.z, (int)u_tileInfo.w);

        set_uni_vec4(g_gl.programs[PROGRAM_DEFAULT], "u_tileInfo", u_tileInfo);
        set_uni_vec2(g_gl.programs[PROGRAM_DEFAULT], "u_viewPos" , u_viewPos);
        set_uni_vec2(g_gl.programs[PROGRAM_DEFAULT], "u_coarseOffset" , { nextLvlSnappedPos.x + translation.x, nextLvlSnappedPos.z + translation.y });

        // transform = glm::scale(transform, { scale, 1.0f, scale });

        // set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_modelMatrix", transform);
        set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_morphDirection", blendDir);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TILE]);
        // glDrawElements(GL_TRIANGLE_STRIP, g_app.tileIndexCount, GL_UNSIGNED_INT, nullptr);
        glDrawElements(GL_TRIANGLES, g_app.tileIndexCount, GL_UNSIGNED_INT, nullptr);
    };
    

    // Center Tiles
    renderTile({0,0}, 1, 1, NONE);
    // renderTile({0,-tileDim}, 1, 1, NONE);
    // renderTile({-tileDim,-tileDim}, 1, 1, NONE);
    // renderTile({-tileDim, 0}, 1, 1, NONE);

    // Rings
    for (uint32_t level = 0u; level < CLIPMAP_LEVELS; ++level)
    {
        const float scale = (1<<level);
        const float translation = scale * tileDim;

        // Top
        renderTile({0.0f, translation}, level, scale, TOP);
        // renderTile({translation, translation}, level, scale, TOP | RIGHT);

        // // Right
        // renderTile({translation, 0.0f}, level, scale, RIGHT);
        // renderTile({translation, -translation}, level, scale, RIGHT);

        // // Bot
        // renderTile({translation, -2.0f*translation}, level, scale, BOT | RIGHT);
        // renderTile({0.0f, -2.0f*translation}, level, scale, BOT);
        // renderTile({-translation, -2.0f*translation}, level, scale, BOT);
        // renderTile({-2.0f*translation, -2.0f*translation}, level, scale, BOT | LEFT);

        // // Left
        // renderTile({-2.0f*translation, -translation}, level, scale, LEFT);
        // renderTile({-2.0f*translation, 0.0f}, level, scale, LEFT);

        // // Top
        // renderTile({-2.0f*translation, translation}, level, scale, TOP | LEFT);
        // renderTile({-translation, translation}, level, scale, TOP);
    }

    glUseProgram(0u);
}

void gui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Gui"))
    {
        ImGui::Checkbox("Show Test", &g_app.showTest);
        ImGui::Checkbox("Wireframe", &g_app.showWireframe);
        ImGui::Checkbox("Morph", &g_app.morphEnabled);
        ImGui::Checkbox("Morph Debug", &g_app.showMorphDebug);
        ImGui::SliderFloat("Height Scale", &g_app.heightScale, 0.1f, 100.0f, "%.2f");
        ImGui::InputFloat3("View Pos", &(g_camera.pos[0]));
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void release()
{
    glDeleteBuffers(BUFFER_COUNT, g_gl.buffers);
    glDeleteVertexArrays(VERTEXARRAY_COUNT, g_gl.vertexArrays);
    glDeleteTextures(TEXTURE_COUNT, g_gl.textures);
    glDeleteProgram(g_gl.programs[PROGRAM_DEFAULT]);
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
    glfwSetCursorPosCallback(window, &cursorPosCallback);
    glfwSetScrollCallback(window, &mouseScrollCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}\n");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("gladLoadGLLoader failed\n");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    LOG("-- Begin -- Demo\n");

    LOG("-- Begin -- Init\n");
    init();
    LOG("-- End -- Init\n");

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        render();

        gui();

        glfwSwapBuffers(window);
    }

    release();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}