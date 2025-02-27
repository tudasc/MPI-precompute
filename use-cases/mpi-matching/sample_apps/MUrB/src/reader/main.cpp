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
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace std;

#include "../common/ogl/OGLSpheresVisuGS.h"
#include "../common/ogl/OGLSpheresVisuInst.h"

#include "../common/utils/ArgumentsReader.h"
#include "../common/utils/Perf.h"

#include "../common/core/Bodies.h"

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

/* global variables */
string RootInputFileName;     /*!< Root input file name for read bodies. */
unsigned long NBodies;        /*!< Number of bodies. */
unsigned long NIterations;    /*!< Number of iterations. */
bool Verbose = false;         /*!< Mode verbose. */
bool GSEnable = false;        /*!< Enable geometry shader. */
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
  map<string, string> reqArgs, faculArgs, docArgs;
  Arguments_reader argsReader(argc, argv);

  reqArgs["f"] = "rootInputFileName";
  docArgs["f"] =
      "the root file name of the body file(s) to read, do not use with -n "
      "(you can put 'data/in/p1/8bodies').";

  faculArgs["v"] = "";
  docArgs["v"] = "enable verbose mode.";
  faculArgs["h"] = "";
  docArgs["h"] = "display this help.";
  faculArgs["-help"] = "";
  docArgs["-help"] = "display this help.";
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

  if (argsReader.parse_arguments(reqArgs, faculArgs)) {
    RootInputFileName = argsReader.get_argument("f");
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
  if (argsReader.exist_argument("-gs"))
    GSEnable = true;
  if (argsReader.exist_argument("-ww"))
    WinWidth = stoi(argsReader.get_argument("-ww"));
  if (argsReader.exist_argument("-wh"))
    WinHeight = stoi(argsReader.get_argument("-wh"));
}

/*!
 * \fn     SpheresVisu* selectImplementationAndAllocateVisu(Bodies<T> *bodies)
 * \brief  Select and allocate an n-body visualization object.
 *
 * \param  bodies : An bodies object.
 * \tparam T      : Type.
 *
 * \return A fresh allocated visualization.
 */
template <typename T>
SpheresVisu *selectImplementationAndAllocateVisu(Bodies<T> *bodies) {
  SpheresVisu *visu;

  const T *positionsX = bodies->getPositionsX();
  const T *positionsY = bodies->getPositionsY();
  const T *positionsZ = bodies->getPositionsZ();
  const T *radiuses = bodies->getRadiuses();

  if (GSEnable) // geometry shader = better performances on dedicated GPUs
    visu = new OGLSpheresVisuGS<T>("MUrB reader (geometry shader)", WinWidth,
                                   WinHeight, positionsX, positionsY,
                                   positionsZ, radiuses, bodies->getN());
  else
    visu = new OGLSpheresVisuInst<T>("MUrB reader (instancing)", WinWidth,
                                     WinHeight, positionsX, positionsY,
                                     positionsZ, radiuses, bodies->getN());
  cout << "\n";

  return visu;
}

/*!
 * \fn     void countNIterationsAndNBodies(unsigned long &nIterations, unsigned
 * long &nBodies) \brief  Initialize nIterations and nBodies from files.
 *
 * \param  nIterations : number of iterations.
 * \tparam nBodies     : number of bodies in each iteration.
 */
void countNIterationsAndNBodies(unsigned long &nIterations,
                                unsigned long &nBodies) {
  // count number of iterations and number of bodies
  nBodies = 0;
  nIterations = 0;
  ifstream file;
  string fileName =
      RootInputFileName + ".i" + to_string(nIterations) + ".p0.dat";
  file.open(fileName.c_str(), std::ios::in);
  if (file.is_open()) {
    /* for ASCII files
    file >> nBodies;
    file.close();
    */

    /* for binary files */
    char cn[sizeof(unsigned long)];
    file.read(cn, sizeof(unsigned long));
    unsigned long *tmp = (unsigned long *)cn;
    nBodies = *tmp;

    bool searchingContinue;
    do {
      fileName =
          RootInputFileName + ".i" + to_string(nIterations + 1) + ".p0.dat";
      file.open(fileName.c_str(), std::ios::in | ios::binary);

      if (file.is_open()) {
        /* for ASCII files
        unsigned long curnBodies = 0;
        file >> curNBodies;
        */

        /* for binary files */
        file.read(cn, sizeof(unsigned long));
        unsigned long *tmp = (unsigned long *)cn;
        unsigned long curNBodies = *tmp;

        if (curNBodies != nBodies) {
          cout << "The number of bodies per iteration is not always the "
                  "same... exiting."
               << "\n";
          exit(-1);
        }

        file.close();
        nIterations++;
        searchingContinue = true;
      } else
        searchingContinue = false;

    } while (searchingContinue);
  } else {
    cout << "Unable to read \"" + fileName + "\" file... exiting."
         << "\n";
    exit(-1);
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
  // read arguments from the command line
  // usage: ./nbody -f fileName [-v] [--gs] ...
  argsReader(argc, argv);

  // initialize NIterations & NBodies
  countNIterationsAndNBodies(NIterations, NBodies);

  // create a bodies object
  string fileName = RootInputFileName + ".i0.p0.dat";
  bool binMode = true;
  Bodies<floatType> *bodies = new Bodies<floatType>(fileName, binMode);

  // get MB used for this visualization
  float Mbytes = bodies->getAllocatedBytes() / 1024.f / 1024.f;

  // display reader configuration
  cout << "n-body reader configuration:"
       << "\n";
  cout << "----------------------------"
       << "\n";
  cout << "  -> input file name(s) : " << RootInputFileName << ".i*.p0.dat"
       << "\n";
  cout << "  -> nb. of bodies      : " << NBodies << "\n";
  cout << "  -> nb. of iterations  : " << NIterations << "\n";
  cout << "  -> verbose mode       : " << ((Verbose) ? "enable" : "disable")
       << "\n";
  cout << "  -> mem. allocated     : " << Mbytes << " MB"
       << "\n";
  cout << "  -> geometry shader    : " << ((GSEnable) ? "enable" : "disable")
       << "\n";
  cout << "  -> window width       : " << WinWidth << "\n";
  cout << "  -> window height      : " << WinHeight << "\n"
       << "\n";

  // initialize visualization of bodies (with spheres in space)
  SpheresVisu *visu = selectImplementationAndAllocateVisu<floatType>(bodies);

  cout << "Visualization is ready... (press space bar to start)"
       << "\n";

  // display initial conditions
  visu->refreshDisplay();

  // loop over the iterations
  Perf readIte, readTotal, lastTimePressedButton;

  lastTimePressedButton.start();
  bool visuPause = true;
  unsigned long iIte = 1;
  short speed = 1;
  while (!visu->windowShouldClose() && iIte <= NIterations) {
    // refresh the display in OpenGL window
    visu->refreshDisplay();

    if (!visuPause) {
      // read bodies from file
      fileName = RootInputFileName + ".i" + to_string(iIte) + ".p0.dat";
      readIte.start();
      bodies->readFromFileBinary(fileName);
      readIte.stop();
      readTotal += readIte;

      // display the status of this iteration
      if (Verbose)
        cout << "Reading iteration n°" << iIte << " file (" << fileName
             << ") took " << readIte.getElapsedTime() << " ms (speed is "
             << speed << ")"
             << "\n";

      if ((long)iIte + (long)speed < 0) {
        iIte = 0;
        speed = 1;
        visuPause = true;
        cout << "Automatic pause, press space bar to continue..."
             << "\n";
      } else
        iIte += speed;
    }

    // play/pause management
    if (visu->pressedSpaceBar()) {
      lastTimePressedButton.stop();
      if (lastTimePressedButton.getElapsedTime() > 500) {
        if (visuPause) {
          visuPause = false;
          cout << "Visualization is running!"
               << "\n";
        } else {
          visuPause = true;
          cout << "Pause, press space bar to continue..."
               << "\n";
        }
        lastTimePressedButton.start();
      }
    }

    // speed management
    if (visu->pressedPageUp()) {
      lastTimePressedButton.stop();
      if (lastTimePressedButton.getElapsedTime() > 500 &&
          (iIte + speed) <= NIterations) {
        speed++;
        if (speed == 0)
          speed++;
        lastTimePressedButton.start();
        cout << "Current speed is " << speed << "."
             << "\n";
      }
    }
    if (visu->pressedPageDown()) {
      lastTimePressedButton.stop();
      if (lastTimePressedButton.getElapsedTime() > 500 &&
          ((long)iIte + speed) >= 0) {
        speed--;
        if (speed == 0)
          speed--;
        lastTimePressedButton.start();
        cout << "Current speed is " << speed << "."
             << "\n";
      }
    }
  }

  cout << "Visualization ended."
       << "\n"
       << "\n";
  cout << "Entire visualization took " << readTotal.getElapsedTime() << " ms"
       << "\n";

  // free resources
  delete visu;
  delete bodies;

  return EXIT_SUCCESS;
}
