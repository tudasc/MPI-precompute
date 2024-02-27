/*!
 * \file    OGLTools.h
 * \brief   Basic tools for OpenGL dev.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#ifdef VISU
#ifndef OGLTOOLS_H_
#define OGLTOOLS_H_

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

class OGLTools {
public:
  static GLFWwindow *initAndMakeWindow(const int winWidth, const int winHeight,
                                       const std::string winName);

  static GLuint loadShaderFromFile(const GLenum shaderType,
                                   const std::string shaderFilePath);

  static GLuint linkShaders(const std::vector<GLuint> shaders);
};

#endif /* OGLTOOLS_H_ */
#endif /* VISU */
