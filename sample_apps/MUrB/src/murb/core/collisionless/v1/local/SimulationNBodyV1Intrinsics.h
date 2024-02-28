/*!
 * \file    SimulationNBodyV1Intrinsics.h
 * \brief   Implementation of SimulationNBodyLocal with intrinsic function calls
 * (n² computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V1_INTRINSICS_H_
#define SIMULATION_N_BODY_V1_INTRINSICS_H_

#include <mipp.h>
#include <string>

#include "SimulationNBodyV1.h"

/*!
 * \class  SimulationNBodyV1Intrinsics
 * \brief  Implementation of SimulationNBodyLocal with intrinsic function calls
 * (n² computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV1Intrinsics : public SimulationNBodyV1<T> {
public:
  SimulationNBodyV1Intrinsics(const unsigned long nBodies);
  SimulationNBodyV1Intrinsics(const std::string inputFileName);
  virtual ~SimulationNBodyV1Intrinsics();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  static inline void computeAccelerationBetweenTwoBodies(
      const mipp::Reg<T> &rG, const mipp::Reg<T> &rqiX,
      const mipp::Reg<T> &rqiY, const mipp::Reg<T> &rqiZ, mipp::Reg<T> &raiX,
      mipp::Reg<T> &raiY, mipp::Reg<T> &raiZ, mipp::Reg<T> &rclosNeighi,
      const mipp::Reg<T> &rmj, const mipp::Reg<T> &rqjX,
      const mipp::Reg<T> &rqjY, const mipp::Reg<T> &rqjZ);

private:
  void init();
  void _computeLocalBodiesAcceleration();
};

#include "SimulationNBodyV1Intrinsics.hxx"

#endif /* SIMULATION_N_BODY_V1_INTRINSICS_H_ */
