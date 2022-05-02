#include <sstream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Helpers.hpp"
#include "Defines.hpp"

static std::string read_file(const std::string filepath)
{
   std::ifstream ifs { filepath, std::ios::in };

   if (ifs.is_open())
   {
      std::ostringstream ss;
      ss << ifs.rdbuf();

      ifs.close();

      return ss.str();
   }
   else
      EXIT("Failed to open file " + filepath);
}

static void check_compilation_status(const GLuint shader_handle, const std::string& shader_path)
{
   GLint success;
   char info_log[512];
   glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &success);
   if (!success)
   {
      glGetShaderInfoLog(shader_handle, 512, NULL, info_log);
      std::cout << info_log << '\n';
      EXIT("ERROR: " + shader_path + " compilation failed!\n " + std::string(info_log));
   }
}

static GLuint compile_shader(const std::string shader_path, const uint64_t gl_shader_type)
{
   const std::string shader_source = read_file(shader_path);
   const char* shader_source_cstr = shader_source.c_str();

   const GLuint shader_handle = glCreateShader(gl_shader_type);
   glShaderSource(shader_handle, 1, &shader_source_cstr, NULL);
   glCompileShader(shader_handle);
   check_compilation_status(shader_handle, shader_path);
   return shader_handle;
}

static void check_link_status(const GLuint shader_program, const std::string& program_name)
{
   GLint success;
   char info_log[512];
   glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
   if (!success)
   {
      glGetProgramInfoLog(shader_program, 512, NULL, info_log);
      std::cout << info_log << '\n';
      EXIT("ERROR: " + program_name + " linking failed!\n " + std::string(info_log));
   }
}

GLuint createProgram(std::string vertexPath, std::string fragmentPath, std::string programName)
{
   const GLuint vertex_shader_handle   = compile_shader(vertexPath, GL_VERTEX_SHADER);
   const GLuint fragment_shader_handle = compile_shader(fragmentPath, GL_FRAGMENT_SHADER);

   GLuint programHandle = glCreateProgram();
   glAttachShader(programHandle, vertex_shader_handle);
   glAttachShader(programHandle, fragment_shader_handle);
   glLinkProgram(programHandle);
   check_link_status(programHandle, programName);

   glDeleteShader(vertex_shader_handle);
   glDeleteShader(fragment_shader_handle);

   return programHandle;
}

// GLuint create_texture_2d16(const std::string tex_filepath)
// {
//    GLuint tex_handle;
//    glGenTextures(1, &tex_handle);
//    glBindTexture(GL_TEXTURE_2D, tex_handle);

//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//    stbi_set_flip_vertically_on_load(true); 
//    int tex_width, tex_height, tex_num_chan;
//    unsigned char* tex_data = (unsigned char*)stbi_load_16(tex_filepath.c_str(), &tex_width, &tex_height, &tex_num_chan, 1);

//    if (!tex_data)
//       EXIT("Failed to load texture " + tex_filepath);

//    uint64_t format = 0x0;
//    switch (tex_num_chan)
//    {
//       case 1:
//          format = GL_RED;
//          break;
//       case 2:
//          format = GL_RG;
//          break;
//       default:
//          EXIT("Failed to load texture " + tex_filepath + " Format not supported!");
//    };

//    glTexImage2D(GL_TEXTURE_2D, 0, format, tex_width, tex_height, 0, format, GL_UNSIGNED_SHORT, tex_data);
//    glGenerateMipmap(GL_TEXTURE_2D);

//    stbi_image_free(tex_data);

//    return tex_handle;

// }

// GLuint create_texture_2d(const std::string tex_filepath)
// {
//    GLuint tex_handle;
//    glGenTextures(1, &tex_handle);
//    glBindTexture(GL_TEXTURE_2D, tex_handle);

//    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//    stbi_set_flip_vertically_on_load(true); 
//    int tex_width, tex_height, tex_num_chan;
//    unsigned char* tex_data = stbi_load(tex_filepath.c_str(), &tex_width, &tex_height, &tex_num_chan, 4);

//    if (!tex_data)
//       EXIT("Failed to load texture " + tex_filepath);

//    uint64_t format = 0x0;
//    switch (tex_num_chan)
//    {
//       case 1:
//          format = GL_RGBA;
//          break;
//       case 3:
//          format = GL_RGB;
//          break;
//       case 4:
//          format = GL_RGBA;
//          break;
//       default:
//          EXIT("Failed to load texture " + tex_filepath);
//    }

//    glTexImage2D(GL_TEXTURE_2D, 0, format, tex_width, tex_height, 0, format, GL_UNSIGNED_BYTE, tex_data);
//    glGenerateMipmap(GL_TEXTURE_2D);

//    stbi_image_free(tex_data);

//    return tex_handle;
// }