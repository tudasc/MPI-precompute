# Program skeleton slicing

This Repository contains the llvm pass that computes a program skeleton needed to

## Prerequisites

For this Project, we used clang/`llvm 16.0.1`

## Building / Usage

Currently. There are two different use cases:

* MPI Matching: Precalculation of MPI message envelopes. Camke Option: `MPI_USE_CASE`
* Thread Sanitizer: Skeleton based on Tsan Instrumentation without computation. Camke Option: `SANITIZER_USE_CASE`
  Use The Cmake Options to build them.

Refer to the Readme in each use cases directory for more information about its usage.