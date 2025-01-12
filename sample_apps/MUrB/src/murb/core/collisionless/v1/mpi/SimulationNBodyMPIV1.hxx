/*!
 * \file    SimulationNBodyMPIV1.hxx
 * \brief   Naive implementation of SimulationNBodyMPI (n² computations).
 * \author  A. Cassagne
 * \date    2014
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

#include "SimulationNBodyMPIV1.h"

template <typename T>
SimulationNBodyMPIV1<T>::SimulationNBodyMPIV1(const unsigned long nBodies)
    : SimulationNBodyMPI<T>(nBodies) {
  this->init();
}

template <typename T>
SimulationNBodyMPIV1<T>::SimulationNBodyMPIV1(const std::string inputFileName)
    : SimulationNBodyMPI<T>(inputFileName) {
  this->init();
}

template <typename T> void SimulationNBodyMPIV1<T>::init() {
  this->flopsPerIte =
      18.f * (((float)this->bodies->getN() * (float)this->MPISize) - 1) *
      ((float)this->bodies->getN() * (float)this->MPISize);
}

template <typename T> SimulationNBodyMPIV1<T>::~SimulationNBodyMPIV1() {}

template <typename T> void SimulationNBodyMPIV1<T>::initIteration() {
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++) {
    this->accelerations.x[iBody] = 0.0;
    this->accelerations.y[iBody] = 0.0;
    this->accelerations.z[iBody] = 0.0;

    this->closestNeighborDist[iBody] = std::numeric_limits<T>::infinity();
  }
}

template <typename T>
void SimulationNBodyMPIV1<T>::computeLocalBodiesAcceleration() {
  const T *masses = this->bodies->getMasses();
  const T *positionsX = this->bodies->getPositionsX();
  const T *positionsY = this->bodies->getPositionsY();
  const T *positionsZ = this->bodies->getPositionsZ();

#pragma omp parallel for schedule(runtime)
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
    for (unsigned long jBody = 0; jBody < this->bodies->getN(); jBody++)
      if (iBody != jBody)
        SimulationNBodyMPIV1<T>::computeAccelerationBetweenTwoBodies(
            this->G, positionsX[iBody], positionsY[iBody], positionsZ[iBody],
            this->accelerations.x[iBody], this->accelerations.y[iBody],
            this->accelerations.z[iBody], this->closestNeighborDist[iBody],
            masses[jBody], positionsX[jBody], positionsY[jBody],
            positionsZ[jBody]);
}

template <typename T>
void SimulationNBodyMPIV1<T>::computeNeighborBodiesAcceleration() {
  const T *positionsX = this->bodies->getPositionsX();
  const T *positionsY = this->bodies->getPositionsY();
  const T *positionsZ = this->bodies->getPositionsZ();

  const T *neighMasses = this->neighborBodies->getMasses();
  const T *neighPositionsX = this->neighborBodies->getPositionsX();
  const T *neighPositionsY = this->neighborBodies->getPositionsY();
  const T *neighPositionsZ = this->neighborBodies->getPositionsZ();

#pragma omp parallel for schedule(runtime)
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
    for (unsigned long jBody = 0; jBody < this->bodies->getN(); jBody++)
      SimulationNBodyMPIV1<T>::computeAccelerationBetweenTwoBodies(
          this->G, positionsX[iBody], positionsY[iBody], positionsZ[iBody],
          this->accelerations.x[iBody], this->accelerations.y[iBody],
          this->accelerations.z[iBody], this->closestNeighborDist[iBody],
          neighMasses[jBody], neighPositionsX[jBody], neighPositionsY[jBody],
          neighPositionsZ[jBody]);
}

// 18 flops
template <typename T>
void SimulationNBodyMPIV1<T>::computeAccelerationBetweenTwoBodies(
    const T &G, const T &qiX, const T &qiY, const T &qiZ, T &aiX, T &aiY,
    T &aiZ, T &closNeighi, const T &mj, const T &qjX, const T &qjY,
    const T &qjZ) {
  const T rijX = qjX - qiX; // 1 flop
  const T rijY = qjY - qiY; // 1 flop
  const T rijZ = qjZ - qiZ; // 1 flop

  // compute the distance || rij ||² between body i and body j
  const T rijSquared = (rijX * rijX) + (rijY * rijY) + (rijZ * rijZ); // 5 flops

  // compute the distance || rij ||
  const T rij = std::sqrt(rijSquared); // 1 flop

  // we cannot divide by 0
  assert(rij != 0);

  // compute the acceleration value between body i and body j: || ai || = G.mj /
  // || rij ||^3
  const T ai = G * mj / (rijSquared * rij); // 3 flops

  // add the acceleration value into the acceleration vector: ai += || ai ||.rij
  aiX += ai * rijX; // 2 flops
  aiY += ai * rijY; // 2 flops
  aiZ += ai * rijZ; // 2 flops

  closNeighi = std::min(closNeighi, rij);
}
