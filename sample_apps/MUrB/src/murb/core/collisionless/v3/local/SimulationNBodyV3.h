/*!
 * \file    SimulationNBodyV3.h
 * \brief   Implementation of SimulationNBodyLocal with the softening factor (n²
 * computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V3_H_
#define SIMULATION_N_BODY_V3_H_

#include <string>

#include "../../SimulationNBodyLocal.h"

/*!
 * \class  SimulationNBodyV3
 * \brief  Implementation of SimulationNBodyLocal with the softening factor (n²
 * computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV3 : public SimulationNBodyLocal<T> {
protected:
  T softeningSquared;

public:
  SimulationNBodyV3(const unsigned long nBodies, T softening = 0.035);
  SimulationNBodyV3(const std::string inputFileName, T softening = 0.035);
  virtual ~SimulationNBodyV3();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  static inline void computeAccelerationBetweenTwoBodies(
      const T &G, const T &softSquared, const T &qiX, const T &qiY,
      const T &qiZ, T &aiX, T &aiY, T &aiZ, T &closNeighi, const T &mj,
      const T &qjX, const T &qjY, const T &qjZ);

private:
  void init();
};

#include "SimulationNBodyV3.hxx"

#endif /* SIMULATION_N_BODY_V3_H_ */
