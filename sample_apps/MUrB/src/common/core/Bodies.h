/*!
 * \file    Bodies.h
 * \brief   Bodies container.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 */
#ifndef BODIES_H_
#define BODIES_H_

#include <string>

template <typename T> class SimulationNBodyMPI;

/*!
 * \struct vector3
 * \brief  Structure of 3 arrays.
 *
 * \tparam T : Type.
 *
 * The vector3 structure represent 3D data.
 */
template <typename T = double> struct vector3 {
  T *x; /*!< First dimension array. */
  T *y; /*!< Second dimension array. */
  T *z; /*!< Third dimension array. */
};

/*!
 * \class  Bodies
 * \brief  Bodies class represents the physic data of each body (mass, radius,
 * position and velocity).
 *
 * \tparam T : Type.
 */
template <typename T = double> class Bodies {
  friend SimulationNBodyMPI<T>;

protected:
  unsigned long n;       /*!< Number of bodies. */
  T *masses;             /*!< Array of masses. */
  T *radiuses;           /*!< Array of radiuses. */
  vector3<T> positions;  /*!< 3D array of positions. */
  vector3<T> velocities; /*!< 3D array of velocities. */
  unsigned long
      nVecs; /*!< Number of vectors (nVecs = n / nbOfElementsInOneVector). */
  unsigned short
      padding; /*!< Number of fictional bodies to fill the last vector */
  // stats
  float allocatedBytes; /*!< Number of allocated bytes. */

public:
  /*!
   *  \brief Constructor.
   *
   *  Bodies constructor.
   */
  Bodies();

  /*!
   *  \brief Constructor.
   *
   *  Bodies constructor : generates random bodies in space.
   *
   *  \param n        : Number of bodies.
   *  \param randInit : Initialization number for random generation.
   */
  Bodies(const unsigned long n, const unsigned long randInit = 0);

  /*!
   *  \brief Constructor.
   *
   *  Bodies constructor : reads bodies from file.
   *
   *  \param inputFileName : name of the input file to read.
   */
  Bodies(const std::string inputFileName, bool binMode = false);

  /*!
   *  \brief Constructor.
   *
   *  Bodies copy constructor (do not make a copy of buffers !!).
   *
   *  \param bodies : An other Bodies object.
   */
  Bodies(const Bodies<T> &bodies);

  /*!
   *  \brief Destructor.
   *
   *  Bodies destructor.
   */
  virtual ~Bodies();

  /*!
   *  \brief Make an hard copy from a other Bodies object.
   *
   *  This method copies the buffers and the attributes from bodies object.
   *
   *  \param bodies : A Bodies object.
   */
  void hardCopy(const Bodies<T> &bodies);

  /*!
   *  \brief N getter.
   *
   *  \return The number of bodies.
   */
  const unsigned long &getN() const;

  /*!
   *  \brief NVecs getter.
   *
   *  \return The number of vectors.
   */
  const unsigned long &getNVecs() const;

  /*!
   *  \brief Padding getter.
   *
   *  \return The number of bodies in the padding zone.
   */
  const unsigned short &getPadding() const;

  /*!
   *  \brief Masses getter.
   *
   *  \return Array of masses.
   */
  const T *getMasses() const;

  /*!
   *  \brief Radiuses getter.
   *
   *  \return Array of radiuses.
   */
  const T *getRadiuses() const;

  /*!
   *  \brief Positions getter.
   *
   *  \return Array of positions (first dimension).
   */
  const T *getPositionsX() const;

  /*!
   *  \brief Positions getter.
   *
   *  \return Array of positions (second dimension).
   */
  const T *getPositionsY() const;

  /*!
   *  \brief Positions getter.
   *
   *  \return Array of positions (third dimension).
   */
  const T *getPositionsZ() const;

  /*!
   *  \brief Velocities getter.
   *
   *  \return Array of velocities (first dimension).
   */
  const T *getVelocitiesX() const;

  /*!
   *  \brief Velocities getter.
   *
   *  \return Array of velocities (second dimension).
   */
  const T *getVelocitiesY() const;

  /*!
   *  \brief Velocities getter.
   *
   *  \return Array of velocities (third dimension).
   */
  const T *getVelocitiesZ() const;

  /*!
   *  \brief Allocated bytes getter.
   *
   *  \return The number of allocated bytes.
   */
  const float &getAllocatedBytes() const;

  /*!
   *  \brief Update positions and velocities array.
   *
   *  \param accelerations : The array of accelerations needed to compute new
   * positions and velocities. \param dt            : The time step value
   * (required for time integration scheme).
   *
   *  Update positions and velocities, this is the time integration scheme to
   * apply after each iteration.
   */
  void updatePositionsAndVelocities(const vector3<T> &accelerations, T &dt);

  /*!
   *  \brief Read bodies from a file.
   *
   *  \param inputFileName : Name of the input file.
   *
   *  \return True if this operation is a success, false else.
   */
  bool readFromFile(const std::string inputFileName);

  /*!
   *  \brief Read bodies from a file (binary mode).
   *
   *  \param inputFileName : Name of the input file.
   *
   *  \return True if this operation is a success, false else.
   */
  bool readFromFileBinary(const std::string inputFileName);

  /*!
   *  \brief Write bodies into a stream.
   *
   *  \param stream : The C++ stream to write into.
   *  \param writeN : Write the number of bodies in the stream if true, do not
   * write this number else.
   */
  void write(std::ostream &stream, bool writeN = true) const;

  /*!
   *  \brief Write bodies into a stream (binary mode).
   *
   *  \param stream : The C++ stream to write into.
   *  \param writeN : Write the number of bodies in the stream if true, do not
   * write this number else.
   */
  void writeBinary(std::ostream &stream, bool writeN = true) const;

  /*!
   *  \brief Write bodies into a file.
   *
   *  \param outputFileName : The output file name.
   *
   *  \return True if this operation is a success, false else.
   */
  bool writeIntoFile(const std::string outputFileName) const;

  /*!
   *  \brief Write bodies into a file (binary mode).
   *
   *  \param outputFileName : The output file name.
   *
   *  \return True if this operation is a success, false else.
   */
  bool writeIntoFileBinary(const std::string outputFileName) const;

  /*!
   *  \brief Write bodies into a file (MPI specific method).
   *
   *  \param outputFileName : The output file name.
   *  \param MPINBodies     : Number of bodies (or 0 if we don't want to write
   * the bodies number).
   *
   *  \return True if this operation is a success, false else.
   */
  bool writeIntoFileMPI(const std::string outputFileName,
                        const unsigned long MPINBodies = 0) const;

  /*!
   *  \brief Write bodies into a file (MPI specific method, binary mode).
   *
   *  \param outputFileName : The output file name.
   *  \param MPINBodies     : Number of bodies (or 0 if we don't want to write
   * the bodies number).
   *
   *  \return True if this operation is a success, false else.
   */
  bool writeIntoFileMPIBinary(const std::string outputFileName,
                              const unsigned long MPINBodies = 0) const;

protected:
  /*!
   *  \brief Deallocation of buffers.
   */
  void deallocateBuffers();

  /*!
   *  \brief Body setter.
   *
   *  \param iBody : Body i id.
   *  \param mi    : Body i mass.
   *  \param ri    : Body i radius.
   *  \param qiX   : Body i position x.
   *  \param qiY   : Body i position y.
   *  \param qiZ   : Body i position z.
   *  \param viX   : Body i velocity x.
   *  \param viY   : Body i velocity y.
   *  \param viZ   : Body i velocity z.
   */
  inline void setBody(const unsigned long &iBody, const T &mi, const T &ri,
                      const T &qiX, const T &qiY, const T &qiZ, const T &viX,
                      const T &viY, const T &viZ);

  /*!
   *  \brief Read bodies from a stream.
   *
   *  \param stream : Stream to read.
   *
   *  \return True if this operation is a success, false else.
   */
  bool read(std::istream &stream);

  /*!
   *  \brief Read bodies from a stream (binary mode).
   *
   *  \param stream : Stream to read.
   *
   *  \return True if this operation is a success, false else.
   */
  bool readBinary(std::istream &stream);

  /*!
   *  \brief Allocation of buffers.
   */
  void allocateBuffers();

  /*!
   *  \brief Initialized bodies randomly.
   *
   *  \param randInit : Initialization number for random generation.
   */
  void initRandomly(const unsigned long randInit = 0);

  /*!
   *  \brief Initialized bodies from a file.
   *
   *  \param inputFileName : Name of the input file.
   */
  bool initFromFile(const std::string inputFileName);

  /*!
   *  \brief Initialized bodies from a file (binary mode).
   *
   *  \param inputFileName : Name of the input file.
   */
  bool initFromFileBinary(const std::string inputFileName);
};

/*!
 * \fn     std::ostream& operator<<(std::ostream &o, const Bodies<T>& s)
 * \brief  Overlap "<< operator" for writing output Bodies.
 *
 * \param  o      : A standard stream.
 * \tparam Bodies : Bodies to write.
 *
 * \return The updated stream o.
 */
template <typename T>
std::ostream &operator<<(std::ostream &o, const Bodies<T> &s);

#endif /* BODIES_H_ */
