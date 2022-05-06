#include <stdio.h>
#include <vector>
#include <array>

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
constexpr uint32_t CLIPMAP_LEVELS = 1u;

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
    VERTEXARRAY_TILE          = 1,
    VERTEXARRAY_FILLER        = 2,
    VERTEXARRAY_CROSS         = 3,
    VERTEXARRAY_TRIM          = 4,
    VERTEXARRAY_SEAM          = 5,
    VERTEXARRAY_COUNT
};

enum 
{
    BUFFER_TEST_TILE_VERTEX = 0,
    BUFFER_TEST_TILE_INDEX  = 1,
    BUFFER_VERTEX_TILE      = 2,
    BUFFER_INDEX_TILE       = 3,
    BUFFER_FILLER_VERTEX    = 4,
    BUFFER_FILLER_INDEX     = 5,
    BUFFER_CROSS_VERTEX     = 6,
    BUFFER_CROSS_INDEX      = 7,
    BUFFER_TRIM_VERTEX      = 8,
    BUFFER_TRIM_INDEX       = 9,
    BUFFER_SEAM_VERTEX      = 10,
    BUFFER_SEAM_INDEX       = 11,
    BUFFER_COUNT
};

struct OpenGLManager {
    GLuint programs[PROGRAM_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
} g_gl;

struct AppManager {
    size_t tileIndexCount   = 0u;
    size_t fillerIndexCount = 0u;
    size_t crossIndexCount  = 0u;
    size_t trimIndexCount   = 0u;
    size_t seamIndexCount   = 0u;

    size_t testTileIndexCount = 0;
    glm::vec2 heightMapDim { 0.0f, 0.0f };

    bool showTest = false;
    bool showWireframe = false;
    bool morphEnabled = true;
    bool showMorphDebug = false;
    float heightScale = 1.0f;
    int rotType = 0;
    bool freezeCamera = false;
    bool seamsEnabled = false;

    glm::vec3 frozenCamPos { 0.0f, 0.0f, 0.0f};
} g_app;

struct CameraManager {
    Camera camera;

    float pitch; // radians
    float yaw;   // radians


    glm::vec3 pos { 0.0, 0.0, -50.0 };
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


constexpr uint32_t NUM_CLIPMAP_LEVELS = 5u;
constexpr uint32_t TILE_RESOLUTION = 125u;


void init()
{
    g_gl.programs[PROGRAM_DEFAULT] = createProgram("../shaders/default.vert", "../shaders/default.frag", "default");
    g_gl.programs[PROGRAM_TEST   ] = createProgram("../shaders/test.vert"   , "../shaders/default.frag", "default");

    g_camera.camera.setPerspectiveProjection(glm::radians(50.f), VIEWER_WIDTH /  VIEWER_HEIGHT, 0.1f, 20000.f);
    updateCameraMatrix();

    g_gl.textures[TEXTURE_HEIGHTMAP] =  create_texture_2d("../assets/kauai.png");

    struct Vertex {
        float pos[3];

        Vertex()
            : pos { 0.0f, 0.0f, 0.0f }
        {}

        Vertex(float x, float y, float z)
            : pos {x, y, z}
        {}
    };

    // Tile mesh
    {
        // (0,0) is the bottom left of the plane

        std::vector<Vertex> vertices;

        for (size_t y = 0; y < (TILE_RESOLUTION + 1u); ++y)
        {
            for (size_t x = 0; x < (TILE_RESOLUTION + 1u); ++x)
            {
                vertices.emplace_back(x, 0.0f, y);
            }
        }

        // constexpr size_t numIndiciesPerRow = 2u * (TILE_RESOLUTION + 1u);
        // constexpr size_t numIndices = 2u * (TILE_RESOLUTION * TILE_RESOLUTION + 2 * TILE_RESOLUTION - 1);
        // std::vector<uint32_t> indices(numIndices);

        // for (uint32_t y = 0u; y < TILE_RESOLUTION; ++y)
        // {
        //     const size_t rowStartIndex = y * numIndiciesPerRow + y * 2u;
        //     const size_t rowEndIndex   = rowStartIndex + numIndiciesPerRow; 

        //     for (uint32_t i = rowStartIndex, j = 0u; i < rowEndIndex; i+=2u, ++j)
        //     {
        //         indices[i] = (TILE_RESOLUTION + 1u) * y + j;
        //     }

        //     for (uint32_t i = rowStartIndex + 1u; i < rowEndIndex; i+=2u)
        //     {
        //         indices[i] = indices[i-1u] + TILE_RESOLUTION + 1u;
        //     }

        //     // Degenerate Triangles
        //     if (y != TILE_RESOLUTION - 1u)
        //     {
        //         indices[rowEndIndex] = indices[rowEndIndex-1];
        //         indices[rowEndIndex+1] = indices[rowStartIndex+1];
        //     }
        // }    

        constexpr size_t numIndices = 6u * TILE_RESOLUTION * TILE_RESOLUTION;
        std::vector<uint32_t> indices(numIndices);

        size_t idx = 0;
        for (uint32_t y = 0u; y < TILE_RESOLUTION; ++y)
        {
            const uint32_t start = y * TILE_RESOLUTION + y;
            const uint32_t end   = start + TILE_RESOLUTION + 1u;
            for (uint32_t x = 0u; x < TILE_RESOLUTION; ++x)
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

    // Filler mesh
    {
        constexpr size_t mesh_vertex_count = 2u * (TILE_RESOLUTION + 1u);
        constexpr size_t vertex_count = 4u * mesh_vertex_count;
        std::array<Vertex, vertex_count> vertices;

        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x <= TILE_RESOLUTION; ++x)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(TILE_RESOLUTION + x + 1), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == mesh_vertex_count);

        for (int y = 0; y < 2u; ++y)
        {
            for (int x = TILE_RESOLUTION; x >= 0; --x)
            {
                vertices[v_idx++] = Vertex(-1.0f * static_cast<float>(TILE_RESOLUTION + x), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == 2u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = 0; y <= TILE_RESOLUTION; ++y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, static_cast<float>(TILE_RESOLUTION + y + 1));
            }
        }

        assert(v_idx == 3u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = TILE_RESOLUTION; y >= 0; --y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, -1.0f * static_cast<float>(TILE_RESOLUTION + y));
            }
        }

        assert(v_idx == 4u * mesh_vertex_count);

        constexpr size_t mesh_index_count = 6u * TILE_RESOLUTION;
        constexpr size_t index_count = 4u * mesh_index_count;
        std::array<uint32_t, index_count> indices;

        size_t idx = 0u;
        for (uint32_t patch = 0u; patch < 4u; ++patch)
        {
            for (uint32_t i = 0; i < TILE_RESOLUTION; ++i)
            {
                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 1u;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 2u;

                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 2u;
                indices[idx++] = patch * mesh_vertex_count + i + 1u;
            }
        }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_FILLER]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_FILLER_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_FILLER_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_FILLER]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_FILLER_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_FILLER_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.fillerIndexCount = index_count;
    }

    // Cross mesh
    {
        constexpr size_t mesh_vertex_count = 2u * (TILE_RESOLUTION + 1u);
        constexpr size_t vertex_count = 4u * mesh_vertex_count;
        std::array<Vertex, vertex_count> vertices;

        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x <= TILE_RESOLUTION; ++x)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x + 1), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == mesh_vertex_count);

        for (int y = 0; y < 2u; ++y)
        {
            for (int x = TILE_RESOLUTION; x >= 0; --x)
            {
                vertices[v_idx++] = Vertex(-1.0f * static_cast<float>(x), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == 2u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = 0; y <= TILE_RESOLUTION; ++y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, static_cast<float>(y + 1));
            }
        }

        assert(v_idx == 3u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = TILE_RESOLUTION; y >= 0; --y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, -1.0f * static_cast<float>(y));
            }
        }

        assert(v_idx == 4u * mesh_vertex_count);

        constexpr size_t mesh_index_count = 6u * TILE_RESOLUTION;
        constexpr size_t index_count = 4u * mesh_index_count;
        std::array<uint32_t, index_count> indices;

        size_t idx = 0u;
        for (uint32_t patch = 0u; patch < 4u; ++patch)
        {
            for (uint32_t i = 0; i < TILE_RESOLUTION; ++i)
            {
                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 1u;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 2u;

                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + TILE_RESOLUTION + 2u;
                indices[idx++] = patch * mesh_vertex_count + i + 1u;
            }
        }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_CROSS]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_CROSS_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_CROSS_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_CROSS]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_CROSS_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_CROSS_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.crossIndexCount = index_count;
    }

    // Trim mesh
    {
        constexpr size_t inner_width = (TILE_RESOLUTION * 4u + 1u) + 2u;
        constexpr size_t vertex_count = 2u * 2u * inner_width;
        std::array<Vertex, vertex_count> vertices;

        // Hoizontal (bottom)
        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x < inner_width; ++x)
            {
                vertices[v_idx++] = Vertex(x, 0.0f, y);
            }
        }

        // Vertical
        for (int x = 0; x < 2u; ++x)
        {
            for (int y = 0; y < inner_width; ++y)
            {
                vertices[v_idx++] = Vertex(x, 0.0f, y);
            }
        }

        for (int i = 0; i < vertices.size(); i++)
        {
            vertices[i].pos[0] -= TILE_RESOLUTION * 2u + 1.5f;
            vertices[i].pos[2] -= TILE_RESOLUTION * 2u + 1.5f;
        }

        assert(v_idx == vertex_count);

        constexpr size_t index_count = 2u * (inner_width - 1) * 6u;
        std::array<uint32_t, index_count> indices;

        // Horizontal (bottom)
        size_t idx = 0;
        for (int mesh = 0; mesh < 2; mesh++)
        {
            for (uint32_t i = 0; i < (inner_width - 1); ++i)
            {
                indices[idx++] = mesh * 2u * inner_width + i;
                indices[idx++] = mesh * 2u * inner_width + i + inner_width;
                indices[idx++] = mesh * 2u * inner_width + i + inner_width + 1u;

                indices[idx++] = mesh * 2u * inner_width + i;
                indices[idx++] = mesh * 2u * inner_width + i + inner_width + 1u;
                indices[idx++] = mesh * 2u * inner_width + i + 1u;
            }
        }
        
        assert(idx == index_count);

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_TRIM]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TRIM]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.trimIndexCount = indices.size();
    }

    // Seam mesh
    {
        constexpr size_t inner_width = (TILE_RESOLUTION * 4u + 1u) + 1u;
        constexpr size_t vertex_count = 4u * (inner_width);
        std::array<Vertex, vertex_count> vertices;

        for( int i = 0; i < inner_width; i++ )
        {
            vertices[ inner_width * 0 + i ] = { i, 0, 0 };
            vertices[ inner_width * 1 + i ] = { inner_width, 0, i };
            vertices[ inner_width * 2 + i ] = { inner_width - i, 0, inner_width };
            vertices[ inner_width * 3 + i ] = { 0, 0, inner_width - i };
        }

        for (int i = 0; i < vertices.size(); ++i)
        {
            // vertices[i].pos[0] *= 1.0f - 1.0f / ((float)inner_width + 20);
            // vertices[i].pos[2] *= 1.0f - 1.0f / ((float)inner_width + 20);

            // vertices[i].pos[0] -= 0.5f;
            // vertices[i].pos[2] -= 0.5f;
        }

        constexpr size_t index_count = inner_width * 6u;
        std::array<uint32_t, index_count> indices;
        size_t n = 0;

        for( uint32_t i = 0; i < inner_width * 4; i += 2 )
        {
            indices[ n++ ] = i + 1;
            indices[ n++ ] = i;
            indices[ n++ ] = i + 2;
        }

        // make the last triangle wrap around
        indices[ indices.size() - 1 ] = 0;

        assert(n == indices.size());

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_SEAM]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_SEAM_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_SEAM_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_SEAM]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_SEAM_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_SEAM_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.seamIndexCount = indices.size();
    }


#if 0
    // Trim Mesh
    {
        constexpr size_t inner_filled_width = 4u * TILE_RESOLUTION + 1u;
        constexpr size_t inner_width = inner_filled_width + 2u;
        constexpr size_t vertex_count = 2u * inner_width;
        std::array<Vertex, vertex_count> vertices;

        // Default trim shape is in the form of a "L"

        // Hoizontal (bottom)
        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x < inner_width; ++x)
            {
                vertices[v_idx++] = Vertex(x, 0.0f, y);
            }
        }

        assert(v_idx == vertex_count);

        for (size_t i = 0; i < vertex_count; ++i)
        {
            vertices[i].pos[0] -= (inner_width + 1) / 2;       
            vertices[i].pos[2] -= (inner_width + 1) / 2;
        }

        // Vertical (top)

        constexpr size_t index_count = 6u * (inner_width - 1);//  + 6u * (inner_width - 1u);
        std::array<uint32_t, index_count> indices;

        // Horizontal (bottom)
        size_t idx = 0;
        for (uint32_t i = 0; i < (inner_width - 1); ++i)
        {
            indices[idx++] = i;
            indices[idx++] = i + inner_width;
            indices[idx++] = i + inner_width + 1u;

            indices[idx++] = i;
            indices[idx++] = i + inner_width + 1u;
            indices[idx++] = i + 1u;
        }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_TRIM]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TRIM]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.trimIndexCount = index_count;
    }


    {
        constexpr int CLIPMAP_VERT_RESOLUTION = TILE_RESOLUTION * 4 + 1 + 1;
        std::array<Vertex, (CLIPMAP_VERT_RESOLUTION * 2 + 1) * 2> vertices;
        size_t n = 0;

        // vertical part of L
        for( int i = 0; i < CLIPMAP_VERT_RESOLUTION + 1; i++ ) {
            vertices[ n++ ] = { 0, 0, CLIPMAP_VERT_RESOLUTION - i };
            vertices[ n++ ] = { 1, 0, CLIPMAP_VERT_RESOLUTION - i };
        }

        size_t start_of_horizontal = n;
                                   
        // horizontal part of L
        for( int i = 0; i < CLIPMAP_VERT_RESOLUTION; i++ ) {
            vertices[ n++ ] = { i + 1, 0, 0 };
            vertices[ n++ ] = { i + 1, 0, 1 };
        }

        assert( n == vertices.size() );

        for( Vertex& v : vertices ) {
            v.pos[0] -= 0.5f * CLIPMAP_VERT_RESOLUTION + 1;
            v.pos[2] -= 0.5f * CLIPMAP_VERT_RESOLUTION + 1;
        }

        constexpr size_t index_count = 6u * (CLIPMAP_VERT_RESOLUTION * 2 - 1);//  + 6u * (inner_width - 1u);
        std::array<uint32_t, index_count> indices;
        n = 0;

        for( int i = 0; i < CLIPMAP_VERT_RESOLUTION; i++ ) {
            indices[ n++ ] = ( i + 0 ) * 2 + 1;
            indices[ n++ ] = ( i + 0 ) * 2 + 0;
            indices[ n++ ] = ( i + 1 ) * 2 + 0;

            indices[ n++ ] = ( i + 1 ) * 2 + 1;
            indices[ n++ ] = ( i + 0 ) * 2 + 1;
            indices[ n++ ] = ( i + 1 ) * 2 + 0;
        }

        for( int i = 0; i < CLIPMAP_VERT_RESOLUTION - 1; i++ ) {
            indices[ n++ ] = start_of_horizontal + ( i + 0 ) * 2 + 1;
            indices[ n++ ] = start_of_horizontal + ( i + 0 ) * 2 + 0;
            indices[ n++ ] = start_of_horizontal + ( i + 1 ) * 2 + 0;

            indices[ n++ ] = start_of_horizontal + ( i + 1 ) * 2 + 1;
            indices[ n++ ] = start_of_horizontal + ( i + 0 ) * 2 + 1;
            indices[ n++ ] = start_of_horizontal + ( i + 1 ) * 2 + 0;
        }

        assert( n == indices.size());

        // // Horizontal (bottom)
        // size_t idx = 0;
        // for (int i = 0; i < (CLIPMAP_VERT_RESOLUTION - 1); ++i)
        // {
        //     indices[idx++] = i;
        //     indices[idx++] = i + CLIPMAP_VERT_RESOLUTION;
        //     indices[idx++] = i + CLIPMAP_VERT_RESOLUTION + 1u;

        //     indices[idx++] = i;
        //     indices[idx++] = i + CLIPMAP_VERT_RESOLUTION + 1u;
        //     indices[idx++] = i + 1u;
        // }

        glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_TRIM]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glGenBuffers(1, &g_gl.buffers[BUFFER_TRIM_INDEX]);

        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TRIM]);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_TRIM_INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0u);
        glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, pos));

        glBindVertexArray(0u);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);

        g_app.trimIndexCount = index_count;

        LOG("DONE\n");
    }
#endif
    // Test 1024x1024 tile
    {
        const uint32_t testTileDim = 255;
        std::vector<Vertex> vertices;

        // const float step = 1.0f / TILE_RESOLUTION;

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
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_tileResolution", TILE_RESOLUTION);
    set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_morphEnable", g_app.morphEnabled);
    set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_showMorphDebug", g_app.showMorphDebug);
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_heightScale", g_app.heightScale);

    glm::mat4 rotMatrix { 1.0f };
    glm::rotate(rotMatrix, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix", rotMatrix);

    if (g_app.showWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const GLenum drawMode = GL_TRIANGLE_STRIP;
    static constexpr float tileDim = static_cast<float>(TILE_RESOLUTION);

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
        // LOG("tileInfo: %.1f %1.f %d %d\n", u_tileInfo.x, u_tileInfo.y, (int)u_tileInfo.z, (int)u_tileInfo.w);

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




void testRender()
{
    glClearColor(0.12f, 0.68f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_app.showWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(g_gl.programs[PROGRAM_DEFAULT]);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_HEIGHTMAP]);

    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_projMatrix"    , g_camera.camera.getProjection());
    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_viewMatrix"    , g_camera.view);
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_tileResolution", static_cast<float>(TILE_RESOLUTION));
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_heightScale", g_app.heightScale);

    const glm::vec2 view_pos = (g_app.freezeCamera) ?
        glm::vec2(g_app.frozenCamPos.x, g_app.frozenCamPos.z) : 
        glm::vec2(g_camera.pos.x, g_camera.pos.z );
    glm::mat4 identMat { 1.0f };

    // draw cross
    set_uni_vec2 (g_gl.programs[PROGRAM_DEFAULT], "u_translation", glm::floor(view_pos) );
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_scale"      , 1.0f                 );
    set_uni_mat4 (g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix"  , identMat             );
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_CROSS]);
    glDrawElements(GL_TRIANGLES, g_app.crossIndexCount, GL_UNSIGNED_INT, nullptr);

    for (uint32_t level = 0u; level < NUM_CLIPMAP_LEVELS; ++level)
    {
        const float scale = (1u << level);
        const glm::vec2 snapped_pos = glm::floor(view_pos / scale) * scale;
        const glm::vec2 tile_size { TILE_RESOLUTION << level, TILE_RESOLUTION << level };
        const glm::vec2 base = snapped_pos - glm::vec2(TILE_RESOLUTION << (level + 1), TILE_RESOLUTION << (level + 1));

        for( int x = 0; x < 4; x++ )
        {
            for( int y = 0; y < 4; y++ )
            {
                // draw a 4x4 set of tiles. cut out the middle 2x2 unless we're at the finest level
                if( level != 0 && ( x == 1 || x == 2 ) && ( y == 1 || y == 2 ) ) {
                    continue;
                }

                glm::mat3 rotation_matrix { 1.0f };
                const glm::vec2 fill = glm::vec2(x >= 2 ? 1 : 0, y >= 2 ? 1 : 0) * scale;
                const glm::vec2 translation = base + glm::vec2(x, y) * tile_size + fill;

                set_uni_vec2 (g_gl.programs[PROGRAM_DEFAULT], "u_translation", translation);
                set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_scale"      , scale      );
                set_uni_mat4 (g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix"  , identMat   );
                set_uni_vec3 (g_gl.programs[PROGRAM_DEFAULT], "u_debugColor" , { 0.0f, 0.0f, 0.0f });

                glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TILE]);
                glDrawElements(GL_TRIANGLES, g_app.tileIndexCount, GL_UNSIGNED_INT, nullptr);

                // LOG("level: %u  base: %.2f %.2f  fill: %d %d  tl: %.2f %.2f  size: %d %d\n", level, base.x, base.y, (int)fill.x, (int)fill.y, tile_tl.x, tile_tl.y, (int)tile_size.x, (int)tile_size.y);
            }
        }


        // draw filler
        {
            glm::mat3 rotation_matrix { 1.0f };

            set_uni_vec2 (g_gl.programs[PROGRAM_DEFAULT], "u_translation", snapped_pos);
            set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_scale"      , scale      );
            set_uni_mat4 (g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix"  , rotation_matrix);

            glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_FILLER]);
            glDrawElements(GL_TRIANGLES, g_app.fillerIndexCount, GL_UNSIGNED_INT, nullptr);
        }

        // Draw trim/seam
        {
            if (level != NUM_CLIPMAP_LEVELS - 1)
            {
                static constexpr float rotation[4] = {
                    0.0f,
                    glm::pi<float>() + glm::half_pi<float>(),
                    glm::half_pi<float>(),
                    glm::pi<float>(),
                };

                const float next_scale = scale * 2.0f;
                const glm::vec2 next_snapped_pos = glm::floor(view_pos / next_scale) * next_scale;

                // Seam
                {
                    if (g_app.seamsEnabled)
                    {
                        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                        glm::vec2 next_base = next_snapped_pos - glm::vec2(static_cast<float>(TILE_RESOLUTION << (level + 1), static_cast<float>(TILE_RESOLUTION << (level + 1))));

                        set_uni_vec2 (g_gl.programs[PROGRAM_DEFAULT], "u_translation", next_base);
                        set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_scale"      , scale    );
                        set_uni_mat4 (g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix"  , identMat );
                        // set_uni_vec3 (g_gl.programs[PROGRAM_DEFAULT], "u_debugColor" , { 1.0f, 0.0f, 0.0f });

                        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_SEAM]);
                        glDrawElements(GL_TRIANGLES, g_app.seamIndexCount, GL_UNSIGNED_INT, nullptr);

                        if (g_app.showWireframe)
                            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        else
                            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    }
                }

                // Trim
                {
                    const glm::vec2 tile_center = snapped_pos + glm::vec2(scale * 0.5f, scale * 0.5f);

                    glm::vec2 d = view_pos - next_snapped_pos;
                    uint32_t r = 0;
                    r |= d.x >= scale ? 0 : 2;
                    r |= d.y >= scale ? 0 : 1;

                    if (level == 0)
                    {
                        // LOG("dx=%.2f  dy=%.2f scale=%d  r=%u  tile_center: %.2f %.2f\n", d.x, d.y, (int)scale, r, tile_center.x, tile_center.y);
                        
                    }

                    glm::mat4 rotation_matrix { 1.0f };
                    rotation_matrix = glm::rotate(rotation_matrix, -rotation[r], glm::vec3(0.0f, 1.0f, 0.0f));

                    set_uni_vec2 (g_gl.programs[PROGRAM_DEFAULT], "u_translation", tile_center);
                    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_scale"      , scale      );
                    set_uni_mat4 (g_gl.programs[PROGRAM_DEFAULT], "u_rotMatrix"  , rotation_matrix);
                    set_uni_vec3 (g_gl.programs[PROGRAM_DEFAULT], "u_debugColor" , { 0.0f, 0.0f, 1.0f });
    
                    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_TRIM]);
                    glDrawElements(GL_TRIANGLES, g_app.trimIndexCount, GL_UNSIGNED_INT, nullptr);
                }
            }
        }
    }

#if 0
for ( u32 l = 0; l < NUM_CLIPMAP_LEVELS; l++ ) {
			float scale = checked_cast< float >( u32( 1 ) << l );
			v2 snapped_pos = floorf( game->frozen_pos.xy() / scale ) * scale;

			// draw tiles
			v2 tile_size = v2( checked_cast< float >( TILE_RESOLUTION << l ) );
			v2 base = snapped_pos - v2( checked_cast< float >( TILE_RESOLUTION << ( l + 1 ) ) );

			for( int x = 0; x < 4; x++ ) {
				for( int y = 0; y < 4; y++ ) {
					// draw a 4x4 set of tiles. cut out the middle 2x2 unless we're at the finest level
					if( l != 0 && ( x == 1 || x == 2 ) && ( y == 1 || y == 2 ) ) {
						continue;
					}

					v2 fill = v2( x >= 2 ? 1 : 0, y >= 2 ? 1 : 0 ) * scale;
					v2 tile_tl = base + v2( x, y ) * tile_size + fill;

					// draw a low poly tile if the tile is entirely outside the world
					v2 tile_br = tile_tl + tile_size;
					bool inside = true;
					if( !intervals_overlap( tile_tl.x, tile_br.x, 0, clipmap.heightmap.w ) )
						inside = false;
					if( !intervals_overlap( tile_tl.y, tile_br.y, 0, clipmap.heightmap.h ) )
						inside = false;

					Mesh mesh = inside ? clipmap.gpu.tile : clipmap.gpu.empty_tile;
					render_state.set_uniform( "model", rotation_uniforms[ 0 ] );
					render_state.set_uniform( "clipmap", renderer_uniforms( tile_tl, scale ) );
					renderer_draw_mesh( mesh, render_state );
				}
			}
#endif
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
        ImGui::InputInt("Rot Type", &g_app.rotType);
        if (ImGui::Checkbox("Freeze Camera", &g_app.freezeCamera))
        {
            g_app.frozenCamPos = g_camera.pos;
        }
        ImGui::Checkbox("Seams", &g_app.seamsEnabled);
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

        // render();
        testRender();

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