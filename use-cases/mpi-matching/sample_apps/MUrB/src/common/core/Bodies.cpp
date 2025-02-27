/*!
 * \file    Bodies.hxx
 * \brief   Bodies container.
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
#include <mipp.h>
#include <string>
#include <sys/stat.h>

#include "../utils/Perf.h"

#include "Bodies.h"

template <typename T>
Bodies<T>::Bodies()
    : n(0), masses(nullptr), radiuses(nullptr), nVecs(0), padding(0),
      allocatedBytes(0) {}

template <typename T>
Bodies<T>::Bodies(const unsigned long n, const unsigned long randInit)
    : n(n), masses(nullptr), radiuses(nullptr),
      nVecs(ceil((T)n / (T)mipp::N<T>())),
      padding((this->nVecs * mipp::N<T>()) - this->n), allocatedBytes(0) {
  assert(n > 0);
  this->initRandomly(randInit);
}

template <typename T>
Bodies<T>::Bodies(const std::string inputFileName, bool binMode)
    : n(0), masses(nullptr), radiuses(nullptr), nVecs(0), padding(0),
      allocatedBytes(0) {
  if (binMode) {
    if (!this->initFromFileBinary(inputFileName))
      exit(-1);
  } else {
    if (!this->initFromFile(inputFileName))
      exit(-1);
  }
}

template <typename T>
Bodies<T>::Bodies(const Bodies<T> &bodies)
    : n(bodies.n), masses(bodies.masses), radiuses(bodies.radiuses),
      nVecs(bodies.nVecs), padding(bodies.padding),
      allocatedBytes(bodies.allocatedBytes) {
  this->positions.x = bodies.positions.x;
  this->positions.y = bodies.positions.y;
  this->positions.z = bodies.positions.z;

  this->velocities.x = bodies.velocities.x;
  this->velocities.y = bodies.velocities.y;
  this->velocities.z = bodies.velocities.z;
}

template <typename T> void Bodies<T>::hardCopy(const Bodies<T> &bodies) {
  assert((this->n + this->padding) == (bodies.n + bodies.padding));

  this->n = bodies.n;
  this->padding = bodies.padding;
  this->nVecs = bodies.nVecs;
  this->allocatedBytes = bodies.allocatedBytes;

  for (unsigned long iBody = 0; iBody < this->n + this->padding; iBody++) {
    this->masses[iBody] = bodies.masses[iBody];
    this->radiuses[iBody] = bodies.radiuses[iBody];
    this->positions.x[iBody] = bodies.positions.x[iBody];
    this->positions.y[iBody] = bodies.positions.y[iBody];
    this->positions.z[iBody] = bodies.positions.z[iBody];
    this->velocities.x[iBody] = bodies.velocities.x[iBody];
    this->velocities.y[iBody] = bodies.velocities.y[iBody];
    this->velocities.z[iBody] = bodies.velocities.z[iBody];
  }
}

template <typename T> void Bodies<T>::allocateBuffers() {
  this->masses = mipp::malloc<T>(this->n + this->padding);
  this->radiuses = mipp::malloc<T>(this->n + this->padding);

  this->positions.x = mipp::malloc<T>(this->n + this->padding);
  this->positions.y = mipp::malloc<T>(this->n + this->padding);
  this->positions.z = mipp::malloc<T>(this->n + this->padding);

  this->velocities.x = mipp::malloc<T>(this->n + this->padding);
  this->velocities.y = mipp::malloc<T>(this->n + this->padding);
  this->velocities.z = mipp::malloc<T>(this->n + this->padding);

  this->allocatedBytes = (this->n + this->padding) * sizeof(T) * 8;
}

template <typename T> Bodies<T>::~Bodies() {
  if (this->masses != nullptr) {
    mipp::free(this->masses);
    this->masses = nullptr;
  }

  if (this->radiuses != nullptr) {
    mipp::free(this->radiuses);
    this->radiuses = nullptr;
  }

  if (this->positions.x != nullptr) {
    mipp::free(this->positions.x);
    this->positions.x = nullptr;
  }
  if (this->positions.y != nullptr) {
    mipp::free(this->positions.y);
    this->positions.y = nullptr;
  }
  if (this->positions.z != nullptr) {
    mipp::free(this->positions.z);
    this->positions.z = nullptr;
  }

  if (this->velocities.x != nullptr) {
    mipp::free(this->velocities.x);
    this->velocities.x = nullptr;
  }
  if (this->velocities.y != nullptr) {
    mipp::free(this->velocities.y);
    this->velocities.y = nullptr;
  }
  if (this->velocities.z != nullptr) {
    mipp::free(this->velocities.z);
    this->velocities.z = nullptr;
  }
}

template <typename T> const unsigned long &Bodies<T>::getN() const {
  return const_cast<const unsigned long &>(this->n);
}

template <typename T> const unsigned long &Bodies<T>::getNVecs() const {
  return const_cast<const unsigned long &>(this->nVecs);
}

template <typename T> const unsigned short &Bodies<T>::getPadding() const {
  return const_cast<const unsigned short &>(this->padding);
}

template <typename T> const T *Bodies<T>::getMasses() const {
  return const_cast<const T *>(this->masses);
}

template <typename T> const T *Bodies<T>::getRadiuses() const {
  return const_cast<const T *>(this->radiuses);
}

template <typename T> const T *Bodies<T>::getPositionsX() const {
  return const_cast<const T *>(this->positions.x);
}

template <typename T> const T *Bodies<T>::getPositionsY() const {
  return const_cast<const T *>(this->positions.y);
}

template <typename T> const T *Bodies<T>::getPositionsZ() const {
  return const_cast<const T *>(this->positions.z);
}

template <typename T> const T *Bodies<T>::getVelocitiesX() const {
  return const_cast<const T *>(this->velocities.x);
}

template <typename T> const T *Bodies<T>::getVelocitiesY() const {
  return const_cast<const T *>(this->velocities.y);
}

template <typename T> const T *Bodies<T>::getVelocitiesZ() const {
  return const_cast<const T *>(this->velocities.z);
}

template <typename T> const float &Bodies<T>::getAllocatedBytes() const {
  return this->allocatedBytes;
}

template <typename T>
void Bodies<T>::setBody(const unsigned long &iBody, const T &mi, const T &ri,
                        const T &qiX, const T &qiY, const T &qiZ, const T &viX,
                        const T &viY, const T &viZ) {
  this->masses[iBody] = mi;

  this->radiuses[iBody] = ri;

  this->positions.x[iBody] = qiX;
  this->positions.y[iBody] = qiY;
  this->positions.z[iBody] = qiZ;

  this->velocities.x[iBody] = viX;
  this->velocities.y[iBody] = viY;
  this->velocities.z[iBody] = viZ;
}

/* create a galaxy...
template <typename T>
void Bodies<T>::initRandomly(const unsigned long randInit)
{
        this->allocateBuffers();

        srand(randInit);
        for(unsigned long iBody = 0; iBody < this->n; iBody++)
        {
                srand(iBody);
                T mi, ri, qiX, qiY, qiZ, viX, viY, viZ;

                if(iBody == 0)
                {
                        mi = 2.0e24;
                        ri = 0.0e6;
                        qiX = 0.0;
                        qiY = 0.0;
                        qiZ = 0.0;
                        viX = 0;
                        viY = 0;
                        viZ = 0;
                }
                else
                {
                        mi = ((rand() / (T) RAND_MAX) * 0.1e20);
                        ri = mi * 5.0e-15;

                        T horizontalAngle = ((RAND_MAX - rand()) / (T)
(RAND_MAX)) * 2.0 * M_PI; T verticalAngle   = ((RAND_MAX - rand()) / (T)
(RAND_MAX)) * 2.0 * M_PI; T distToCenter    = ((RAND_MAX - rand()) / (T)
(RAND_MAX)) * 1.0e8 + 1.0e8;

                        qiX = std::cos(verticalAngle) *
std::sin(horizontalAngle) * distToCenter; qiY = std::sin(verticalAngle)
* distToCenter; qiZ = std::cos(verticalAngle) * std::cos(horizontalAngle) *
distToCenter;

                        viX =  qiY * 4.0e-6;
                        viY = -qiX * 4.0e-6;
                        viZ =  0.0e2;
                }

                this->setBody(iBody, mi, ri, qiX, qiY, qiZ, viX, viY, viZ);
        }

        // fill the bodies in the padding zone
        for(unsigned long iBody = this->n; iBody < this->n + this->padding;
iBody++)
        {
                T qiX, qiY, qiZ, viX, viY, viZ;

                qiX = ((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * (5.0e8
* 1.33); qiY = ((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * 5.0e8; qiZ =
((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * 5.0e8 -10.0e8;

                viX = ((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * 1.0e2;
                viY = ((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * 1.0e2;
                viZ = ((rand() - RAND_MAX/2) / (T) (RAND_MAX/2)) * 1.0e2;

                this->setBody(iBody, 0, 0, qiX, qiY, qiZ, viX, viY, viZ);
        }
}
*/

/* real random */
template <typename T>
void Bodies<T>::initRandomly(const unsigned long randInit) {
  this->allocateBuffers();

  srand(randInit);
  for (unsigned long iBody = 0; iBody < this->n; iBody++) {
    T mi, ri, qiX, qiY, qiZ, viX, viY, viZ;

    mi = ((rand() / (T)RAND_MAX) * 5.0e21);

    ri = mi * 0.5e-14;

    qiX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * (5.0e8 * 1.33);
    qiY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8;
    qiZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8 - 10.0e8;

    viX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;

    this->setBody(iBody, mi, ri, qiX, qiY, qiZ, viX, viY, viZ);
  }

  // fill the bodies in the padding zone
  for (unsigned long iBody = this->n; iBody < this->n + this->padding;
       iBody++) {
    T qiX, qiY, qiZ, viX, viY, viZ;

    qiX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * (5.0e8 * 1.33);
    qiY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8;
    qiZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8 - 10.0e8;

    viX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;

    this->setBody(iBody, 0, 0, qiX, qiY, qiZ, viX, viY, viZ);
  }
}

template <typename T>
bool Bodies<T>::initFromFile(const std::string inputFileName) {
  std::ifstream bodiesFile;
  bodiesFile.open(inputFileName.c_str(), std::ios::in);

  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << inputFileName << "\" file (reading)."
              << "\n";
    return false;
  }

  bool isOk = this->read(bodiesFile);

  bodiesFile.close();

  if (!isOk) {
    std::cout << "Something bad occurred during the reading of \""
              << inputFileName << "\" file... exiting."
              << "\n";
    return false;
  }

  return true;
}

template <typename T>
bool Bodies<T>::initFromFileBinary(const std::string inputFileName) {
  std::ifstream bodiesFile;
  bodiesFile.open(inputFileName.c_str(), std::ios::in | std::ios::binary);

  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << inputFileName << "\" file (reading)."
              << "\n";
    return false;
  }

  bool isOk = this->readBinary(bodiesFile);

  bodiesFile.close();

  if (!isOk) {
    std::cout << "Something bad occurred during the reading of \""
              << inputFileName << "\" file... exiting."
              << "\n";
    return false;
  }

  return true;
}

template <typename T>
void Bodies<T>::updatePositionsAndVelocities(const vector3<T> &accelerations,
                                             T &dt) {
  // flops = n * 18
  for (unsigned long iBody = 0; iBody < this->n; iBody++) {
    T mi, ri, qiX, qiY, qiZ, viX, viY, viZ;

    mi = this->masses[iBody];

    ri = this->radiuses[iBody];

    T aiXDt = accelerations.x[iBody] * dt;
    T aiYDt = accelerations.y[iBody] * dt;
    T aiZSt = accelerations.z[iBody] * dt;

    qiX = this->positions.x[iBody] +
          (this->velocities.x[iBody] + aiXDt * 0.5) * dt;
    qiY = this->positions.y[iBody] +
          (this->velocities.y[iBody] + aiYDt * 0.5) * dt;
    qiZ = this->positions.z[iBody] +
          (this->velocities.z[iBody] + aiZSt * 0.5) * dt;

    viX = this->velocities.x[iBody] + aiXDt;
    viY = this->velocities.y[iBody] + aiYDt;
    viZ = this->velocities.z[iBody] + aiZSt;

    this->setBody(iBody, mi, ri, qiX, qiY, qiZ, viX, viY, viZ);
  }
}

template <typename T>
bool Bodies<T>::readFromFile(const std::string inputFileName) {
  // assert(this->n == 0);
  return this->initFromFile(inputFileName);
}

template <typename T>
bool Bodies<T>::readFromFileBinary(const std::string inputFileName) {
  // assert(this->n == 0);
  return this->initFromFileBinary(inputFileName);
}

template <typename T> bool Bodies<T>::read(std::istream &stream) {
  // this->n = 0;
  unsigned long newN = 0;
  stream >> newN;

  this->nVecs = ceil((T)newN / (T)mipp::N<T>());
  this->padding = (this->nVecs * mipp::N<T>()) - newN;

  if (newN && (this->n == 0)) {
    this->n = newN;
    this->allocateBuffers();
  } else if (newN && (this->n == newN)) {
    this->n = newN;
  } else
    return false;

  for (unsigned long iBody = 0; iBody < this->n; iBody++) {
    T mi, ri, qiX, qiY, qiZ, viX, viY, viZ;

    stream >> mi;

    stream >> ri;

    stream >> qiX;
    stream >> qiY;
    stream >> qiZ;

    stream >> viX;
    stream >> viY;
    stream >> viZ;

    this->setBody(iBody, mi, ri, qiX, qiY, qiZ, viX, viY, viZ);

    if (!stream.good())
      return false;
  }

  // fill the bodies in the padding zone
  for (unsigned long iBody = this->n; iBody < this->n + this->padding;
       iBody++) {
    T qiX, qiY, qiZ, viX, viY, viZ;

    qiX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * (5.0e8 * 1.33);
    qiY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8;
    qiZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8 - 10.0e8;

    viX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;

    this->setBody(iBody, 0, 0, qiX, qiY, qiZ, viX, viY, viZ);
  }

  return true;
}

template <typename T> bool Bodies<T>::readBinary(std::istream &stream) {
  // this->n = 0;
  char cn[sizeof(unsigned long)];
  stream.read(cn, sizeof(unsigned long));
  unsigned long *tmp = (unsigned long *)cn;
  unsigned long newN = *tmp;

  this->nVecs = ceil((T)newN / (T)mipp::N<T>());
  this->padding = (this->nVecs * mipp::N<T>()) - newN;

  if (newN && (this->n == 0)) {
    this->n = newN;
    this->allocateBuffers();
  } else if (newN && (this->n == newN))
    this->n = newN;
  else
    return false;

  stream.read((char *)this->masses, this->n * sizeof(T));
  stream.read((char *)this->radiuses, this->n * sizeof(T));
  stream.read((char *)this->positions.x, this->n * sizeof(T));
  stream.read((char *)this->positions.y, this->n * sizeof(T));
  stream.read((char *)this->positions.z, this->n * sizeof(T));
  stream.read((char *)this->velocities.x, this->n * sizeof(T));
  stream.read((char *)this->velocities.y, this->n * sizeof(T));
  stream.read((char *)this->velocities.z, this->n * sizeof(T));

  // fill the bodies in the padding zone
  for (unsigned long iBody = this->n; iBody < this->n + this->padding;
       iBody++) {
    T qiX, qiY, qiZ, viX, viY, viZ;

    qiX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * (5.0e8 * 1.33);
    qiY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8;
    qiZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 5.0e8 - 10.0e8;

    viX = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viY = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;
    viZ = ((rand() - RAND_MAX / 2) / (T)(RAND_MAX / 2)) * 1.0e2;

    this->setBody(iBody, 0, 0, qiX, qiY, qiZ, viX, viY, viZ);
  }

  return true;
}

template <typename T>
void Bodies<T>::write(std::ostream &stream, bool writeN) const {
  if (writeN)
    stream << this->n << "\n";

  for (unsigned long iBody = 0; iBody < this->n; iBody++)
    stream << this->masses[iBody] << " " << this->radiuses[iBody] << " "
           << this->positions.x[iBody] << " " << this->positions.y[iBody] << " "
           << this->positions.z[iBody] << " " << this->velocities.x[iBody]
           << " " << this->velocities.y[iBody] << " "
           << this->velocities.z[iBody] << "\n";
}

template <typename T>
void Bodies<T>::writeBinary(std::ostream &stream, bool writeN) const {
  if (writeN) {
    char const *cn = reinterpret_cast<char const *>(&(this->n));
    stream.write(cn, sizeof(unsigned long));
  }

  char const *cmi = reinterpret_cast<char const *>(this->masses);
  char const *cri = reinterpret_cast<char const *>(this->radiuses);
  char const *cqiX = reinterpret_cast<char const *>(this->positions.x);
  char const *cqiY = reinterpret_cast<char const *>(this->positions.y);
  char const *cqiZ = reinterpret_cast<char const *>(this->positions.z);
  char const *cviX = reinterpret_cast<char const *>(this->velocities.x);
  char const *cviY = reinterpret_cast<char const *>(this->velocities.y);
  char const *cviZ = reinterpret_cast<char const *>(this->velocities.z);

  stream.write(cmi, this->n * sizeof(T));
  stream.write(cri, this->n * sizeof(T));
  stream.write(cqiX, this->n * sizeof(T));
  stream.write(cqiY, this->n * sizeof(T));
  stream.write(cqiZ, this->n * sizeof(T));
  stream.write(cviX, this->n * sizeof(T));
  stream.write(cviY, this->n * sizeof(T));
  stream.write(cviZ, this->n * sizeof(T));
}

template <typename T>
bool Bodies<T>::writeIntoFile(const std::string outputFileName) const {
  std::fstream bodiesFile(outputFileName.c_str(), std::ios_base::out);
  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << outputFileName
              << "\" file (writing). Exiting..."
              << "\n";
    return false;
  }

  this->write(bodiesFile);

  bodiesFile.close();

  return true;
}

template <typename T>
bool Bodies<T>::writeIntoFileBinary(const std::string outputFileName) const {
  std::fstream bodiesFile(outputFileName.c_str(),
                          std::ios::out | std::ios::binary);
  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << outputFileName
              << "\" file (writing). Exiting..."
              << "\n";
    return false;
  }

  this->writeBinary(bodiesFile);

  bodiesFile.close();

  return true;
}

template <typename T>
bool Bodies<T>::writeIntoFileMPI(const std::string outputFileName,
                                 const unsigned long MPINBodies) const {
  std::fstream bodiesFile;

  if (MPINBodies)
    bodiesFile.open(outputFileName.c_str(),
                    std::fstream::out | std::fstream::trunc);
  else
    bodiesFile.open(outputFileName.c_str(),
                    std::fstream::out | std::fstream::app);

  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << outputFileName
              << "\" file (writing). Exiting..."
              << "\n";
    return false;
  }

  if (MPINBodies)
    bodiesFile << MPINBodies << "\n";

  bool writeN = false;
  this->write(bodiesFile, writeN);

  bodiesFile.close();

  return true;
}

template <typename T>
bool Bodies<T>::writeIntoFileMPIBinary(const std::string outputFileName,
                                       const unsigned long MPINBodies) const {
  std::fstream bodiesFile;

  if (MPINBodies)
    bodiesFile.open(outputFileName.c_str(),
                    std::fstream::out | std::ios::binary | std::fstream::trunc);
  else
    bodiesFile.open(outputFileName.c_str(),
                    std::fstream::out | std::ios::binary | std::fstream::app);

  if (!bodiesFile.is_open()) {
    std::cout << "Can't open \"" << outputFileName
              << "\" file (writing). Exiting..."
              << "\n";
    return false;
  }

  if (MPINBodies) {
    char const *cnmpi = reinterpret_cast<char const *>(&MPINBodies);
    bodiesFile.write(cnmpi, sizeof(unsigned long));
  }

  bool writeN = false;
  this->writeBinary(bodiesFile, writeN);

  bodiesFile.close();

  return true;
}

template <typename T>
std::ostream &operator<<(std::ostream &o, const Bodies<T> &s) {
  s.write(o);
  return o;
}

// ====================================================================================
// explicit template instantiation
#ifdef NBODY_DOUBLE
template class Bodies<double>;
template std::ostream &operator<< <double>(std::ostream &o,
                                           const Bodies<double> &s);
#else
template class Bodies<float>;
template std::ostream &operator<< <float>(std::ostream &o,
                                          const Bodies<float> &s);
#endif
// ====================================================================================
// explicit template instantiation
