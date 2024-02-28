/*!
 * \file    SimulationNBodyMPI.h
 * \brief   Abstract n-body collision less simulation class for MPI computations
 * (between many nodes). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifdef USE_MPI
#ifndef SIMULATION_N_BODY_MPI_H_
#define SIMULATION_N_BODY_MPI_H_

#include <mpi.h>
#include <string>

#include "../../../common/core/Bodies.h"

#include "../SimulationNBody.h"

/*!
 * \class  SimulationNBodyMPI
 * \brief  Abstract n-body simulation class for MPI computations (between many
 * nodes).
 *
 * \tparam T : Type.
 */
template <typename T = double>
class SimulationNBodyMPI : public SimulationNBody<T> {
protected:
  Bodies<T> *neighborBodies;
  const int MPISize;
  const int MPIRank;

private:
  Bodies<T> MPIBodiesBuffers[2];
  MPI_Request MPIRequests[2][2 * 9];

protected:
  SimulationNBodyMPI(const unsigned long nBodies);
  SimulationNBodyMPI(const std::string inputFileName);

public:
  virtual ~SimulationNBodyMPI();

  void computeOneIteration();

protected:
  virtual void initIteration() = 0;
  virtual void computeLocalBodiesAcceleration() = 0;
  virtual void computeNeighborBodiesAcceleration() = 0;

private:
  void init();
  void findTimeStep();
};

#endif /* SIMULATION_N_BODY_MPI_H_ */
#endif
