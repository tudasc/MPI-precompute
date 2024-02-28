/*!
 * \file    OGLSpheresVisu.h
 * \brief   Abstract class for spheres visualization in OpenGL.
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
#ifndef OGL_SPHERES_VISU_H_
#define OGL_SPHERES_VISU_H_

#include <map>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "SpheresVisu.h"

#include "OGLControl.h"

template <typename T = double> class OGLSpheresVisu : public SpheresVisu {
protected:
  GLFWwindow *window;

  const T *positionsX;
  float *positionsXBuffer;
  const T *positionsY;
  float *positionsYBuffer;
  const T *positionsZ;
  float *positionsZBuffer;
  const T *radius;
  float *radiusBuffer;

  const unsigned long nSpheres;

  GLuint vertexArrayRef;
  GLuint positionBufferRef[3];
  GLuint radiusBufferRef;
  GLuint mvpRef;
  GLuint shaderProgramRef;

  glm::mat4 mvp;

  OGLControl *control;

protected:
  OGLSpheresVisu(const std::string winName, const int winWidth,
                 const int winHeight, const T *positionsX, const T *positionsY,
                 const T *positionsZ, const T *radius,
                 const unsigned long nSpheres);
  OGLSpheresVisu();

public:
  virtual ~OGLSpheresVisu();
  virtual void refreshDisplay() = 0;
  bool windowShouldClose();
  bool pressedSpaceBar();
  bool pressedPageUp();
  bool pressedPageDown();

protected:
  bool compileShaders(const std::vector<GLenum> shadersType,
                      const std::vector<std::string> shadersFiles);
  void updatePositions();
};

#endif /* OGL_SPHERES_VISU_H_ */
#endif
