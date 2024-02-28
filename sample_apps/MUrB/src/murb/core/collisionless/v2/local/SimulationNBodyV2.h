/*!
 * \file    SimulationNBodyV2.h
 * \brief   Implementation of SimulationNBodyLocal (n²/2 computations).
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V2_H_
#define SIMULATION_N_BODY_V2_H_

#include <string>

#include "../../SimulationNBodyLocal.h"

/*!
 * \class  SimulationNBodyV2
 * \brief  Implementation of SimulationNBodyLocal (n²/2 computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV2 : public SimulationNBodyLocal<T> {
public:
  SimulationNBodyV2(const unsigned long nBodies);
  SimulationNBodyV2(const std::string inputFileName);
  virtual ~SimulationNBodyV2();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  static inline void computeAccelerationBetweenTwoBodies(
      const T &G, const T &mi, const T &qiX, const T &qiY, const T &qiZ, T &aiX,
      T &aiY, T &aiZ, T &closNeighi, const T &mj, const T &qjX, const T &qjY,
      const T &qjZ, T &ajX, T &ajY, T &ajZ, T &closNeighj);

private:
  void reAllocateBuffers();
};

#include "SimulationNBodyV2.hxx"

#endif /* SIMULATION_N_BODY_V2_H_ */
