/*!
 * \file    SimulationNBodyLocal.hxx
 * \brief   Abstract n-body collision less simulation class for local
 * computations (inside a node). \author  A. Cassagne \date    2014
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

#include "SimulationNBodyLocal.h"

template <typename T>
SimulationNBodyLocal<T>::SimulationNBodyLocal(const unsigned long nBodies,
                                              const unsigned long randInit)
    : SimulationNBody<T>(new Bodies<T>(nBodies, randInit)) {}

template <typename T>
SimulationNBodyLocal<T>::SimulationNBodyLocal(const std::string inputFileName)
    : SimulationNBody<T>(new Bodies<T>(inputFileName)) {}

template <typename T> SimulationNBodyLocal<T>::~SimulationNBodyLocal() {
  delete this->bodies;
}

template <typename T> void SimulationNBodyLocal<T>::computeOneIteration() {
  this->initIteration();
  this->computeLocalBodiesAcceleration();
  if (!this->dtConstant)
    this->findTimeStep();
  this->bodies->updatePositionsAndVelocities(this->accelerations, this->dt);
}

template <typename T> void SimulationNBodyLocal<T>::findTimeStep() {
  // TODO: be careful with the V1Intrinsics version: with fake bodies added at
  // the end of the last vector, the
  //       dynamic time step is broken.
  //       It is necessary to launch the simulation with a number of bodies
  //       multiple of mipp::vectorSize<T>()!
  if (!this->dtConstant) {
    this->dt = std::numeric_limits<T>::infinity();
    for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++) {
      const T newDt = this->computeTimeStep(iBody);

      if (newDt < this->dt)
        this->dt = newDt;
    }

    if (this->dt < this->minDt)
      this->dt = this->minDt;
  }
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class SimulationNBodyLocal<double>;
#else
template class SimulationNBodyLocal<float>;
#endif
// ====================================================================================
// explicit template instantiation
