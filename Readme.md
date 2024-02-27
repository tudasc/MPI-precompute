# Compiler-Based Precalculation of MPI Message Envelopes

This Repo contains TODO introduction sentence, what this is about

## Prerequisites

For this Project, we used clang/`llvm 16.0.1`
`openmpi 4.1.1` will be downloaded and build during the cmake build step (see below)

## Building

Before Building with cmake, one may need to have a look at [openmpi-patch/CMakeLists.txt](openmpi-patch/CMakeLists.txt)
and change the ``OMPI_CONFIGURE_FLAGS`` variable to fit their system.
The variable is used to configure openmpi.
After one had adjusted the openmpi configuration, building with cmake is quite straightforward:

```
mkdir build && cd build
cmake ..
make -j
source setup_env.sh
ctest # run the tests to check if build was successful
```

## Usage

The build step creates a ``setup_env.sh`` file, that sets the required environment variables.
To build an application with the pass, replace the compiler to use with ``clang_wrap_cc`` or ``clang_wrap_cxx``
respectively.
The ``setup_env.sh`` automatically sets the compiler to use with ``mpicc`` and ``mpicxx`` to those wrappers.
In order to activate the pass, one needs to supply the environment
variable ``export USE_MPI_COMPILER_ASSISTANCE_PASS=true``
The compile step needs the following command line arguments to work
correctly ``-fno-inline -flto -fwhole-program-vtables``.
The ``-fno-inline`` will be removed after the analysis, so that inlining does happen.

## Build of MUrB example application

Before building MUrB be sure to init the submodules (``git submodule update --init --recursive``) to download the
prerequisite requirements; also refer to the [Readme](sample_apps/MUrB/README.md) in the MUrB directory.
MUrB can than be built using cmake, we used the following build settings with our Pass:

```
cmake .. -DCMAKE_CXX_COMPILER=$MPICXX -DCMAKE_CXX_FLAGS="-fopenmp -O3 -fno-inline -fuse-ld=lld -flto -fwhole-program-vtables" -DENABLE_MURB_MPI=ON -DENABLE_VISU=OFF -DENABLE_MURB_READER=OFF
mpirun -n 2 ./bin/murb -v --im 100 -i 10 -n 100 # to test if it runs
```

The file [sample_apps/scripts/showcase_experiment.sh](sample_apps/scripts/showcase_experiment.sh) details all steps
required to reproduce our measurements

