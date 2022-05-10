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

constexpr uint32_t VIEWER_WIDTH = 1200u;
constexpr uint32_t VIEWER_HEIGHT = 900u;
constexpr uint32_t NUM_CLIPMAP_LEVELS = 4u;

enum
{
    PROGRAM_DEFAULT = 0,
    PROGRAM_COUNT
};

enum
{
    TEXTURE_HEIGHTMAP = 0,
    TEXTURE_COUNT
};

enum
{
    VERTEXARRAY_TEST_TILE = 0,
    VERTEXARRAY_TILE = 1,
    VERTEXARRAY_FILLER = 2,
    VERTEXARRAY_CROSS = 3,
    VERTEXARRAY_TRIM = 4,
    VERTEXARRAY_SEAM = 5,
    VERTEXARRAY_COUNT
};

enum
{
    BUFFER_TEST_TILE_VERTEX = 0,
    BUFFER_TEST_TILE_INDEX = 1,
    BUFFER_VERTEX_TILE = 2,
    BUFFER_INDEX_TILE = 3,
    BUFFER_FILLER_VERTEX = 4,
    BUFFER_FILLER_INDEX = 5,
    BUFFER_CROSS_VERTEX = 6,
    BUFFER_CROSS_INDEX = 7,
    BUFFER_TRIM_VERTEX = 8,
    BUFFER_TRIM_INDEX = 9,
    BUFFER_SEAM_VERTEX = 10,
    BUFFER_SEAM_INDEX = 11,
    BUFFER_COUNT
};

struct OpenGLManager
{
    GLuint programs[PROGRAM_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
} g_gl;

struct AppManager
{
    uint32_t numClipmapLevels = 5u;
    uint32_t tileDim = 64u;

    size_t indexCounts[VERTEXARRAY_COUNT];
    glm::vec3 meshDebugColors[VERTEXARRAY_COUNT];

    std::vector<std::array<glm::vec2, 12>> tileTransformTable;
    std::array<glm::mat4, 4> trimRotMatrices;

    size_t tileIndexCount = 0u;
    size_t fillerIndexCount = 0u;
    size_t crossIndexCount = 0u;
    size_t trimIndexCount = 0u;
    size_t seamIndexCount = 0u;

    size_t testTileIndexCount = 0;
    glm::vec2 heightMapDim{0.0f, 0.0f};

    bool showWireframe = false;
    float heightScale = 1.0f;
    bool freezeCamera = false;
    bool seamsEnabled = true;
    bool showDebugColors = true;

    glm::vec3 frozenCamPos{0.0f, 0.0f, 0.0f};
} g_app;

struct CameraManager
{
    Camera camera;

    float pitch; // radians
    float yaw;   // radians

    glm::vec3 pos{0.0, 0.0, 0.0};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::mat4 view;

    glm::mat4 projection;
} g_camera;

void updateCameraMatrix()
{
    g_camera.view = glm::lookAt(g_camera.pos, g_camera.pos + g_camera.forward, {0.0f, 1.0f, 0.0f});
}

/**
 * @brief Recieves cursor position, measured in screen coordinates relative to
 * the top-left corner of the window.
 *
 * @param window active window
 * @param x screen coordinate w.r.t. top left-corner
 * @param y screen coordinate w.r.t. top-left corner
 */
void cursorPosCallback(GLFWwindow *window, double x, double y)
{
    static float x0 = x, y0 = y;
    const float dx = x - x0;
    const float dy = y0 - y;

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        const static float scalar = 5e-2;
        static float pitch = 0.0f;
        static float yaw = 90.0f;

        pitch += scalar * dy;
        yaw += scalar * dx;

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

void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    const float scalar = 10.0f;
    g_camera.pos -= g_camera.forward * static_cast<float>(yoffset) * scalar; // * static_cast<float>(5e-2);

    updateCameraMatrix();
}

GLuint create_texture_2d(const std::string tex_filepath)
{
    GLuint tex_handle;
    glGenTextures(1, &tex_handle);
    glBindTexture(GL_TEXTURE_2D, tex_handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int tex_width, tex_height, tex_num_chan;
    unsigned char *tex_data = stbi_load(tex_filepath.c_str(), &tex_width, &tex_height, &tex_num_chan, 4);

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

    g_app.heightMapDim.x = tex_width;
    g_app.heightMapDim.y = tex_height;
    stbi_image_free(tex_data);

    return tex_handle;
}


void createTileTransformTable()
{
    g_app.tileTransformTable.clear();
    g_app.tileTransformTable.reserve(g_app.numClipmapLevels);

    for (uint32_t level = 0u; level < g_app.numClipmapLevels; ++level)
    {
        const float scale = (1<<level);

        std::array<glm::vec2, 12>& arr = g_app.tileTransformTable[level];

        arr[0] = glm::vec2(scale, scale * g_app.tileDim + scale);
        arr[1] = glm::vec2(scale * g_app.tileDim + scale, scale * g_app.tileDim + scale);

        arr[2] = glm::vec2(scale * g_app.tileDim + scale, scale);
        arr[3] = glm::vec2(scale * g_app.tileDim + scale, -scale * g_app.tileDim);

        arr[4] = glm::vec2(scale * g_app.tileDim + scale, -2.0f * scale * static_cast<float>(g_app.tileDim));
        arr[5] = glm::vec2(scale, -2.0f * scale * static_cast<float>(g_app.tileDim));
        arr[6] = glm::vec2(-scale * static_cast<float>(g_app.tileDim), -2.0f * scale * static_cast<float>(g_app.tileDim));
        arr[7] = glm::vec2(-2.0f * scale * static_cast<float>(g_app.tileDim), -2.0f * scale * static_cast<float>(g_app.tileDim));

        arr[8] = glm::vec2(-2.0f * scale * static_cast<float>(g_app.tileDim), -scale * g_app.tileDim);
        arr[9] = glm::vec2(-2.0f * scale * static_cast<float>(g_app.tileDim), scale);

        arr[10] = glm::vec2(-2.0f * scale * static_cast<float>(g_app.tileDim), scale * g_app.tileDim + scale);
        arr[11] = glm::vec2(-scale * static_cast<float>(g_app.tileDim), scale * g_app.tileDim + scale);
    }
}

void createMeshes()
{
    glDeleteBuffers(BUFFER_COUNT, g_gl.buffers);
    glDeleteVertexArrays(VERTEXARRAY_COUNT, g_gl.vertexArrays);

    assert(g_app.tileDim <= 255);

    struct Vertex
    {
        float pos[3];

        Vertex()
            : pos{0.0f, 0.0f, 0.0f}
        {
        }

        Vertex(float x, float y, float z)
            : pos{x, y, z}
        {
        }
    };


    // Tile mesh
    {
        std::vector<Vertex> vertices;

        for (size_t y = 0; y < (g_app.tileDim + 1u); ++y)
        {
            for (size_t x = 0; x < (g_app.tileDim + 1u); ++x)
            {
                vertices.emplace_back(x, 0.0f, y);
            }
        }
        
        const size_t numIndices = 6u * g_app.tileDim * g_app.tileDim;
        std::vector<uint32_t> indices(numIndices);

        size_t idx = 0;
        for (uint32_t y = 0u; y < g_app.tileDim; ++y)
        {
            const uint32_t start = y * g_app.tileDim + y;
            const uint32_t end = start + g_app.tileDim + 1u;
            for (uint32_t x = 0u; x < g_app.tileDim; ++x)
            {
                indices[idx++] = start + x;
                indices[idx++] = end + x;
                indices[idx++] = start + 1u + x;

                indices[idx++] = end + x;
                indices[idx++] = end + 1u + x;
                indices[idx++] = start + 1u + x;
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
        g_app.indexCounts[VERTEXARRAY_TILE] = numIndices;
        g_app.meshDebugColors[VERTEXARRAY_TILE] = {0.3f, 0.3f, 0.3f};
    }

    // Filler mesh
    {
        const size_t mesh_vertex_count = 2u * (g_app.tileDim + 1u);
        const size_t vertex_count = 4u * mesh_vertex_count;
        std::vector<Vertex> vertices(vertex_count);

        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x <= g_app.tileDim; ++x)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(g_app.tileDim + x + 1), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == mesh_vertex_count);

        for (int y = 0; y < 2u; ++y)
        {
            for (int x = g_app.tileDim; x >= 0; --x)
            {
                vertices[v_idx++] = Vertex(-1.0f * static_cast<float>(g_app.tileDim + x), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == 2u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = 0; y <= g_app.tileDim; ++y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, static_cast<float>(g_app.tileDim + y + 1));
            }
        }

        assert(v_idx == 3u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = g_app.tileDim; y >= 0; --y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, -1.0f * static_cast<float>(g_app.tileDim + y));
            }
        }

        assert(v_idx == 4u * mesh_vertex_count);

        const size_t mesh_index_count = 6u * g_app.tileDim;
        const size_t index_count = 4u * mesh_index_count;
        std::vector<uint32_t> indices(index_count);

        size_t idx = 0u;
        for (uint32_t patch = 0u; patch < 4u; ++patch)
        {
            for (uint32_t i = 0; i < g_app.tileDim; ++i)
            {
                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 1u;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 2u;

                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 2u;
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
        g_app.indexCounts[VERTEXARRAY_FILLER] = index_count;
        g_app.meshDebugColors[VERTEXARRAY_FILLER] = { 0.13f, 0.87f, 0.31f };
    }

    // Cross mesh
    {
        const size_t mesh_vertex_count = 2u * (g_app.tileDim + 1u);
        const size_t vertex_count = 4u * mesh_vertex_count;
        std::vector<Vertex> vertices(vertex_count);

        size_t v_idx = 0u;
        for (int y = 0; y < 2u; ++y)
        {
            for (int x = 0; x <= g_app.tileDim; ++x)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x + 1), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == mesh_vertex_count);

        for (int y = 0; y < 2u; ++y)
        {
            for (int x = g_app.tileDim; x >= 0; --x)
            {
                vertices[v_idx++] = Vertex(-1.0f * static_cast<float>(x), 0.0f, static_cast<float>(y));
            }
        }

        assert(v_idx == 2u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = 0; y <= g_app.tileDim; ++y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, static_cast<float>(y + 1));
            }
        }

        assert(v_idx == 3u * mesh_vertex_count);

        for (int x = 0; x < 2u; ++x)
        {
            for (int y = g_app.tileDim; y >= 0; --y)
            {
                vertices[v_idx++] = Vertex(static_cast<float>(x), 0.0f, -1.0f * static_cast<float>(y));
            }
        }

        assert(v_idx == 4u * mesh_vertex_count);

        const size_t mesh_index_count = 6u * g_app.tileDim;
        const size_t index_count = 4u * mesh_index_count;
        std::vector<uint32_t> indices(index_count);

        size_t idx = 0u;
        for (uint32_t patch = 0u; patch < 4u; ++patch)
        {
            for (uint32_t i = 0; i < g_app.tileDim; ++i)
            {
                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 1u;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 2u;

                indices[idx++] = patch * mesh_vertex_count + i;
                indices[idx++] = patch * mesh_vertex_count + i + g_app.tileDim + 2u;
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

        g_app.indexCounts[VERTEXARRAY_CROSS] = index_count;
        g_app.meshDebugColors[VERTEXARRAY_CROSS] = {1.0f, 0.0f, 0.0f};
    }

    // Trim mesh
    {
        const size_t inner_width = (g_app.tileDim * 4u + 1u) + 2u;
        const size_t vertex_count = 2u * 2u * inner_width;
        std::vector<Vertex> vertices(vertex_count);

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
            vertices[i].pos[0] -= g_app.tileDim * 2u + 1.5f;
            vertices[i].pos[2] -= g_app.tileDim * 2u + 1.5f;
        }

        assert(v_idx == vertex_count);

        const size_t index_count = 2u * (inner_width - 1) * 6u;
        std::vector<uint32_t> indices(index_count);

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
        g_app.indexCounts[VERTEXARRAY_TRIM] = indices.size();
        g_app.meshDebugColors[VERTEXARRAY_TRIM] = {0.0f, 0.0f, 1.0f};
    }

    // Seam mesh
    {
        const size_t inner_width = (g_app.tileDim * 4u + 1u) + 1u;
        const size_t vertex_count = 4u * (inner_width);
        std::vector<Vertex> vertices(vertex_count);

        for (int i = 0; i < inner_width; i++)
        {
            vertices[inner_width * 0 + i] = {static_cast<float>(i), 0, 0};
            vertices[inner_width * 1 + i] = {static_cast<float>(inner_width), 0, static_cast<float>(i)};
            vertices[inner_width * 2 + i] = {static_cast<float>(inner_width) - static_cast<float>(i), 0, static_cast<float>(inner_width)};
            vertices[inner_width * 3 + i] = {0, 0, static_cast<float>(inner_width) - static_cast<float>(i)};
        }

        const size_t index_count = inner_width * 6u;
        std::vector<uint32_t> indices(index_count);
        size_t n = 0;

        for (uint32_t i = 0; i < inner_width * 4; i += 2)
        {
            indices[n++] = i + 1;
            indices[n++] = i;
            indices[n++] = i + 2;
        }

        // make the last triangle wrap around
        indices[indices.size() - 1] = 0;

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
        g_app.indexCounts[VERTEXARRAY_SEAM] = indices.size();
    }

}

void init()
{
    g_gl.programs[PROGRAM_DEFAULT] = createProgram("../shaders/default.vert", "../shaders/default.frag", "default");

    g_camera.camera.setPerspectiveProjection(glm::radians(50.f), VIEWER_WIDTH / VIEWER_HEIGHT, 0.1f, 20000.f);
    updateCameraMatrix();

    g_gl.textures[TEXTURE_HEIGHTMAP] = create_texture_2d("../assets/test3.png");

    g_app.trimRotMatrices[0] = glm::mat4 { 1.0f };
    glm::mat4 rotMat { 1.0f };
    rotMat = glm::rotate(rotMat, -glm::pi<float>() - glm::half_pi<float>() , glm::vec3(0.0f, 1.0f, 0.0f));
    g_app.trimRotMatrices[1] = rotMat;
    rotMat = glm::mat4 { 1.0f };
    rotMat = glm::rotate(rotMat, -glm::half_pi<float>() , glm::vec3(0.0f, 1.0f, 0.0f));
    g_app.trimRotMatrices[2] = rotMat;
    rotMat = glm::mat4 { 1.0f };
    rotMat = glm::rotate(rotMat, -glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    g_app.trimRotMatrices[3] = rotMat;

    createMeshes();

    createTileTransformTable();
}

void renderMesh(size_t programId, size_t meshId, const glm::mat4 &rotation, float scale, glm::vec2 translation)
{
    const GLuint programHandle = g_gl.programs[programId];

    set_uni_vec2(programHandle, "u_translation", translation);
    set_uni_float(programHandle, "u_scale", scale);
    set_uni_mat4(programHandle, "u_rotMatrix", rotation);
    set_uni_vec3(programHandle, "u_debugColor", g_app.meshDebugColors[meshId]);

    glBindVertexArray(g_gl.vertexArrays[meshId]);
    glDrawElements(GL_TRIANGLES, g_app.indexCounts[meshId], GL_UNSIGNED_INT, nullptr);
}

void render()
{
    static const glm::mat4 identMat{1.0f};

    glClearColor(0.12f, 0.68f, 0.87f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_app.showWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(g_gl.programs[PROGRAM_DEFAULT]);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_HEIGHTMAP]);

    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_projMatrix", g_camera.camera.getProjection());
    set_uni_mat4(g_gl.programs[PROGRAM_DEFAULT], "u_viewMatrix", g_camera.view);
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_tileResolution", static_cast<float>(g_app.tileDim));
    set_uni_float(g_gl.programs[PROGRAM_DEFAULT], "u_heightScale", g_app.heightScale);
    set_uni_int(g_gl.programs[PROGRAM_DEFAULT], "u_showDebug", g_app.showDebugColors);

    const glm::vec2 view_pos = (g_app.freezeCamera) ? glm::vec2(g_app.frozenCamPos.x, g_app.frozenCamPos.z) : glm::vec2(g_camera.pos.x, g_camera.pos.z);

    // Constant tiles (will always be rendered regardless of clipmap configuration)
    {
        const glm::vec2 snapped_pos = glm::floor(view_pos);

        // Cross mesh
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_CROSS, identMat, 1.0f, snapped_pos);

        // Inner 4 tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, 1.0f, snapped_pos + glm::vec2(1.0f, 1.0f));
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, 1.0f, snapped_pos + glm::vec2(1.0f, -static_cast<float>(g_app.tileDim)));
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, 1.0f, snapped_pos + glm::vec2(-static_cast<float>(g_app.tileDim), -static_cast<float>(g_app.tileDim)));
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, 1.0f, snapped_pos + glm::vec2(-static_cast<float>(g_app.tileDim), 1.0f));;
    }

    for (uint32_t level = 0u; level < g_app.numClipmapLevels; level++)
    {
        const float scale = (1u << level);
        const glm::vec2 snapped_pos = glm::floor(view_pos / scale) * scale;
        const glm::vec2 tile_size{g_app.tileDim << level, g_app.tileDim << level};
        const glm::vec2 base = snapped_pos - glm::vec2(g_app.tileDim << (level + 1), g_app.tileDim << (level + 1));

        // Top tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][0]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][1]);

        // Right tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][2]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][3]);

        // Bot tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][4]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][5]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][6]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][7]);

        // Left tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][8]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][9]);

        // Top tiles
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][10]);
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TILE, identMat, scale, snapped_pos + g_app.tileTransformTable[level][11]);

        // Filler
        renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_FILLER, identMat, scale, snapped_pos);

        if (level != (g_app.numClipmapLevels - 1))
        {
            const float next_scale = scale * 2.0f;
            const glm::vec2 next_snapped_pos = glm::floor(view_pos / next_scale) * next_scale;

            // Trim
            const glm::vec2 tile_center = snapped_pos + glm::vec2(scale * 0.5f, scale * 0.5f);
            glm::vec2 d = view_pos - next_snapped_pos;

            uint32_t r = 0;
            r |= d.x >= scale ? 0 : 2;
            r |= d.y >= scale ? 0 : 1;

            renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_TRIM, g_app.trimRotMatrices[r], scale, tile_center);

            // Seam
            if (g_app.seamsEnabled)
            {
                glm::vec2 next_base = next_snapped_pos - glm::vec2(static_cast<float>(g_app.tileDim << (level + 1), static_cast<float>(g_app.tileDim << (level + 1))));
                renderMesh(PROGRAM_DEFAULT, VERTEXARRAY_SEAM, identMat, scale, next_base);
            }
        }
    }
}

void gui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Gui"))
    {
        int tileRes = static_cast<int>(g_app.tileDim);
        if (ImGui::DragInt("Tile Resolution", &tileRes, 1.0f, 2, 255))
        {
            g_app.tileDim = static_cast<uint32_t>(tileRes);
            createTileTransformTable();
            createMeshes();
        }

        int numLevels = static_cast<int>(g_app.numClipmapLevels);
        if (ImGui::DragInt("Levels", &numLevels, 0.5f, 1, 12))
        {
            g_app.numClipmapLevels = static_cast<uint32_t>(numLevels);
            createTileTransformTable();
            createMeshes();
        }
        
        ImGui::Checkbox("Wireframe", &g_app.showWireframe);
        ImGui::SliderFloat("Height Scale", &g_app.heightScale, 0.1f, 200.0f, "%.2f");
        ImGui::Checkbox("Seams", &g_app.seamsEnabled);
        ImGui::Checkbox("Debug", &g_app.showDebugColors);

        if (ImGui::InputFloat3("View Pos", &(g_camera.pos[0])))
        {
            updateCameraMatrix();
        }
        if (ImGui::Checkbox("Freeze Camera", &g_app.freezeCamera))
        {
            g_app.frozenCamPos = g_camera.pos;
        }
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
    GLFWwindow *window = glfwCreateWindow(
        VIEWER_WIDTH, VIEWER_HEIGHT,
        "Geometry Clipmaps Demo", nullptr, nullptr);
    if (window == nullptr)
    {
        LOG("=> Failure <=\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, &cursorPosCallback);
    glfwSetScrollCallback(window, &mouseScrollCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}\n");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LOG("gladLoadGLLoader failed\n");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    LOG("-- Begin -- Demo\n");

    LOG("-- Begin -- Init\n");
    init();
    LOG("-- End -- Init\n");

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {
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