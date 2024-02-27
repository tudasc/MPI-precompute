/*!
 * \file    SpheresVisu.h
 * \brief   Abstract class for spheres visualization.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#ifndef SPHERES_VISU_H_
#define SPHERES_VISU_H_

class SpheresVisu {
protected:
  SpheresVisu() {}

public:
  virtual ~SpheresVisu() {}
  virtual void refreshDisplay() = 0;
  virtual bool windowShouldClose() = 0;
  virtual bool pressedSpaceBar() = 0;
  virtual bool pressedPageUp() = 0;
  virtual bool pressedPageDown() = 0;
};

#endif /* SPHERES_VISU_H_ */
