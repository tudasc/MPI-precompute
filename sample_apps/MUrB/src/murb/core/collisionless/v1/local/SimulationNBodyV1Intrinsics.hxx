/*!
 * \file    SimulationNBodyV1Intrinsics.hxx
 * \brief   Implementation of SimulationNBodyLocal with intrinsic function calls (n² computations).
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#include <cmath>
#include <limits>
#include <string>
#include <cassert>
#include <fstream>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#else
#ifndef NO_OMP
#define NO_OMP
inline void omp_set_num_threads(int) {           }
inline int  omp_get_num_threads(   ) { return 1; }
inline int  omp_get_max_threads(   ) { return 1; }
inline int  omp_get_thread_num (   ) { return 0; }
#endif
#endif

#include "SimulationNBodyV1Intrinsics.h"

template <typename T>
SimulationNBodyV1Intrinsics<T>::SimulationNBodyV1Intrinsics(const unsigned long nBodies)
	: SimulationNBodyV1<T>(nBodies)
{
	this->init();
}

template <typename T>
SimulationNBodyV1Intrinsics<T>::SimulationNBodyV1Intrinsics(const std::string inputFileName)
	: SimulationNBodyV1<T>(inputFileName)
{
	this->init();
}

template <typename T>
void SimulationNBodyV1Intrinsics<T>::init()
{
	this->flopsPerIte = 19.f * ((float)this->bodies->getN() -1.f) * (float)this->bodies->getN();
}

template <>
void SimulationNBodyV1Intrinsics<float>::init()
{
	this->flopsPerIte = 20.f * ((float)this->bodies->getN() -1.f) * (float)this->bodies->getN();
}

template <typename T>
SimulationNBodyV1Intrinsics<T>::~SimulationNBodyV1Intrinsics()
{
}

template <typename T>
void SimulationNBodyV1Intrinsics<T>::initIteration()
{
	for(unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
	{
		this->accelerations.x[iBody] = 0.0;
		this->accelerations.y[iBody] = 0.0;
		this->accelerations.z[iBody] = 0.0;

		this->closestNeighborDist[iBody] = std::numeric_limits<T>::infinity();
	}
}

template <>
void SimulationNBodyV1Intrinsics<float>::initIteration()
{
	for(unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
	{
		this->accelerations.x[iBody] = 0.0;
		this->accelerations.y[iBody] = 0.0;
		this->accelerations.z[iBody] = 0.0;

		this->closestNeighborDist[iBody] = 0.0;
	}
}

template <typename T>
void SimulationNBodyV1Intrinsics<T>::_computeLocalBodiesAcceleration()
{
	// TODO: be careful with the V1Intrinsics version: with fake bodies added at the end of the last vector, the
	//       dynamic time step is broken.
	//       It is necessary to launch the simulation with a number of bodies multiple of mipp::vectorSize<T>()!
	assert(this->dtConstant || (this->bodies->getN() % mipp::N<T>() == 0));

	const auto mass  = (mipp::Reg<T>*)this->getBodies()->getMasses();
	const auto posX  = (mipp::Reg<T>*)this->getBodies()->getPositionsX();
	const auto posY  = (mipp::Reg<T>*)this->getBodies()->getPositionsY();
	const auto posZ  = (mipp::Reg<T>*)this->getBodies()->getPositionsZ();
	      auto accX  = (mipp::Reg<T>*)this->accelerations.x;
	      auto accY  = (mipp::Reg<T>*)this->accelerations.y;
	      auto accZ  = (mipp::Reg<T>*)this->accelerations.z;
	      auto closN = (mipp::Reg<T>*)this->closestNeighborDist;

	const mipp::Reg<T> rG = (T)this->G;

#pragma omp parallel for schedule(runtime) firstprivate(rG)
	for(unsigned long iVec = 0; iVec < this->bodies->getNVecs(); iVec++)
	{
		// load vectors
		const auto rqiX = posX[iVec]; // load
		const auto rqiY = posY[iVec]; // load
		const auto rqiZ = posZ[iVec]; // load

		auto raiX = accX[iVec]; // load
		auto raiY = accY[iVec]; // load
		auto raiZ = accZ[iVec]; // load

		mipp::Reg<T> rclosNeighi = (T)0;
		if(!this->dtConstant)
			rclosNeighi = closN[iVec]; // load

		for(unsigned long jVec = 0; jVec < this->bodies->getNVecs(); jVec++)
		{
			// load vectors
			auto rmj  = mass[jVec]; // load
			auto rqjX = posX[jVec]; // load
			auto rqjY = posY[jVec]; // load
			auto rqjZ = posZ[jVec]; // load

			if(iVec != jVec)
			{
				for(unsigned short iRot = 0; iRot < mipp::N<T>(); iRot++)
				{
					SimulationNBodyV1Intrinsics<T>::computeAccelerationBetweenTwoBodies(rG,
					                                                                    rqiX, rqiY, rqiZ,
					                                                                    raiX, raiY, raiZ,
					                                                                    rclosNeighi,
					                                                                    rmj,
					                                                                    rqjX, rqjY, rqjZ);

					// we make one useless rotate in the last iteration...
					rmj  = mipp::rrot(rmj);
					rqjX = mipp::rrot(rqjX);
					rqjY = mipp::rrot(rqjY);
					rqjZ = mipp::rrot(rqjZ);
				}
			}
			else
			{
				for(unsigned short iRot = 1; iRot < mipp::N<T>(); iRot++)
				{
					rmj  = mipp::rrot(rmj);
					rqjX = mipp::rrot(rqjX);
					rqjY = mipp::rrot(rqjY);
					rqjZ = mipp::rrot(rqjZ);

					SimulationNBodyV1Intrinsics<T>::computeAccelerationBetweenTwoBodies(rG,
					                                                                    rqiX, rqiY, rqiZ,
					                                                                    raiX, raiY, raiZ,
					                                                                    rclosNeighi,
					                                                                    rmj,
					                                                                    rqjX, rqjY, rqjZ);
				}
			}
		}

		// store vectors
		raiX.store(this->accelerations.x + iVec * mipp::N<T>());
		raiY.store(this->accelerations.y + iVec * mipp::N<T>());
		raiZ.store(this->accelerations.z + iVec * mipp::N<T>());

		if(!this->dtConstant)
			rclosNeighi.store(this->closestNeighborDist + iVec * mipp::N<T>());
	}
}

template <typename T>
void SimulationNBodyV1Intrinsics<T>::computeLocalBodiesAcceleration()
{
	this->_computeLocalBodiesAcceleration();
}

template <>
void SimulationNBodyV1Intrinsics<float>::computeLocalBodiesAcceleration()
{
	this->_computeLocalBodiesAcceleration();

	for(unsigned long iBody = 0; iBody < this->bodies->getN(); iBody++)
		this->closestNeighborDist[iBody] = 1.0 / this->closestNeighborDist[iBody];
}

// 19 flops
template <typename T>
void SimulationNBodyV1Intrinsics<T>::computeAccelerationBetweenTwoBodies(const mipp::Reg<T> &rG,
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
                                                                         const mipp::Reg<T> &rqjZ)
{
	auto rrijX = rqjX - rqiX; // 1 flop
	auto rrijY = rqjY - rqiY; // 1 flop
	auto rrijZ = rqjZ - rqiZ; // 1 flop

	auto rrijSquared = mipp::Reg<T>((T)0);
	rrijSquared = mipp::fmadd(rrijX, rrijX, rrijSquared); // 2 flops
	rrijSquared = mipp::fmadd(rrijY, rrijY, rrijSquared); // 2 flops
	rrijSquared = mipp::fmadd(rrijZ, rrijZ, rrijSquared); // 2 flops

	auto rrij = mipp::sqrt(rrijSquared); // 1 flop

	auto rai = (rG * rmj) / (rrij * rrijSquared); // 3 flops

	raiX = mipp::fmadd(rai, rrijX, raiX); // 2 flops
	raiY = mipp::fmadd(rai, rrijY, raiY); // 2 flops
	raiZ = mipp::fmadd(rai, rrijZ, raiZ); // 2 flops

	rclosNeighi = mipp::min(rclosNeighi, rrij);
}

// 20 flops
template <>
void SimulationNBodyV1Intrinsics<float>::computeAccelerationBetweenTwoBodies(const mipp::Reg<float> &rG,
                                                                             const mipp::Reg<float> &rqiX,
                                                                             const mipp::Reg<float> &rqiY,
                                                                             const mipp::Reg<float> &rqiZ,
                                                                                   mipp::Reg<float> &raiX,
                                                                                   mipp::Reg<float> &raiY,
                                                                                   mipp::Reg<float> &raiZ,
                                                                                   mipp::Reg<float> &rclosNeighi,
                                                                             const mipp::Reg<float> &rmj,
                                                                             const mipp::Reg<float> &rqjX,
                                                                             const mipp::Reg<float> &rqjY,
                                                                             const mipp::Reg<float> &rqjZ)
{
	auto rrijX = rqjX - rqiX; // 1 flop
	auto rrijY = rqjY - rqiY; // 1 flop
	auto rrijZ = rqjZ - rqiZ; // 1 flop

	auto rrijSquared = mipp::Reg<float>(0.f);
	rrijSquared = mipp::fmadd(rrijX, rrijX, rrijSquared); // 2 flops
	rrijSquared = mipp::fmadd(rrijY, rrijY, rrijSquared); // 2 flops
	rrijSquared = mipp::fmadd(rrijZ, rrijZ, rrijSquared); // 2 flops

	auto rrijInv = mipp::rsqrt(rrijSquared); // 1 flop

	// || ai || = G * mj / (rij   * rij   * rij  ) <=>
	// || ai || = G * mj * (1/rij * 1/rij * 1/rij)
	auto rai = rG * rmj * rrijInv * rrijInv * rrijInv; // 4 flops

	raiX = mipp::fmadd(rai, rrijX, raiX); // 2 flops
	raiY = mipp::fmadd(rai, rrijY, raiY); // 2 flops
	raiZ = mipp::fmadd(rai, rrijZ, raiZ); // 2 flops

	rclosNeighi = mipp::max(rclosNeighi, rrijInv);
}
