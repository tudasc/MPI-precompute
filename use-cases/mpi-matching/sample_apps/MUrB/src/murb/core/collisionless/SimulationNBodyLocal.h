/*!
 * \file    SimulationNBodyLocal.h
 * \brief   Abstract n-body collision less simulation class for local
 * computations (inside a node). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_LOCAL_H_
#define SIMULATION_N_BODY_LOCAL_H_

#include <string>

#include "../SimulationNBody.h"

/*!
 * \class  SimulationNBodyLocal
 * \brief  Abstract n-body simulation class for local computations (inside a
 * node).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyLocal : public SimulationNBody<T> {
protected:
  SimulationNBodyLocal(const unsigned long nBodies,
                       const unsigned long randInit = 1);
  SimulationNBodyLocal(const std::string inputFileName);

public:
  virtual ~SimulationNBodyLocal();

  void computeOneIteration();

protected:
  virtual void initIteration() = 0;
  virtual void computeLocalBodiesAcceleration() = 0;

private:
  void findTimeStep();
};

#endif /* SIMULATION_N_BODY_LOCAL_H_ */
