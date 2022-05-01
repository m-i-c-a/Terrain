#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <string>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

void createProgram(std::string vertexPath, std::string fragmentPath, std::string programName, GLuint& programHandle);

inline void set_uni_vec2(GLuint programHandle, const std::string& uni_name, const glm::vec2& vec2)
{ glUniform2fv(glGetUniformLocation(programHandle, uni_name.c_str()), 1, &(vec2[0])); }

inline void set_uni_vec3(GLuint programHandle, const std::string& uni_name, const glm::vec3& vec3)
{ glUniform3fv(glGetUniformLocation(programHandle, uni_name.c_str()), 1, &(vec3[0])); }

inline void set_uni_vec4(GLuint programHandle, const std::string& uni_name, const glm::vec4& vec4)
{ glUniform4fv(glGetUniformLocation(programHandle, uni_name.c_str()), 1, &(vec4[0])); }

inline void set_uni_mat4(GLuint programHandle, const std::string& uni_name, const glm::mat4& mat4)
{ glUniformMatrix4fv(glGetUniformLocation(programHandle, uni_name.c_str()), 1, GL_FALSE, glm::value_ptr(mat4)); }

inline void set_uni_int(GLuint programHandle, const std::string& uni_name, const GLint i)
{ glUniform1i(glGetUniformLocation(programHandle, uni_name.c_str()), i); }

inline void set_uni_float(GLuint programHandle, const std::string& uni_name, const GLfloat f)
{ glUniform1f(glGetUniformLocation(programHandle, uni_name.c_str()), f); }

#endif // HELPERS_HPP