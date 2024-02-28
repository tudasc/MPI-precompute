/*!
 * \file    SimulationNBodyV3.hxx
 * \brief   Implementation of SimulationNBodyLocal with the softening factor (n²
 * computations). \author  A. Cassagne \date    2014
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

#include "SimulationNBodyV3.h"

template <typename T>
SimulationNBodyV3<T>::SimulationNBodyV3(const unsigned long nBodies,
                                        T softening)
    : SimulationNBodyLocal<T>(nBodies),
      softeningSquared(softening * softening) {
  this->init();
}

template <typename T>
SimulationNBodyV3<T>::SimulationNBodyV3(const std::string inputFileName,
                                        T softening)
    : SimulationNBodyLocal<T>(inputFileName),
      softeningSquared(softening * softening) {
  this->init();
}

template <typename T> void SimulationNBodyV3<T>::init() {
  this->flopsPerIte =
      19.f * ((float)this->bodies->getN() - 1.f) * (float)this->bodies->getN();
}

template <typename T> SimulationNBodyV3<T>::~SimulationNBodyV3() {}

template <typename T> void SimulationNBodyV3<T>::initIteration() {
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++) {
    this->accelerations.x[iBody] = 0.0;
    this->accelerations.y[iBody] = 0.0;
    this->accelerations.z[iBody] = 0.0;

    this->closestNeighborDist[iBody] = std::numeric_limits<T>::infinity();
  }
}

template <typename T>
void SimulationNBodyV3<T>::computeLocalBodiesAcceleration() {
  const T *masses = this->getBodies()->getMasses();

  const T *positionsX = this->getBodies()->getPositionsX();
  const T *positionsY = this->getBodies()->getPositionsY();
  const T *positionsZ = this->getBodies()->getPositionsZ();

#pragma omp parallel for schedule(runtime)
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
    for (unsigned long jBody = 0; jBody < this->bodies->getN(); jBody++)
      SimulationNBodyV3<T>::computeAccelerationBetweenTwoBodies(
          this->G, this->softeningSquared, positionsX[iBody], positionsY[iBody],
          positionsZ[iBody], this->accelerations.x[iBody],
          this->accelerations.y[iBody], this->accelerations.z[iBody],
          this->closestNeighborDist[iBody], masses[jBody], positionsX[jBody],
          positionsY[jBody], positionsZ[jBody]);
}

// 19 flops
template <typename T>
void SimulationNBodyV3<T>::computeAccelerationBetweenTwoBodies(
    const T &G, const T &softSquared, const T &qiX, const T &qiY, const T &qiZ,
    T &aiX, T &aiY, T &aiZ, T &closNeighi, const T &mj, const T &qjX,
    const T &qjY, const T &qjZ) {
  const T rijX = qjX - qiX; // 1 flop
  const T rijY = qjY - qiY; // 1 flop
  const T rijZ = qjZ - qiZ; // 1 flop

  // compute the distance (|| rij ||² + epsilon²) between body i and body j
  const T rijSquared =
      (rijX * rijX) + (rijY * rijY) + (rijZ * rijZ) + softSquared; // 6 flops

  // compute the distance ~= || rij ||
  const T rij = std::sqrt(rijSquared); // 1 flop

  // compute the acceleration value between body i and body j: || ai || = G.mj /
  // || rij ||^3
  const T ai = G * mj / (rijSquared * rij); // 3 flops

  // add the acceleration value into the acceleration vector: ai += || ai ||.rij
  aiX += ai * rijX; // 2 flops
  aiY += ai * rijY; // 2 flops
  aiZ += ai * rijZ; // 2 flops

  closNeighi = std::min(closNeighi, rij);
}
