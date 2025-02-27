/*!
 * \file    SimulationNBodyCollisionLocal.h
 * \brief   Abstract n-body simulation class with collisions for local
 * computations (inside a node). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_COLLISION_LOCAL_H_
#define SIMULATION_N_BODY_COLLISION_LOCAL_H_

#include <string>
#include <vector>

#include "../SimulationNBody.h"

/*!
 * \class  SimulationNBodyCollisionLocal
 * \brief  Abstract n-body simulation class with collisions for local
 * computations (inside a node).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyCollisionLocal : public SimulationNBody<T> {
protected:
  std::vector<std::vector<unsigned long>> collisions;

  SimulationNBodyCollisionLocal(const unsigned long nBodies,
                                const unsigned long randInit = 1);
  SimulationNBodyCollisionLocal(const std::string inputFileName);

public:
  virtual ~SimulationNBodyCollisionLocal();

  void computeOneIteration();

protected:
  virtual void initIteration() = 0;
  virtual void computeLocalBodiesAcceleration() = 0;

private:
  void findTimeStep();
};

#endif /* SIMULATION_N_BODY_COLLISION_LOCAL_H_ */
