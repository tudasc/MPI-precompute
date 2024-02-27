/*!
 * \file    BodiesCollision.h
 * \brief   Bodies container with collisions management.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef BODIES_COLLISION_H_
#define BODIES_COLLISION_H_

#include <string>
#include <vector>

#include "../../../common/core/Bodies.h"

/*!
 * \class  BodiesCollision
 * \brief  BodiesCollision class represents the physic data of each body (mass,
 * radius, position and velocity) and manages collisions.
 *
 * \tparam T : Type.
 */
template <typename T = double> class BodiesCollision : public Bodies<T> {
public:
  BodiesCollision();
  BodiesCollision(const unsigned long n, const unsigned long randInit = 0);
  BodiesCollision(const std::string inputFileName);
  BodiesCollision(const BodiesCollision<T> &bodies);

  virtual ~BodiesCollision();

  void applyCollisions(std::vector<std::vector<unsigned long>> collisions);
};

#endif /* BODIES_COLLISION_H_ */
