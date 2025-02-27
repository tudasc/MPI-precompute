/*!
 * \file    SimulationNBodyV1CB.hxx
 * \brief   Naive implementation of SimulationNBodyLocal with Cache Blocking
 * technique (n² computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#else
#ifndef NO_OMP
#define NO_OMP
inline void omp_set_num_threads(int) {}
inline int omp_get_num_threads() { return 1; }
inline int omp_get_max_threads() { return 1; }
inline int omp_get_thread_num() { return 0; }
#endif
#endif

#include "SimulationNBodyV1CB.h"

template <typename T>
SimulationNBodyV1CB<T>::SimulationNBodyV1CB(const unsigned long nBodies)
    : SimulationNBodyV1<T>(nBodies) {}

template <typename T>
SimulationNBodyV1CB<T>::SimulationNBodyV1CB(const std::string inputFileName)
    : SimulationNBodyV1<T>(inputFileName) {}

template <typename T> SimulationNBodyV1CB<T>::~SimulationNBodyV1CB() {}

/*
        AI  = (23 * blockSize * nBodies * nBlocks)  / ((4 * blockSize + 7 *
   nBodies) * nBlocks) <=> AI  = (23 * blockSize * nBodies)            /  (4 *
   blockSize + 7 * nBodies)            <=> AI  = (23 * blockSize * nBlock *
   blockSize) /  (4 * blockSize + 7 * nBlock * blockSize) <=> AI  = (23 * nBlock
   * blockSize²)            / ((4 + 7 * nBlock) * blockSize)            <=> AI
   = (23 * nBlock * blockSize)             /  (4 + 7 * nBlock) <=> AI ~= (23 *
   nBlock * blockSize)             /      (7 * nBlock) <=> AI ~= (23 *
   blockSize)                      /       7
        -------------------------------------------------------------------------------------------
        OI  = AI                                    /      sizeof(T) <=> OI  =
   (23 * blockSize)                      / (7 * sizeof(T))
*/
template <typename T>
void SimulationNBodyV1CB<T>::computeLocalBodiesAcceleration() {
  const T *masses = this->getBodies()->getMasses();

  const T *positionsX = this->getBodies()->getPositionsX();
  const T *positionsY = this->getBodies()->getPositionsY();
  const T *positionsZ = this->getBodies()->getPositionsZ();

  unsigned long blockSize = 512;
  // flops  = 23 * blockSize * nBodies      * nBlocks
  // memops = (4 * blockSize + 7 * nBodies) * nBlocks
  for (unsigned long jOff = 0; jOff < this->bodies->getN(); jOff += blockSize) {
    blockSize = std::min(blockSize, this->bodies->getN() - jOff);
    // flops  = 23 * blockSize * nBodies
    // memops =  4 * blockSize + 7 * nBodies
#pragma omp parallel for schedule(runtime)
    for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
      // flops  = 23 * blockSize
      // memops =  4 * blockSize + 7
      for (unsigned long jBody = jOff; jBody < jOff + blockSize; jBody++)
        if (iBody != jBody)
          SimulationNBodyV1<T>::computeAccelerationBetweenTwoBodies(
              this->G, positionsX[iBody], positionsY[iBody], positionsZ[iBody],
              this->accelerations.x[iBody], this->accelerations.y[iBody],
              this->accelerations.z[iBody], this->closestNeighborDist[iBody],
              masses[jBody], positionsX[jBody], positionsY[jBody],
              positionsZ[jBody]);
  }
}
