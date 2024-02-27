/*!
 * \file    SimulationNBodyCollisionV1.hxx
 * \brief   Naive implementation of SimulationNBodyCollisionLocal (n²
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

#include "SimulationNBodyCollisionV1.h"

template <typename T>
SimulationNBodyCollisionV1<T>::SimulationNBodyCollisionV1(
    const unsigned long nBodies)
    : SimulationNBodyCollisionLocal<T>(nBodies) {
  this->init();
}

template <typename T>
SimulationNBodyCollisionV1<T>::SimulationNBodyCollisionV1(
    const std::string inputFileName)
    : SimulationNBodyCollisionLocal<T>(inputFileName) {
  this->init();
}

template <typename T> void SimulationNBodyCollisionV1<T>::init() {
  this->flopsPerIte =
      20.f * ((float)this->bodies->getN() - 1.f) * (float)this->bodies->getN();
}

template <typename T>
SimulationNBodyCollisionV1<T>::~SimulationNBodyCollisionV1() {}

template <typename T> void SimulationNBodyCollisionV1<T>::initIteration() {
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++) {
    this->accelerations.x[iBody] = 0.0;
    this->accelerations.y[iBody] = 0.0;
    this->accelerations.z[iBody] = 0.0;

    this->closestNeighborDist[iBody] = std::numeric_limits<T>::infinity();

    this->collisions[iBody].clear();
    // if(this->collisions[iBody].capacity() < 4)
    //	this->collisions[iBody].resize(4);
  }
}

template <typename T>
void SimulationNBodyCollisionV1<T>::computeLocalBodiesAcceleration() {
  const T *masses = this->bodies->getMasses();
  const T *radiuses = this->bodies->getRadiuses();
  const T *positionsX = this->bodies->getPositionsX();
  const T *positionsY = this->bodies->getPositionsY();
  const T *positionsZ = this->bodies->getPositionsZ();

#pragma omp parallel for schedule(runtime)
  for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
    for (unsigned long jBody = 0; jBody < this->bodies->getN(); jBody++)
      if (iBody != jBody) {
        T dist =
            SimulationNBodyCollisionV1<T>::computeAccelerationBetweenTwoBodies(
                this->G, radiuses[iBody], positionsX[iBody], positionsY[iBody],
                positionsZ[iBody], this->accelerations.x[iBody],
                this->accelerations.y[iBody], this->accelerations.z[iBody],
                this->closestNeighborDist[iBody], masses[jBody],
                radiuses[jBody], positionsX[jBody], positionsY[jBody],
                positionsZ[jBody]);
        if (dist <= 0)
          this->collisions[iBody].push_back(jBody);
      }
}

// 20 flops
template <typename T>
T SimulationNBodyCollisionV1<T>::computeAccelerationBetweenTwoBodies(
    const T &G, const T &ri, const T &qiX, const T &qiY, const T &qiZ, T &aiX,
    T &aiY, T &aiZ, T &closNeighi, const T &mj, const T &rj, const T &qjX,
    const T &qjY, const T &qjZ) {
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

  return rij - (ri + rj); // 2 flops
}
