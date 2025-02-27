/*!
 * \file    OGLSpheresVisuGS.hxx
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
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "OGLSpheresVisuGS.h"

#include "OGLTools.h"

template <typename T>
OGLSpheresVisuGS<T>::OGLSpheresVisuGS(const std::string winName,
                                      const int winWidth, const int winHeight,
                                      const T *positionsX, const T *positionsY,
                                      const T *positionsZ, const T *radius,
                                      const unsigned long nSpheres)
    : OGLSpheresVisu<T>(winName, winWidth, winHeight, positionsX, positionsY,
                        positionsZ, radius, nSpheres) {
  if (this->window) {
    // specify shaders path and compile them
    std::vector<GLenum> shadersType(3);
    std::vector<std::string> shadersFiles(3);
    shadersType[0] = GL_VERTEX_SHADER;
    shadersFiles[0] = "../src/common/ogl/shaders/vertex150.glsl";
    shadersType[1] = GL_GEOMETRY_SHADER;
    shadersFiles[1] = "../src/common/ogl/shaders/geometry150.glsl";
    shadersType[2] = GL_FRAGMENT_SHADER;
    shadersFiles[2] = "../src/common/ogl/shaders/fragment150.glsl";

    this->compileShaders(shadersType, shadersFiles);
  }
}

template <typename T> OGLSpheresVisuGS<T>::~OGLSpheresVisuGS() {}

template <typename T> void OGLSpheresVisuGS<T>::refreshDisplay() {
  if (this->window) {
    this->updatePositions();

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // use our shader program
    if (this->shaderProgramRef != 0)
      glUseProgram(this->shaderProgramRef);

    // 1rst attribute buffer : vertex positions
    int iBufferIndex;
    for (iBufferIndex = 0; iBufferIndex < 3; iBufferIndex++) {
      glEnableVertexAttribArray(iBufferIndex);
      glBindBuffer(GL_ARRAY_BUFFER, this->positionBufferRef[iBufferIndex]);
      glVertexAttribPointer(
          iBufferIndex, // attribute. No particular reason for 0, but must match
                        // the layout in the shader.
          1,            // size
          GL_FLOAT,     // type
          GL_FALSE,     // normalized?
          0,            // stride
          (void *)0     // array buffer offset
      );
    }

    // 2nd attribute buffer : radius
    glEnableVertexAttribArray(iBufferIndex);
    glBindBuffer(GL_ARRAY_BUFFER, this->radiusBufferRef);
    glVertexAttribPointer(
        iBufferIndex++, // attribute. No particular reason for 1, but must match
                        // the layout in the shader.
        1,              // size
        GL_FLOAT,       // type
        GL_FALSE,       // normalized?
        0,              // stride
        (void *)0       // array buffer offset
    );

    // Compute the MVP matrix from keyboard and mouse input
    this->mvp = this->control->computeViewAndProjectionMatricesFromInputs();

    // Send our transformation to the currently bound shader,
    // in the "MVP" uniform
    glUniformMatrix4fv(this->mvpRef, 1, GL_FALSE, &this->mvp[0][0]);

    // Draw the triangle !
    glDrawArrays(GL_POINTS, 0, this->nSpheres);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);

    // Swap front and back buffers
    glfwSwapBuffers(this->window);

    // Poll for and process events
    glfwPollEvents();

    // Sleep if necessary
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class OGLSpheresVisuGS<double>;
#else
template class OGLSpheresVisuGS<float>;
#endif
// ====================================================================================
// explicit template instantiation
#endif
