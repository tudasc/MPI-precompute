/*!
 * \file    SimulationNBodyV2Intrinsics.h
 * \brief   Implementation of SimulationNBodyLocal with intrinsic function calls
 * (n²/2 computations). \author  G. Hautreux \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V2_INTRINSICS
#define SIMULATION_N_BODY_V2_INTRINSICS

#include <mipp.h>
#include <string>

#include "SimulationNBodyV2.h"

/*!
 * \class  SimulationNBodyV2Intrinsics
 * \brief  Implementation of SimulationNBodyLocal with intrinsic function calls
 * (n²/2 computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV2Intrinsics : public SimulationNBodyV2<T> {
public:
  SimulationNBodyV2Intrinsics(const unsigned long nBodies);
  SimulationNBodyV2Intrinsics(const std::string inputFileName);
  virtual ~SimulationNBodyV2Intrinsics();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();

  static inline void computeAccelerationBetweenTwoBodies(
      const mipp::Reg<T> &rG, const mipp::Reg<T> &rmi, const mipp::Reg<T> &rqiX,
      const mipp::Reg<T> &rqiY, const mipp::Reg<T> &rqiZ, mipp::Reg<T> &raiX,
      mipp::Reg<T> &raiY, mipp::Reg<T> &raiZ, mipp::Reg<T> &rclosNeighi,
      const mipp::Reg<T> &rmj, const mipp::Reg<T> &rqjX,
      const mipp::Reg<T> &rqjY, const mipp::Reg<T> &rqjZ, mipp::Reg<T> &rajX,
      mipp::Reg<T> &rajY, mipp::Reg<T> &rajZ, mipp::Reg<T> &rclosNeighj);

private:
  void _computeLocalBodiesAcceleration();
  void _reAllocateBuffers();
  void reAllocateBuffers();
  void _initIteration();
};

#include "SimulationNBodyV2Intrinsics.hxx"

#endif /* SIMULATION_N_BODY_V2_INTRINSICS */
