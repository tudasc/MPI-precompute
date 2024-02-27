/*!
 * \file    SimulationNBodyV1CB.h
 * \brief   Naive implementation of SimulationNBodyLocal with the Cache Blocking technique (n² computations).
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V1_CB_H_
#define SIMULATION_N_BODY_V1_CB_H_

#include <string>

#include "SimulationNBodyV1.h"

/*!
 * \class  SimulationNBodyV1CB
 * \brief  Naive implementation of SimulationNBodyLocal with the Cache Blocking technique (n² computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV1CB : public SimulationNBodyV1<T>
{
public:
	SimulationNBodyV1CB(const unsigned long nBodies);
	SimulationNBodyV1CB(const std::string inputFileName);
	virtual ~SimulationNBodyV1CB();

protected:
	virtual void computeLocalBodiesAcceleration();
};

#include "SimulationNBodyV1CB.hxx"

#endif /* SIMULATION_N_BODY_V1_CB_H_ */
