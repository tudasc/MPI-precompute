/*!
 * \file    main.cpp
 * \brief   Code entry.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#ifdef NBODY_DOUBLE
using floatType = double;
#else
using floatType = float;
#endif

#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include "../common/ogl/SpheresVisu.h"
#include "../common/ogl/SpheresVisuNo.h"
#ifdef VISU
#include "../common/ogl/OGLSpheresVisuGS.h"
#include "../common/ogl/OGLSpheresVisuInst.h"
#endif

#include "../common/utils/ArgumentsReader.h"
#include "../common/utils/Perf.h"

#include "../common/core/Bodies.h"

#include "core/SimulationNBody.h"
#include "core/collisionless/v1/local/SimulationNBodyV1.h"
#include "core/collisionless/v1/local/SimulationNBodyV1CB.h"
#include "core/collisionless/v1/local/SimulationNBodyV1Intrinsics.h"
#include "core/collisionless/v1/local/SimulationNBodyV1Vectors.h"
#include "core/collisionless/v2/local/SimulationNBodyV2CB.h"
#include "core/collisionless/v2/local/SimulationNBodyV2Intrinsics.h"
#include "core/collisionless/v2/local/SimulationNBodyV2Vectors.h"
#include "core/collisionless/v3/local/SimulationNBodyV3.h"
#include "core/collisionless/v3/local/SimulationNBodyV3Intrinsics.h"
#include "core/collisionless/v3/local/SimulationNBodyV3IntrinsicsBH.h"

#include "core/collision/v1/local/SimulationNBodyCollisionV1.h"

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

#ifdef USE_MPI
#include "../common/utils/ToMPIDatatype.h"
#include "core/collisionless/v1/mpi/SimulationNBodyMPIV1.h"
#include "core/collisionless/v1/mpi/SimulationNBodyMPIV1Intrinsics.h"
#include <mpi.h>
#else
#ifndef NO_MPI
#define NO_MPI
class MPI_Comm {
public:
  MPI_Comm(){};
  virtual ~MPI_Comm(){};
};
static MPI_Comm MPI_COMM_WORLD;
static inline void MPI_Init(int *argc, char ***argv) {}
static inline void MPI_Finalize() {}
static inline int MPI_get_rank() { return 0; }
static inline int MPI_get_size() { return 1; }
static inline int MPI_Barrier(MPI_Comm comm) { return 0; }
static inline int MPI_Abort(MPI_Comm comm, int val) {
  exit(val);
  return 0;
}
#endif
#endif

/* global variables */
string RootInputFileName;     /*!< Root input file name for read bodies. */
string RootOutputFileName;    /*!< Root output file name for write bodies. */
unsigned long NBodies;        /*!< Number of bodies. */
unsigned long NIterations;    /*!< Number of iterations. */
unsigned int ImplId = 10;     /*!< Implementation id. */
bool Verbose = false;         /*!< Mode verbose. */
bool GSEnable = false;        /*!< Enable geometry shader. */
bool VisuEnable = true;       /*!< Enable visualization. */
bool DtVariable = false;      /*!< Variable time step. */
floatType Dt = 3600;          /*!< Time step in seconds. */
floatType MinDt = 200;        /*!< Minimum time step. */
floatType Softening = 0.035;  /*!< Softening factor value. */
unsigned int WinWidth = 800;  /*!< Window width for visualization. */
unsigned int WinHeight = 600; /*!< Window height for visualization. */

/*!
 * \fn     void argsReader(int argc, char** argv)
 * \brief  Read arguments from command line and set global variables.
 *
 * \param  argc : Number of arguments.
 * \param  argv : Array of arguments.
 */
void argsReader(int argc, char **argv) {
  map<string, string> reqArgs1, reqArgs2, faculArgs, docArgs;
  Arguments_reader argsReader(argc, argv);

  reqArgs1["n"] = "nBodies";
  docArgs["n"] = "the number of bodies randomly generated.";
  reqArgs1["i"] = "nIterations";
  docArgs["i"] = "the number of iterations to compute.";

  reqArgs2["f"] = "rootInputFileName";
  docArgs["f"] =
      "the root file name of the body file(s) to read, do not use with -n "
      "(you can put 'data/in/p1/8bodies').";
  reqArgs2["i"] = "nIterations";

  faculArgs["v"] = "";
  docArgs["v"] = "enable verbose mode.";
  faculArgs["w"] = "rootOutputFileName";
  docArgs["w"] = "the root file name of the body file(s) to write (you can put "
                 "'data/out/bodies').";
  faculArgs["h"] = "";
  docArgs["h"] = "display this help.";
  faculArgs["-help"] = "";
  docArgs["-help"] = "display this help.";
  faculArgs["-dt"] = "timeStep";
  docArgs["-dt"] = "select a fixed time step in second (default is " +
                   to_string(Dt) + " sec).";
  faculArgs["-gs"] = "";
  docArgs["-gs"] =
      "enable geometry shader for visu, "
      "this is faster than the standard way but not all GPUs can support it.";
  faculArgs["-ww"] = "winWidth";
  docArgs["-ww"] = "the width of the window in pixel (default is " +
                   to_string(WinWidth) + ").";
  faculArgs["-wh"] = "winHeight";
  docArgs["-wh"] = "the height of the window in pixel (default is " +
                   to_string(WinHeight) + ").";
  faculArgs["-nv"] = "";
  docArgs["-nv"] = "no visualization (disable visu).";
  faculArgs["-vdt"] = "";
  docArgs["-vdt"] = "enable variable time step.";
  faculArgs["-mdt"] = "minTimeStep";
  docArgs["-mdt"] =
      "select the minimal time step (default is " + to_string(MinDt) + " sec).";
  faculArgs["-im"] = "ImplId";
  docArgs["-im"] = "code implementation id (value should be 10, 11, 12, 13, "
                   "14, 20, 21, 22, 23, 30, 33, 34, 100 or 103).";
  faculArgs["-soft"] = "softeningFactor";
  docArgs["-soft"] = "softening factor for implementation 15.";

  if (argsReader.parse_arguments(reqArgs1, faculArgs)) {
    NBodies = stoi(argsReader.get_argument("n")) / MPI_get_size();
    NIterations = stoi(argsReader.get_argument("i"));
    RootInputFileName = "";
  } else if (argsReader.parse_arguments(reqArgs2, faculArgs)) {
    RootInputFileName = argsReader.get_argument("f");
    NIterations = stoi(argsReader.get_argument("i"));
  } else {
    if (argsReader.parse_doc_args(docArgs))
      argsReader.print_usage();
    else
      cout << "A problem was encountered when parsing arguments "
              "documentation... exiting."
           << "\n";
    exit(-1);
  }

  if (argsReader.exist_argument("h") || argsReader.exist_argument("-help")) {
    if (argsReader.parse_doc_args(docArgs))
      argsReader.print_usage();
    else
      cout << "A problem was encountered when parsing arguments "
              "documentation... exiting."
           << "\n";
    exit(-1);
  }

  if (argsReader.exist_argument("v"))
    Verbose = true;
  if (argsReader.exist_argument("w"))
    RootOutputFileName = argsReader.get_argument("w");
  if (argsReader.exist_argument("-dt"))
    Dt = stof(argsReader.get_argument("-dt"));
  if (argsReader.exist_argument("-gs"))
    GSEnable = true;
  if (argsReader.exist_argument("-ww"))
    WinWidth = stoi(argsReader.get_argument("-ww"));
  if (argsReader.exist_argument("-wh"))
    WinHeight = stoi(argsReader.get_argument("-wh"));
  if (argsReader.exist_argument("-nv"))
    VisuEnable = false;
  if (argsReader.exist_argument("-vdt"))
    DtVariable = true;
  if (argsReader.exist_argument("-mdt"))
    MinDt = stof(argsReader.get_argument("-mdt"));
  if (argsReader.exist_argument("-im"))
    ImplId = stoi(argsReader.get_argument("-im"));
  if (argsReader.exist_argument("-soft")) {
    Softening = stof(argsReader.get_argument("-soft"));
    if (Softening == (floatType)0) {
      cout << "Softening factor can't be equal to 0... exiting."
           << "\n";
      exit(-1);
    }
  }
}

/*!
 * \fn     string strDate(T timestamp)
 * \brief  Convert a timestamp into a string "..d ..h ..m ..s".
 *
 * \param  Timestamp : The timestamp to convert
 * \tparam T         : Timestamp type.
 *
 * \return Date as a string.
 */
template <typename T> string strDate(T timestamp) {
  unsigned int days;
  unsigned int hours;
  unsigned int minutes;
  T rest;

  days = timestamp / (24 * 60 * 60);
  rest = timestamp - (days * 24 * 60 * 60);

  hours = rest / (60 * 60);
  rest = rest - (hours * 60 * 60);

  minutes = rest / 60;
  rest = rest - (minutes * 60);

  stringstream res;
  res << setprecision(0) << std::fixed << setw(4) << days << "d "
      << setprecision(0) << std::fixed << setw(4) << hours << "h "
      << setprecision(0) << std::fixed << setw(4) << minutes << "m "
      << setprecision(3) << std::fixed << setw(5) << rest << "s";

  return res.str();
}

// #define ONLY_MPI_IMPLEMENTATIONS

/*!
 * \fn     SimulationNBody<T>* selectImplementationAndAllocateSimulation()
 * \brief  Select and allocate an n-body simulation object.
 *
 * \tparam T : Type.
 *
 * \return A fresh allocated simulation.
 */
template <typename T>
SimulationNBody<T> *selectImplementationAndAllocateSimulation() {
  SimulationNBody<floatType> *simu = nullptr;

  string inputFileName =
      RootInputFileName + ".p" + to_string(MPI_get_rank()) + ".dat";

  switch (ImplId) {
#ifndef ONLY_MPI_IMPLEMENTATIONS
  case 10:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV1<T>(NBodies);
    else
      simu = new SimulationNBodyV1<T>(inputFileName);
    break;
  case 11:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 + cache blocking - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV1CB<T>(NBodies);
    else
      simu = new SimulationNBodyV1CB<T>(inputFileName);
    break;
  case 12:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 + vectors - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV1Vectors<T>(NBodies);
    else
      simu = new SimulationNBodyV1Vectors<T>(inputFileName);
    break;
  case 13:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 + intrinsics - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV1Intrinsics<T>(NBodies);
    else
      simu = new SimulationNBodyV1Intrinsics<T>(inputFileName);
    break;
  case 14:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 + collisions - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyCollisionV1<T>(NBodies);
    else
      simu = new SimulationNBodyCollisionV1<T>(inputFileName);
    break;
  case 20:
    if (!MPI_get_rank())
      cout << "Selected implementation: V2 - O(n²/2)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV2<T>(NBodies);
    else
      simu = new SimulationNBodyV2<T>(inputFileName);
    break;
  case 21:
    if (!MPI_get_rank())
      cout << "Selected implementation: V2 + cache blocking - O(n²/2)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV2CB<T>(NBodies);
    else
      simu = new SimulationNBodyV2CB<T>(inputFileName);
    break;
  case 22:
    if (!MPI_get_rank())
      cout << "Selected implementation: V2 + vectors - O(n²/2)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV2Vectors<T>(NBodies);
    else
      simu = new SimulationNBodyV2Vectors<T>(inputFileName);
    break;
  case 23:
    cout << "Selected implementation: V2 + intrinsics - O(n²/2)"
         << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV2Intrinsics<T>(NBodies);
    else
      simu = new SimulationNBodyV2Intrinsics<T>(inputFileName);
    break;
  case 30:
    if (!MPI_get_rank())
      cout << "Selected implementation: V3 (softening factor) - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV3<T>(NBodies, Softening);
    else
      simu = new SimulationNBodyV3<T>(inputFileName, Softening);
    break;
  case 33:
    if (!MPI_get_rank())
      cout << "Selected implementation: V3 (softening factor) + intrinsics - "
              "O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV3Intrinsics<T>(NBodies, Softening);
    else
      simu = new SimulationNBodyV3Intrinsics<T>(inputFileName, Softening);
    break;
  case 34:
    if (!MPI_get_rank())
      cout << "Selected implementation: V3 (softening factor) + intrinsics "
              "Black Hole - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyV3IntrinsicsBH<T>(NBodies, Softening);
    else
      simu = new SimulationNBodyV3IntrinsicsBH<T>(inputFileName, Softening);
    break;
#endif
#ifndef NO_MPI
  case 100:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 MPI - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyMPIV1<T>(NBodies);
    else
      simu = new SimulationNBodyMPIV1<T>(inputFileName);
    break;
  case 103:
    if (!MPI_get_rank())
      cout << "Selected implementation: V1 MPI + intrinsics - O(n²)"
           << "\n\n";
    if (RootInputFileName.empty())
      simu = new SimulationNBodyMPIV1Intrinsics<T>(NBodies);
    else
      simu = new SimulationNBodyMPIV1Intrinsics<T>(inputFileName);
    break;
#endif
  default:
    if (!MPI_get_rank())
      cout << "This code implementation does not exist... Exiting."
           << "\n";
    MPI_Finalize();
    exit(-1);
    break;
  }

  if (ImplId < 100 && MPI_get_size() > 1) {
    if (!MPI_get_rank())
      cout << "Implementation n°" << ImplId
           << " is not an MPI implementation... Exiting."
           << "\n";
    MPI_Finalize();
    exit(-1);
  }

  return simu;
}

/*!
 * \fn     SpheresVisu* selectImplementationAndAllocateVisu(SimulationNBody<T>
 * *simu) \brief  Select and allocate an n-body visualization object.
 *
 * \param  simu : A simulation.
 * \tparam T    : Type.
 *
 * \return A fresh allocated visualization.
 */
template <typename T>
SpheresVisu *selectImplementationAndAllocateVisu(SimulationNBody<T> *simu) {
  SpheresVisu *visu;

#ifdef VISU
  // only the MPI proc 0 can display the bodies
  if (MPI_get_rank())
    VisuEnable = false;

  if (VisuEnable) {
    const T *positionsX = simu->getBodies()->getPositionsX();
    const T *positionsY = simu->getBodies()->getPositionsY();
    const T *positionsZ = simu->getBodies()->getPositionsZ();
    const T *radiuses = simu->getBodies()->getRadiuses();

    if (GSEnable) // geometry shader = better performances on dedicated GPUs
      visu = new OGLSpheresVisuGS<T>("MUrB n-body (geometry shader)", WinWidth,
                                     WinHeight, positionsX, positionsY,
                                     positionsZ, radiuses, NBodies);
    else
      visu = new OGLSpheresVisuInst<T>("MUrB n-body (instancing)", WinWidth,
                                       WinHeight, positionsX, positionsY,
                                       positionsZ, radiuses, NBodies);
    cout << "\n";
  } else
    visu = new SpheresVisuNo<T>();
#else
  VisuEnable = false;
  visu = new SpheresVisuNo<T>();
#endif

  return visu;
}

/*!
 * \fn     void writeBodies(SimulationNBody<T> *simu, const unsigned long &iIte)
 * \brief  Write bodies from simu object to file.
 *
 * \param  simu : A simulation.
 * \param  iIte : Current iteration number.
 * \tparam T    : Type.
 */
template <typename T>
void writeBodies(SimulationNBody<T> *simu, const unsigned long &iIte) {
  string extension =
      RootOutputFileName.substr(RootOutputFileName.find_last_of(".") + 1);
  string tmpFileName;
  string outputFileName;

  // each process MPI writes its bodies in a common file
  // TODO: bad perfs
  if (extension == "dat") {
    string realRootOutputFileName =
        RootOutputFileName.substr(0, RootOutputFileName.find_last_of("."));
    tmpFileName = realRootOutputFileName + ".i" + to_string(iIte);
    outputFileName = tmpFileName + ".p0.dat";

    unsigned long MPINBodies = 0;
    if (!MPI_get_rank())
      MPINBodies = simu->getBodies()->getN() * MPI_get_size();

    for (unsigned long iRank = 0; iRank < (unsigned long)MPI_get_size();
         iRank++) {
      if (iRank == (unsigned long)MPI_get_rank())
        if (!simu->getBodies()->writeIntoFileMPI(outputFileName, MPINBodies))
          MPI_Abort(MPI_COMM_WORLD, 1);

      MPI_Barrier(MPI_COMM_WORLD);
    }
  } else // each process MPI writes its bodies in separate files
  {
    tmpFileName = RootOutputFileName + ".i" + to_string(iIte);
    outputFileName = tmpFileName + ".p" + to_string(MPI_get_rank()) + ".dat";
    simu->getBodies()->writeIntoFile(outputFileName);
  }

  if (Verbose && !MPI_get_rank()) {
    tmpFileName = tmpFileName + ".p*.dat";
    cout << "   Writing iteration n°" << iIte << " into \"" << tmpFileName
         << "\" file(s)."
         << "\n";
  }
}

/*!
 * \fn     int main(int argc, char** argv)
 * \brief  Code entry function.
 *
 * \param  argc : Number of command line arguments.
 * \param  argv : Array of command line arguments.
 *
 * \return EXIT_SUCCESS
 */
int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  // read arguments from the command line
  // usage: ./nbody -f fileName -i nIterations [-v] [-w] ...
  // usage: ./nbody -n nBodies  -i nIterations [-v] [-w] ...
  argsReader(argc, argv);

  // create the n-body simulation
  SimulationNBody<floatType> *simu =
      selectImplementationAndAllocateSimulation<floatType>();
  const unsigned long n = simu->getBodies()->getN();
  NBodies = n;

  // get MB used for this simulation
  float Mbytes = simu->getAllocatedBytes() / 1024.f / 1024.f;

  // display simulation configuration
  if (!MPI_get_rank()) {
    cout << "n-body simulation configuration:"
         << "\n";
    cout << "--------------------------------"
         << "\n";
    if (!RootInputFileName.empty())
      cout << "  -> input file name(s)    : " << RootInputFileName << ".p*.dat"
           << "\n";
    else
      cout << "  -> random mode           : enable"
           << "\n";
    if (!RootOutputFileName.empty())
      cout << "  -> output file name(s)   : " << RootOutputFileName
           << ".i*.p*.dat"
           << "\n";
    cout << "  -> total nb. of bodies   : " << NBodies * MPI_get_size() << "\n";
    cout << "  -> nb. of bodies per proc: " << NBodies << "\n";
    cout << "  -> nb. of iterations     : " << NIterations << "\n";
    cout << "  -> verbose mode          : "
         << ((Verbose) ? "enable" : "disable") << "\n";
    cout << "  -> precision             : "
         << ((sizeof(floatType) == 4) ? "simple" : "double") << "\n";
    cout << "  -> mem. allocated        : " << Mbytes << " MB"
         << "\n";
    cout << "  -> geometry shader       : "
         << ((GSEnable) ? "enable" : "disable") << "\n";
    cout << "  -> time step             : "
         << ((DtVariable) ? "variable" : to_string(Dt) + " sec") << "\n";
    if (ImplId >= 30 && ImplId <= 39)
      cout << "  -> softening factor      : " << Softening << "\n";
    cout << "  -> nb. of MPI procs      : " << MPI_get_size() << "\n";
    cout << "  -> nb. of threads        : " << omp_get_max_threads() << "\n"
         << "\n";
  }

  // initialize visualization of bodies (with spheres in space)
  SpheresVisu *visu = selectImplementationAndAllocateVisu<floatType>(simu);

  if (!MPI_get_rank())
    cout << "Simulation started..."
         << "\n";

  // write initial bodies into file
  if (!RootOutputFileName.empty())
    writeBodies<floatType>(simu, 0);

  // time step selection
  if (DtVariable)
    simu->setDtVariable(MinDt);
  else
    simu->setDtConstant(Dt);

  // loop over the iterations
  Perf perfIte, perfTotal;
  floatType physicTime = 0.0;
  unsigned long iIte;
  for (iIte = 1; iIte <= NIterations && !visu->windowShouldClose(); iIte++) {
    // refresh the display in OpenGL window
    visu->refreshDisplay();

    // simulation computations
    perfIte.start();
    simu->computeOneIteration();
    perfIte.stop();
    perfTotal += perfIte;

    // compute the elapsed physic time
    physicTime += simu->getDt();

    // display the status of this iteration
    if (Verbose && !MPI_get_rank())
      cout << "Iteration n°" << setw(4) << iIte << " took " << setprecision(3)
           << std::fixed << setw(5) << perfIte.getElapsedTime() << " ms ("
           << setprecision(3) << std::fixed << setw(5)
           << perfIte.getGflops(simu->getFlopsPerIte())
           << " Gflop/s), physic time: " << strDate(physicTime) << "\r";

    // write iteration results into file
    if (!RootOutputFileName.empty())
      writeBodies<floatType>(simu, iIte);
  }
  if (Verbose && !MPI_get_rank())
    cout << "\n";

  if (!MPI_get_rank())
    cout << "Simulation ended."
         << "\n"
         << "\n";

  if (!MPI_get_rank())
    cout << "Entire simulation took " << perfTotal.getElapsedTime() << " ms "
         << "(" << perfTotal.getGflops(simu->getFlopsPerIte() * (iIte - 1))
         << " Gflop/s)"
         << "\n";

  // free resources
  delete visu;
  delete simu;

  if (NIterations > (iIte + 1))
    MPI_Abort(MPI_COMM_WORLD, 0);

  MPI_Finalize();

  return EXIT_SUCCESS;
}
