/*!
 * \file    SimulationNBody.h
 * \brief   Higher abstraction for all n-body simulation classes.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef SIMULATION_N_BODY_H_
#define SIMULATION_N_BODY_H_

#include <string>

#include "../../common/core/Bodies.h"

/*!
 * \class  SimulationNBody
 * \brief  This is the main simulation class, it describes the main methods to
 * implement in extended classes.
 *
 * \tparam T : Type.
 */
template <typename T = double> class SimulationNBody {
protected:
  const T G = 6.67384e-11; /*!< The gravitational constant in m^3.kg^-1.s^-2. */
  Bodies<T> *bodies; /*!< Bodies object, represent all the bodies available in
                        space. */
  vector3<T> accelerations; /*!< 3D array of bodies acceleration calculated in
                               simulation classes. */
  T *closestNeighborDist;   /*!< Vector of distance between the closest neighbor
                               body (for each body). */
  bool dtConstant;          /*!< Are we using constant time step? */
  T dt;                     /*!< Time step value. */
  T minDt; /*!< Minimum time step value (useful with variable time step). */
  // stats
  float flopsPerIte; /*!< Number of floating-point operations per iteration. */
  float allocatedBytes; /*!< Number of allocated bytes. */
  unsigned nMaxThreads; /*!< Maximum number of OMP threads available. */

protected:
  /*!
   *  \brief Constructor.
   *
   *  SimulationNBody constructor.
   *
   *  \param bodies : Bodies class.
   */
  SimulationNBody(Bodies<T> *bodies);

public:
  /*!
   *  \brief Destructor.
   *
   *  SimulationNBody destructor.
   */
  virtual ~SimulationNBody();

  /*!
   *  \brief Bodies getter.
   *
   *  \return Bodies class.
   */
  const Bodies<T> *getBodies() const;

  /*!
   *  \brief DtConstant setter.
   *
   *  \param dtVal : Constant time step value.
   */
  void setDtConstant(T dtVal);

  /*!
   *  \brief DtVariable setter.
   *
   *  \param minDt : Minimum time step value.
   */
  void setDtVariable(T minDt);

  /*!
   *  \brief Time step getter.
   *
   *  \return Time step value.
   */
  const T &getDt() const;

  /*!
   *  \brief Flops per iteration getter.
   *
   *  \return Flops per iteration.
   */
  const float &getFlopsPerIte() const;

  /*!
   *  \brief Allocated bytes getter.
   *
   *  \return Number of allocated bytes.
   */
  const float &getAllocatedBytes() const;

  /*!
   *  \brief Main compute method.
   *
   *  Compute one iteration of the simulation.
   */
  virtual void computeOneIteration() = 0;

protected:
  /*!
   *  \brief Initialized data in order to compute an iteration.
   */
  virtual void initIteration() = 0;

  /*!
   *  \brief Look for the new time step if dtConstant = false, else do nothing.
   */
  virtual void findTimeStep() = 0;

  /*!
   *  \brief Compute the time step for one given body (considering the closest
   * neighbor body).
   *
   *  \param iBody : id of the body.
   *
   *  \return Computed time step for iBody.
   */
  T computeTimeStep(const unsigned long iBody);

private:
  /*!
   *  \brief Allocation of required buffers.
   */
  void allocateBuffers();
};

#endif /* SIMULATION_N_BODY_H_ */
