/*!
 * \file    SpheresVisuNo.hxx
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
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "SpheresVisuNo.h"

template <typename T> SpheresVisuNo<T>::SpheresVisuNo() : SpheresVisu() {}

template <typename T> SpheresVisuNo<T>::~SpheresVisuNo() {}

template <typename T> void SpheresVisuNo<T>::refreshDisplay() {}

template <typename T> bool SpheresVisuNo<T>::windowShouldClose() {
  return false;
}

template <typename T> bool SpheresVisuNo<T>::pressedSpaceBar() { return false; }

template <typename T> bool SpheresVisuNo<T>::pressedPageUp() { return false; }

template <typename T> bool SpheresVisuNo<T>::pressedPageDown() { return false; }

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class SpheresVisuNo<double>;
#else
template class SpheresVisuNo<float>;
#endif
// ====================================================================================
// explicit template instantiation
