#include <sstream>
#include <fstream>

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

void createProgram(std::string vertexPath, std::string fragmentPath, std::string programName, GLuint& programHandle)
{
   const GLuint vertex_shader_handle   = compile_shader(vertexPath, GL_VERTEX_SHADER);
   const GLuint fragment_shader_handle = compile_shader(fragmentPath, GL_FRAGMENT_SHADER);

   programHandle = glCreateProgram();
   glAttachShader(programHandle, vertex_shader_handle);
   glAttachShader(programHandle, fragment_shader_handle);
   glLinkProgram(programHandle);
   check_link_status(programHandle, programName);

   glDeleteShader(vertex_shader_handle);
   glDeleteShader(fragment_shader_handle);
}
