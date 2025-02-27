/*!
 * \file    SpheresVisuNo.h
 * \brief   No visu.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#ifndef OGL_SPHERES_VISU_NO_H_
#define OGL_SPHERES_VISU_NO_H_

#include "SpheresVisu.h"

template <typename T = double> class SpheresVisuNo : public SpheresVisu {
public:
  SpheresVisuNo();

  virtual ~SpheresVisuNo();

  void refreshDisplay();
  bool windowShouldClose();
  bool pressedSpaceBar();
  bool pressedPageUp();
  bool pressedPageDown();
};

#endif /* OGL_SPHERES_VISU_NO_H_ */
