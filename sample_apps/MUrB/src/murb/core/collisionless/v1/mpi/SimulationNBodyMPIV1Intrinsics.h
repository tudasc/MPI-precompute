/*!
 * \file    SimulationNBodyMPIV1Intrinsics.h
 * \brief   Implementation of SimulationNBodyMPI with intrinsic function calls
 * (n² computations). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_MPI_V1_INTRINSICS_H_
#define SIMULATION_N_BODY_MPI_V1_INTRINSICS_H_

#include <mipp.h>
#include <string>

#include "../../SimulationNBodyMPI.h"

/*!
 * \class  SimulationNBodyMPIV1Intrinsics
 * \brief  Implementation of SimulationNBodyMPI with intrinsic function calls
 * (n² computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyMPIV1Intrinsics : public SimulationNBodyMPI<T> {
public:
  SimulationNBodyMPIV1Intrinsics(const unsigned long nBodies);
  SimulationNBodyMPIV1Intrinsics(const std::string inputFileName);
  virtual ~SimulationNBodyMPIV1Intrinsics();

protected:
  virtual void initIteration();
  virtual void computeLocalBodiesAcceleration();
  virtual void computeNeighborBodiesAcceleration();

  static inline void computeAccelerationBetweenTwoBodies(
      const mipp::Reg<T> &rG, const mipp::Reg<T> &rqiX,
      const mipp::Reg<T> &rqiY, const mipp::Reg<T> &rqiZ, mipp::Reg<T> &raiX,
      mipp::Reg<T> &raiY, mipp::Reg<T> &raiZ, mipp::Reg<T> &rclosNeighi,
      const mipp::Reg<T> &rmj, const mipp::Reg<T> &rqjX,
      const mipp::Reg<T> &rqjY, const mipp::Reg<T> &rqjZ);

private:
  void init();
  void _computeLocalBodiesAcceleration();
  void _computeNeighborBodiesAcceleration();
};

#include "SimulationNBodyMPIV1Intrinsics.hxx"

#endif /* SIMULATION_N_BODY_MPI_V1_INTRINSICS_H_ */
