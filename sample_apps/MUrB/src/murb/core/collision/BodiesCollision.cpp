/*!
 * \file    BodiesCollision.hxx
 * \brief   Bodies container with collisions management.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <sys/stat.h>

#include "BodiesCollision.h"

template <typename T> BodiesCollision<T>::BodiesCollision() : Bodies<T>() {}

template <typename T>
BodiesCollision<T>::BodiesCollision(const unsigned long n,
                                    const unsigned long randInit)
    : Bodies<T>(n, randInit) {}

template <typename T>
BodiesCollision<T>::BodiesCollision(const std::string inputFileName)
    : Bodies<T>(inputFileName) {}

template <typename T>
BodiesCollision<T>::BodiesCollision(const BodiesCollision<T> &bodies)
    : Bodies<T>(bodies) {}

template <typename T> BodiesCollision<T>::~BodiesCollision() {}

/* collisions 2D
template <typename T>
void BodiesCollision<T>::applyCollisions(std::vector<std::vector<unsigned long>>
collisions)
{
        for(unsigned long iBody = 0; iBody < this->n; iBody++)
                if(collisions[iBody].size())
                {
                        assert(collisions[iBody].size() < 2);
                        unsigned long jBody = collisions[iBody][0];

                        // 1. Find unit normal and unit tangent vectors
                        T normX = this->positions.x[jBody] -
this->positions.x[iBody]; T normY = this->positions.y[jBody] -
this->positions.y[iBody];

                        T rij = std::sqrt((normX * normX) + (normY * normY));

                        T uNormX = normX / rij;
                        T uNormY = normY / rij;

                        T uTangX = -uNormY;
                        T uTangY =  uNormX;

                        // 3 . Projecting the velocity vectors onto the unit
normal and unit tangent vectors T viNorm = uNormX * this->velocities.x[iBody] +
uNormY * this->velocities.y[iBody]; T viTang = uTangX *
this->velocities.x[iBody] + uTangY * this->velocities.y[iBody];

                        T vjNorm = uNormX * this->velocities.x[jBody] + uNormY *
this->velocities.y[jBody]; T vjTang = uTangX * this->velocities.x[jBody] +
uTangY * this->velocities.y[jBody];

                        // 4. Find the new tangential velocities
                        T viNewTang = viTang;
                        T vjNewTang = vjTang;

                        T mi = this->masses[iBody];
                        T mj = this->masses[jBody];

                        // 5. Find the new normal velocities
                        T viNewNorm = (viNorm * (mi - mj) + (2.0 * mj * vjNorm))
/ (mi + mj); T vjNewNorm = (vjNorm * (mj - mi) + (2.0 * mi * viNorm)) / (mi +
mj);

                        // 6. Convert the scalar normal and tangential
velocities into vectors T viNewNormX = viNewNorm * uNormX; T viNewNormY =
viNewNorm * uNormY;

                        T viNewTangX = viNewTang * uTangX;
                        T viNewTangY = viNewTang * uTangY;

                        T vjNewNormX = vjNewNorm * uNormX;
                        T vjNewNormY = vjNewNorm * uNormY;

                        T vjNewTangX = vjNewTang * uTangX;
                        T vjNewTangY = vjNewTang * uTangY;

                        // 7. Find the final velocity vectors by adding the
normal and tangential components this->velocities.x[iBody] = viNewNormX +
viNewTangX; this->velocities.y[iBody] = viNewNormY + viNewTangY;

                        this->velocities.x[jBody] = vjNewNormX + vjNewTangX;
                        this->velocities.y[jBody] = vjNewNormY + vjNewTangY;

                        collisions[jBody].clear();
                }
}
*/

/* collisions 3D */
template <typename T>
void BodiesCollision<T>::applyCollisions(
    std::vector<std::vector<unsigned long>> collisions) {
  for (unsigned long iBody = 0; iBody < this->n; iBody++) {
    for (unsigned long iCollision = 0; iCollision < collisions[iBody].size();
         iCollision++) {
      unsigned long jBody = collisions[iBody][iCollision];

      if (jBody > iBody) {
        // 1. Find unit normal and unit tangent 1 and tangent 2 vectors
        T normX = this->positions.x[jBody] - this->positions.x[iBody];
        T normY = this->positions.y[jBody] - this->positions.y[iBody];
        T normZ = this->positions.z[jBody] - this->positions.z[iBody];

        T dNorm =
            std::sqrt((normX * normX) + (normY * normY) + (normZ * normZ));

        T uNormX = normX / dNorm;
        T uNormY = normY / dNorm;
        T uNormZ = normZ / dNorm;

        // uTan1 and uTan2 vectors define the collision tangent plan (for 3D
        // collisions)
        T tan1X, tan1Y, tan1Z;
        if (uNormX != 0) {
          tan1X = -(uNormY / uNormX);
          tan1Y = 1.0;
          tan1Z = 0.0;
        } else if (uNormY != 0) {
          tan1X = 1.0;
          tan1Y = -(uNormX / uNormY);
          tan1Z = 0.0;
        } else if (uNormZ != 0) {
          tan1X = 1.0;
          tan1Y = 0.0;
          tan1Z = -(uNormX / uNormZ);
        } else {
          std::cout << "Error: uNorm = {" << uNormX << ", " << uNormY << ", "
                    << uNormZ << "}"
                    << "\n";
          exit(-1);
        }

        T dTan1 =
            std::sqrt((tan1X * tan1X) + (tan1Y * tan1Y) + (tan1Z * tan1Z));

        T uTan1X = tan1X / dTan1;
        T uTan1Y = tan1Y / dTan1;
        T uTan1Z = tan1Z / dTan1;

        // uTan2 vector = (uNormX vector) ^ (uTan1 vector); (cross product or
        // vector product)
        T uTan2X = (uNormY * uTan1Z) - (uNormZ * uTan1Y);
        T uTan2Y = (uNormZ * uTan1X) - (uNormX * uTan1Z);
        T uTan2Z = (uNormX * uTan1Y) - (uNormY * uTan1X);

        // 2. Create the initial (before the collision) velocity vectors, iVel
        // and jVel
        T viX = this->velocities.x[iBody];
        T viY = this->velocities.y[iBody];
        T viZ = this->velocities.z[iBody];

        T vjX = this->velocities.x[jBody];
        T vjY = this->velocities.y[jBody];
        T vjZ = this->velocities.z[jBody];

        // 3. Projecting the velocity vectors onto the unit normal and unit
        // tangent vectors (scalar product or dot product)
        T viNorm = (uNormX * viX) + (uNormY * viY) + (uNormZ * viZ);
        T viTan1 = (uTan1X * viX) + (uTan1Y * viY) + (uTan1Z * viZ);
        T viTan2 = (uTan2X * viX) + (uTan2Y * viY) + (uTan2Z * viZ);

        T vjNorm = (uNormX * vjX) + (uNormY * vjY) + (uNormZ * vjZ);
        T vjTan1 = (uTan1X * vjX) + (uTan1Y * vjY) + (uTan1Z * vjZ);
        T vjTan2 = (uTan2X * vjX) + (uTan2Y * vjY) + (uTan2Z * vjZ);

        // 4. Find the new tangential velocities
        T viNewTan1 = viTan1;
        T viNewTan2 = viTan2;

        T vjNewTan1 = vjTan1;
        T vjNewTan2 = vjTan2;

        T mi = this->masses[iBody];
        T mj = this->masses[jBody];

        // 5. Find the new normal velocities (apply elastic collision based on
        // momentum and kinetic energy conservation)
        T viNewNorm = (viNorm * (mi - mj) + (2.0 * mj * vjNorm)) / (mi + mj);
        T vjNewNorm = (vjNorm * (mj - mi) + (2.0 * mi * viNorm)) / (mi + mj);

        // 6. Convert the scalar normal and tangential velocities into vectors
        T viNewNormX = viNewNorm * uNormX;
        T viNewNormY = viNewNorm * uNormY;
        T viNewNormZ = viNewNorm * uNormZ;

        T viNewTan1X = viNewTan1 * uTan1X;
        T viNewTan1Y = viNewTan1 * uTan1Y;
        T viNewTan1Z = viNewTan1 * uTan1Z;

        T viNewTan2X = viNewTan2 * uTan2X;
        T viNewTan2Y = viNewTan2 * uTan2Y;
        T viNewTan2Z = viNewTan2 * uTan2Z;

        T vjNewNormX = vjNewNorm * uNormX;
        T vjNewNormY = vjNewNorm * uNormY;
        T vjNewNormZ = vjNewNorm * uNormZ;

        T vjNewTan1X = vjNewTan1 * uTan1X;
        T vjNewTan1Y = vjNewTan1 * uTan1Y;
        T vjNewTan1Z = vjNewTan1 * uTan1Z;

        T vjNewTan2X = vjNewTan2 * uTan2X;
        T vjNewTan2Y = vjNewTan2 * uTan2Y;
        T vjNewTan2Z = vjNewTan2 * uTan2Z;

        // 7. Find the final velocity vectors by adding the normal and
        // tangential components
        this->velocities.x[iBody] = viNewNormX + viNewTan1X + viNewTan2X;
        this->velocities.y[iBody] = viNewNormY + viNewTan1Y + viNewTan2Y;
        this->velocities.z[iBody] = viNewNormZ + viNewTan1Z + viNewTan2Z;

        this->velocities.x[jBody] = vjNewNormX + vjNewTan1X + vjNewTan2X;
        this->velocities.y[jBody] = vjNewNormY + vjNewTan1Y + vjNewTan2Y;
        this->velocities.z[jBody] = vjNewNormZ + vjNewTan1Z + vjNewTan2Z;
      }
    }
  }
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class BodiesCollision<double>;
#else
template class BodiesCollision<float>;
#endif
// ====================================================================================
// explicit template instantiation
