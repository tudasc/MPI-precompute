/*!
 * \file    ToMPIDatatype.h
 * \brief   Convert standard C++ types into MPI datatypes.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#ifndef TO_MPI_DATATYPE_H_
#define TO_MPI_DATATYPE_H_

#include <mpi.h>

inline int MPI_get_rank() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return rank;
}
inline int MPI_get_size() {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}

template <typename T> class ToMPIDatatype {
public:
  static inline MPI_Datatype value();
};

template <> inline MPI_Datatype ToMPIDatatype<double>::value() {
  return MPI_DOUBLE;
}

template <> inline MPI_Datatype ToMPIDatatype<float>::value() {
  return MPI_FLOAT;
}

template <> inline MPI_Datatype ToMPIDatatype<long>::value() {
  return MPI_LONG;
}

template <> inline MPI_Datatype ToMPIDatatype<int>::value() {
  return MPI_INTEGER;
}

#endif /* TO_MPI_DATATYPE_H_ */
