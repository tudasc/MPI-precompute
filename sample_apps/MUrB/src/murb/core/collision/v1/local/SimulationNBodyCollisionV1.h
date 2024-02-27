/*!
 * \file    SimulationNBodyCollisionV1.h
 * \brief   Naive implementation of SimulationNBodyCollisionLocal (n²
 * computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_COLLISION_V1_H_
#define SIMULATION_N_BODY_COLLISION_V1_H_

#include <string>

#include "../../SimulationNBodyCollisionLocal.h"

/*!
 * \class  SimulationNBodyCollisionV1
 * \brief  Naive implementation of SimulationNBodyCollisionLocal (n²
 * computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyCollisionV1 : public SimulationNBodyCollisionLocal<T> {
public:
  SimulationNBodyCollisionV1(const unsigned long nBodies);
  SimulationNBodyCollisionV1(const std::string inputFileName);
  virtual ~SimulationNBodyCollisionV1();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  /* return the distance between body i and body j considering the radiuses of
   * the spheres */
  static inline T computeAccelerationBetweenTwoBodies(
      const T &G, const T &ri, const T &qiX, const T &qiY, const T &qiZ, T &aiX,
      T &aiY, T &aiZ, T &closNeighi, const T &mj, const T &rj, const T &qjX,
      const T &qjY, const T &qjZ);

private:
  void init();
};

#include "SimulationNBodyCollisionV1.hxx"

#endif /* SIMULATION_N_BODY_COLLISION_V1_H_ */
