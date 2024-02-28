/*!
 * \file    SimulationNBodyV2Vectors.h
 * \brief   Implementation of SimulationNBodyLocal with vector size stride loops
 * (n²/2 computations). \author  G. Hautreux \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V2_VECTORS_H_
#define SIMULATION_N_BODY_V2_VECTORS_H_

#include <string>

#include "SimulationNBodyV2.h"

/*!
 * \class  SimulationNBodyV2Vectors
 * \brief  Implementation of SimulationNBodyLocal with vector size stride loops
 * (n²/2 computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV2Vectors : public SimulationNBodyV2<T> {
public:
  SimulationNBodyV2Vectors(const unsigned long nBodies);
  SimulationNBodyV2Vectors(const std::string inputFileName);
  virtual ~SimulationNBodyV2Vectors();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

private:
  void reAllocateBuffers();
};

#include "SimulationNBodyV2Vectors.hxx"

#endif /* SIMULATION_N_BODY_V2_VECTORS_H_ */
