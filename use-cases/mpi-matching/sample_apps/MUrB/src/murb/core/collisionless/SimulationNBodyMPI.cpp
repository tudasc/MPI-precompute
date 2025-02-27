/*!
 * \file    SimulationNBodyMPI.hxx
 * \brief   Abstract n-body collision less simulation class for MPI computations
 * (between many nodes). \author  A. Cassagne \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifdef USE_MPI
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#else
#ifndef NO_OMP
#define NO_OMP
inline void omp_set_num_threads(int) {}
inline int omp_get_num_threads() { return 1; }
inline int omp_get_max_threads() { return 1; }
inline int omp_get_thread_num() { return 0; }
#endif
#endif

#include "../../../common/utils/ToMPIDatatype.h"

#include "SimulationNBodyMPI.h"

template <typename T>
SimulationNBodyMPI<T>::SimulationNBodyMPI(const unsigned long nBodies)
    : SimulationNBody<T>(new Bodies<T>(nBodies, MPI_get_rank() * nBodies + 1)),
      neighborBodies(NULL), MPISize(MPI_get_size()), MPIRank(MPI_get_rank()),
      MPIBodiesBuffers{this->bodies->getN(), this->bodies->getN()} {
  this->init();
}

template <typename T>
SimulationNBodyMPI<T>::SimulationNBodyMPI(const std::string inputFileName)
    : SimulationNBody<T>(new Bodies<T>(inputFileName)), neighborBodies(NULL),
      MPISize(MPI_get_size()), MPIRank(MPI_get_rank()),
      MPIBodiesBuffers{this->bodies->getN(), this->bodies->getN()} {
  this->init();
}

template <typename T> void SimulationNBodyMPI<T>::init() {
  const unsigned long n = this->bodies->getN() + this->bodies->getPadding();
  MPI_Datatype MPIType = ToMPIDatatype<T>::value();

  // who is next or previous MPI process ?
  int MPIPrevRank =
      (this->MPIRank == 0) ? this->MPISize - 1 : this->MPIRank - 1;
  int MPINextRank = (this->MPIRank + 1) % this->MPISize;

  Bodies<T> *bBuffs[] = {&this->MPIBodiesBuffers[0],
                         &this->MPIBodiesBuffers[1]};

  // send number of bodies
  MPI_Send_init(&bBuffs[0]->n, 1, MPI_LONG, MPINextRank, 0, MPI_COMM_WORLD,
                &this->MPIRequests[0][0]);
  MPI_Send_init(&bBuffs[1]->n, 1, MPI_LONG, MPINextRank, 1, MPI_COMM_WORLD,
                &this->MPIRequests[1][0]);
  // receive number of bodies
  MPI_Recv_init(&bBuffs[1]->n, 1, MPI_LONG, MPIPrevRank, 0, MPI_COMM_WORLD,
                &this->MPIRequests[0][1]);
  MPI_Recv_init(&bBuffs[0]->n, 1, MPI_LONG, MPIPrevRank, 1, MPI_COMM_WORLD,
                &this->MPIRequests[1][1]);

  // send masses
  MPI_Send_init(bBuffs[0]->masses, n, MPIType, MPINextRank, 2, MPI_COMM_WORLD,
                &this->MPIRequests[0][2]);
  MPI_Send_init(bBuffs[1]->masses, n, MPIType, MPINextRank, 3, MPI_COMM_WORLD,
                &this->MPIRequests[1][2]);
  // receive masses
  MPI_Recv_init(bBuffs[1]->masses, n, MPIType, MPIPrevRank, 2, MPI_COMM_WORLD,
                &this->MPIRequests[0][3]);
  MPI_Recv_init(bBuffs[0]->masses, n, MPIType, MPIPrevRank, 3, MPI_COMM_WORLD,
                &this->MPIRequests[1][3]);

  // send radiuses
  MPI_Send_init(bBuffs[0]->radiuses, n, MPIType, MPINextRank, 4, MPI_COMM_WORLD,
                &this->MPIRequests[0][4]);
  MPI_Send_init(bBuffs[1]->radiuses, n, MPIType, MPINextRank, 5, MPI_COMM_WORLD,
                &this->MPIRequests[1][4]);
  // receive radiuses
  MPI_Recv_init(bBuffs[1]->radiuses, n, MPIType, MPIPrevRank, 4, MPI_COMM_WORLD,
                &this->MPIRequests[0][5]);
  MPI_Recv_init(bBuffs[0]->radiuses, n, MPIType, MPIPrevRank, 5, MPI_COMM_WORLD,
                &this->MPIRequests[1][5]);

  // send positions.x
  MPI_Send_init(bBuffs[0]->positions.x, n, MPIType, MPINextRank, 6,
                MPI_COMM_WORLD, &this->MPIRequests[0][6]);
  MPI_Send_init(bBuffs[1]->positions.x, n, MPIType, MPINextRank, 7,
                MPI_COMM_WORLD, &this->MPIRequests[1][6]);
  // receive positions.x
  MPI_Recv_init(bBuffs[1]->positions.x, n, MPIType, MPIPrevRank, 6,
                MPI_COMM_WORLD, &this->MPIRequests[0][7]);
  MPI_Recv_init(bBuffs[0]->positions.x, n, MPIType, MPIPrevRank, 7,
                MPI_COMM_WORLD, &this->MPIRequests[1][7]);

  // send positions.y
  MPI_Send_init(bBuffs[0]->positions.y, n, MPIType, MPINextRank, 8,
                MPI_COMM_WORLD, &this->MPIRequests[0][8]);
  MPI_Send_init(bBuffs[1]->positions.y, n, MPIType, MPINextRank, 9,
                MPI_COMM_WORLD, &this->MPIRequests[1][8]);
  // receive positions.y
  MPI_Recv_init(bBuffs[1]->positions.y, n, MPIType, MPIPrevRank, 8,
                MPI_COMM_WORLD, &this->MPIRequests[0][9]);
  MPI_Recv_init(bBuffs[0]->positions.y, n, MPIType, MPIPrevRank, 9,
                MPI_COMM_WORLD, &this->MPIRequests[1][9]);

  // send positions.z
  MPI_Send_init(bBuffs[0]->positions.z, n, MPIType, MPINextRank, 10,
                MPI_COMM_WORLD, &this->MPIRequests[0][10]);
  MPI_Send_init(bBuffs[1]->positions.z, n, MPIType, MPINextRank, 11,
                MPI_COMM_WORLD, &this->MPIRequests[1][10]);
  // receive positions.z
  MPI_Recv_init(bBuffs[1]->positions.z, n, MPIType, MPIPrevRank, 10,
                MPI_COMM_WORLD, &this->MPIRequests[0][11]);
  MPI_Recv_init(bBuffs[0]->positions.z, n, MPIType, MPIPrevRank, 11,
                MPI_COMM_WORLD, &this->MPIRequests[1][11]);

  // send velocities.x
  MPI_Send_init(bBuffs[0]->velocities.x, n, MPIType, MPINextRank, 12,
                MPI_COMM_WORLD, &this->MPIRequests[0][12]);
  MPI_Send_init(bBuffs[1]->velocities.x, n, MPIType, MPINextRank, 13,
                MPI_COMM_WORLD, &this->MPIRequests[1][12]);
  // receive velocities.x
  MPI_Recv_init(bBuffs[1]->velocities.x, n, MPIType, MPIPrevRank, 12,
                MPI_COMM_WORLD, &this->MPIRequests[0][13]);
  MPI_Recv_init(bBuffs[0]->velocities.x, n, MPIType, MPIPrevRank, 13,
                MPI_COMM_WORLD, &this->MPIRequests[1][13]);

  // send velocities.y
  MPI_Send_init(bBuffs[0]->velocities.y, n, MPIType, MPINextRank, 14,
                MPI_COMM_WORLD, &this->MPIRequests[0][14]);
  MPI_Send_init(bBuffs[1]->velocities.y, n, MPIType, MPINextRank, 15,
                MPI_COMM_WORLD, &this->MPIRequests[1][14]);
  // receive velocities.y
  MPI_Recv_init(bBuffs[1]->velocities.y, n, MPIType, MPIPrevRank, 14,
                MPI_COMM_WORLD, &this->MPIRequests[0][15]);
  MPI_Recv_init(bBuffs[0]->velocities.y, n, MPIType, MPIPrevRank, 15,
                MPI_COMM_WORLD, &this->MPIRequests[1][15]);

  // send velocities.z
  MPI_Send_init(bBuffs[0]->velocities.z, n, MPIType, MPINextRank, 16,
                MPI_COMM_WORLD, &this->MPIRequests[0][16]);
  MPI_Send_init(bBuffs[1]->velocities.z, n, MPIType, MPINextRank, 17,
                MPI_COMM_WORLD, &this->MPIRequests[1][16]);
  // receive velocities.z
  MPI_Recv_init(bBuffs[1]->velocities.z, n, MPIType, MPIPrevRank, 16,
                MPI_COMM_WORLD, &this->MPIRequests[0][17]);
  MPI_Recv_init(bBuffs[0]->velocities.z, n, MPIType, MPIPrevRank, 17,
                MPI_COMM_WORLD, &this->MPIRequests[1][17]);
}

template <typename T> SimulationNBodyMPI<T>::~SimulationNBodyMPI() {
  for (int i = 0; i < 18; ++i) {
    MPI_Request_free(&this->MPIRequests[0][i]);
    MPI_Request_free(&this->MPIRequests[1][i]);
  }
  delete this->bodies;
}

template <typename T> void SimulationNBodyMPI<T>::computeOneIteration() {
  this->MPIBodiesBuffers[0].hardCopy(*this->bodies);

  this->initIteration();

  // compute bodies acceleration
  // ------------------------------------------------------------------------------------
  MPI_Startall(2 * 9, this->MPIRequests[0]);

  this->computeLocalBodiesAcceleration();

  for (int iStep = 1; iStep < this->MPISize; ++iStep) {
    MPI_Waitall(2 * 9, this->MPIRequests[(iStep - 1) % 2], MPI_STATUSES_IGNORE);

    this->neighborBodies = &this->MPIBodiesBuffers[iStep % 2];

    this->computeNeighborBodiesAcceleration();

    if (iStep < this->MPISize - 1)
      MPI_Startall(2 * 9, this->MPIRequests[iStep % 2]);
  }
  // ----------------------------------------------------------------------------------------------------------------

  // find time step
  // -------------------------------------------------------------------------------------------------
  if (!this->dtConstant)
    this->findTimeStep();
  // ----------------------------------------------------------------------------------------------------------------

  // update positions and velocities
  // --------------------------------------------------------------------------------
  this->bodies->updatePositionsAndVelocities(this->accelerations, this->dt);
  // ----------------------------------------------------------------------------------------------------------------
}

template <typename T> void SimulationNBodyMPI<T>::findTimeStep() {
  // TODO: be careful with the V1Intrinsics version: with fake bodies added at
  // the end of the last vector, the
  //       dynamic time step is broken.
  //       It is necessary to launch the simulation with a number of bodies
  //       multiple of mipp::vectorSize<T>()!
  if (!this->dtConstant) {
    T localDt = std::numeric_limits<T>::infinity();
    for (unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
      localDt = std::min(localDt, this->computeTimeStep(iBody));

    MPI_Allreduce(&localDt, &this->dt, 1, ToMPIDatatype<T>::value(), MPI_MIN,
                  MPI_COMM_WORLD);

    if (this->dt < this->minDt)
      this->dt = this->minDt;
  }
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class SimulationNBodyMPI<double>;
#else
template class SimulationNBodyMPI<float>;
#endif
// ====================================================================================
// explicit template instantiation
#endif
