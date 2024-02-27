/*!
 * \file    SimulationNBody.hxx
 * \brief   Higher abstraction for all n-body simulation classes.
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
#include <mipp.h>
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

#include "SimulationNBody.h"

template <typename T>
SimulationNBody<T>::SimulationNBody(Bodies<T> *bodies)
    : bodies(bodies), closestNeighborDist(NULL), dtConstant(false),
      dt(std::numeric_limits<T>::infinity()), minDt(0), flopsPerIte(0),
      allocatedBytes(bodies->getAllocatedBytes()),
      nMaxThreads(omp_get_max_threads()) {
  assert(bodies != nullptr);

  this->allocateBuffers();
}

template <typename T> SimulationNBody<T>::~SimulationNBody() {
  if (this->accelerations.x != nullptr) {
    mipp::free(this->accelerations.x);
    this->accelerations.x = nullptr;
  }
  if (this->accelerations.y != nullptr) {
    mipp::free(this->accelerations.y);
    this->accelerations.y = nullptr;
  }
  if (this->accelerations.z != nullptr) {
    mipp::free(this->accelerations.z);
    this->accelerations.z = nullptr;
  }

  if (this->closestNeighborDist != nullptr) {
    mipp::free(this->closestNeighborDist);
    this->closestNeighborDist = nullptr;
  }
}

template <typename T> void SimulationNBody<T>::allocateBuffers() {
  this->accelerations.x =
      mipp::malloc<T>(this->bodies->getN() + this->bodies->getPadding());
  this->accelerations.y =
      mipp::malloc<T>(this->bodies->getN() + this->bodies->getPadding());
  this->accelerations.z =
      mipp::malloc<T>(this->bodies->getN() + this->bodies->getPadding());
  this->closestNeighborDist =
      mipp::malloc<T>(this->bodies->getN() + this->bodies->getPadding());

  this->allocatedBytes +=
      (this->bodies->getN() + this->bodies->getPadding()) * sizeof(T) * 4;
}

template <typename T> const Bodies<T> *SimulationNBody<T>::getBodies() const {
  return const_cast<const Bodies<T> *>(this->bodies);
}

template <typename T> void SimulationNBody<T>::setDtConstant(T dtVal) {
  this->dtConstant = true;
  this->dt = dtVal;
}

template <typename T> void SimulationNBody<T>::setDtVariable(T minDt) {
  this->dtConstant = false;
  this->dt = std::numeric_limits<T>::infinity();
  this->minDt = minDt;
}

template <typename T> const T &SimulationNBody<T>::getDt() const {
  return this->dt;
}

template <typename T> const float &SimulationNBody<T>::getFlopsPerIte() const {
  return this->flopsPerIte;
}

template <typename T>
const float &SimulationNBody<T>::getAllocatedBytes() const {
  return this->allocatedBytes;
}

template <typename T>
T SimulationNBody<T>::computeTimeStep(const unsigned long iBody) {
  const T *velocitiesX = this->bodies->getVelocitiesX();
  const T *velocitiesY = this->bodies->getVelocitiesY();
  const T *velocitiesZ = this->bodies->getVelocitiesZ();

  // || velocity[iBody] ||
  const T vi = std::sqrt((velocitiesX[iBody] * velocitiesX[iBody]) +
                         (velocitiesY[iBody] * velocitiesY[iBody]) +
                         (velocitiesZ[iBody] * velocitiesZ[iBody]));

  // || acceleration[iBody] ||
  const T ai =
      std::sqrt((this->accelerations.x[iBody] * this->accelerations.x[iBody]) +
                (this->accelerations.y[iBody] * this->accelerations.y[iBody]) +
                (this->accelerations.z[iBody] * this->accelerations.z[iBody]));

  /*
   * compute dt
   * solve:  (a/2)*dt^2 + v*dt + (-0.1)*ClosestNeighborDist = 0
   * <=>     dt = [ (-v) +/-  sqrt( v^2 - 4 * (a/2) * (-0.1)*ClosestNeighborDist
   * ) ] / [ 2 (a/2) ]
   *
   * dt should be positive (+/- becomes + because result of sqrt is positive)
   * <=>     dt = [ -v + sqrt( v^2 + 0.2*ClosestNeighborDist*a) ] / a
   */
  T dt =
      (std::sqrt(vi * vi + 0.2 * ai * this->closestNeighborDist[iBody]) - vi) /
      ai;

  if (dt == 0)
    dt = std::numeric_limits<T>::epsilon() / ai;

  return dt;
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class SimulationNBody<double>;
#else
template class SimulationNBody<float>;
#endif
// ====================================================================================
// explicit template instantiation
