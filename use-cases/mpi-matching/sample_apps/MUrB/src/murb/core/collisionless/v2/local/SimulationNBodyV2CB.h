/*!
 * \file    SimulationNBodyV2CB.h
 * \brief   Implementation of SimulationNBodyLocal with the Cache Blocking Technique (n²/2 computations).
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_V2_CB_H_
#define SIMULATION_N_BODY_V2_CB_H_

#include <string>

#include "SimulationNBodyV2.h"

/*!
 * \class  SimulationNBodyV2CB
 * \brief  Implementation of SimulationNBodyLocal with the Cache Blocking Technique (n²/2 computations).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyV2CB : public SimulationNBodyV2<T>
{
public:
	SimulationNBodyV2CB(const unsigned long nBodies);
	SimulationNBodyV2CB(const std::string inputFileName);
	virtual ~SimulationNBodyV2CB();

protected:
	virtual void computeLocalBodiesAcceleration();
};

#include "SimulationNBodyV2CB.hxx"

#endif /* SIMULATION_N_BODY_V2_CB_H_ */
