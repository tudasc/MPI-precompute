/*!
 * \file    OGLSpheresVisuGS.h
 * \brief   Class for spheres visualization with geometry shader in OpenGL.
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
#ifndef OGL_SPHERES_VISU_GS_H_
#define OGL_SPHERES_VISU_GS_H_

#include <map>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "OGLSpheresVisu.h"

template <typename T = double>
class OGLSpheresVisuGS : public OGLSpheresVisu<T> {
public:
  OGLSpheresVisuGS(const std::string winName, const int winWidth,
                   const int winHeight, const T *positionsX,
                   const T *positionsY, const T *positionsZ, const T *radius,
                   const unsigned long nSpheres);

  virtual ~OGLSpheresVisuGS();

  void refreshDisplay();
};

#endif /* OGL_SPHERES_VISU_GS_H_ */
#endif
