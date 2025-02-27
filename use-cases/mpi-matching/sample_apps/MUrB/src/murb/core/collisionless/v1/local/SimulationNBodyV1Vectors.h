/*!
 * \file    SimulationNBodyV1Vectors.h
 * \brief   Implementation of SimulationNBodyLocal with vector size stride loops
 * (n² computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V1_VECTORS_H_
#define SIMULATION_N_BODY_V1_VECTORS_H_

#include <string>

#include "SimulationNBodyV1.h"

/*!
 * \class  SimulationNBodyV1Vectors
 * \brief  Implementation of SimulationNBodyLocal with vector size stride loops
 * (n² computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV1Vectors : public SimulationNBodyV1<T> {
public:
  SimulationNBodyV1Vectors(const unsigned long nBodies);
  SimulationNBodyV1Vectors(const std::string inputFileName);
  virtual ~SimulationNBodyV1Vectors();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  inline void computeAccelerationBetweenTwoBodies(
      const T &iPosX, const T &iPosY, const T &iPosZ, T &iAccsX, T &iAccsY,
      T &iAccsZ, T &iClosNeiDist, const T &jMasses, const T &jPosX,
      const T &jPosY, const T &jPosZ);

private:
  void init();
};

#include "SimulationNBodyV1Vectors.hxx"

#endif /* SIMULATION_N_BODY_V1_VECTORS_H_ */
