/*!
 * \file    SimulationNBodyV3Intrinsics.h
 * \brief   Implementation of SimulationNBodyLocal with intrinsic function calls (n² computations).
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V3_INTRINSICS_H_
#define SIMULATION_N_BODY_V3_INTRINSICS_H_

#include <string>
#include <mipp.h>

#include "SimulationNBodyV3.h"

/*!
 * \class  SimulationNBodyV3Intrinsics
 * \brief  Implementation of SimulationNBodyLocal with intrinsic function calls (n² computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV3Intrinsics : public SimulationNBodyV3<T>
{
public:
	SimulationNBodyV3Intrinsics(const unsigned long nBodies, T softening = 0.035);
	SimulationNBodyV3Intrinsics(const std::string inputFileName, T softening = 0.035);
	virtual ~SimulationNBodyV3Intrinsics();

protected:
	virtual void initIteration();
	virtual void computeLocalBodiesAcceleration();

	static inline void computeAccelerationBetweenTwoBodies(const mipp::Reg<T> &rG,
	                                                       const mipp::Reg<T> &rSoftSquared,
	                                                       const mipp::Reg<T> &rqiX,
	                                                       const mipp::Reg<T> &rqiY,
	                                                       const mipp::Reg<T> &rqiZ,
	                                                             mipp::Reg<T> &raiX,
	                                                             mipp::Reg<T> &raiY,
	                                                             mipp::Reg<T> &raiZ,
	                                                             mipp::Reg<T> &rclosNeighi,
	                                                       const mipp::Reg<T> &rmj,
	                                                       const mipp::Reg<T> &rqjX,
	                                                       const mipp::Reg<T> &rqjY,
	                                                       const mipp::Reg<T> &rqjZ);
private:
	void init();
	void _computeLocalBodiesAcceleration();
};

#include "SimulationNBodyV3Intrinsics.hxx"

#endif /* SIMULATION_N_BODY_V3_INTRINSICS_H_ */
