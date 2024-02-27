/*!
 * \file    Perf.cpp
 * \brief   Tools for performance analysis of codes.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#include <cassert>
#include <iostream>
#include <sys/time.h>

#include "Perf.h"

Perf::Perf() : tStart(0), tStop(0) {}

Perf::Perf(const Perf &p) : tStart(p.tStart), tStop(p.tStop) {}

Perf::Perf(float ms) : tStart(0), tStop(ms * 1000) {}

Perf::~Perf() {}

void Perf::start() { this->tStart = Perf::getTime(); }

void Perf::stop() { this->tStop = Perf::getTime(); }

void Perf::reset() {
  this->tStart = 0;
  this->tStop = 0;
}

float Perf::getElapsedTime() { return (this->tStop - this->tStart) / 1000.f; }

float Perf::getGflops(float flops) {
  return (flops * (1000 / this->getElapsedTime())) / 1024.0 / 1024.0 / 1024.0;
}

float Perf::getMemoryBandwidth(unsigned long memops, unsigned short nBytes) {
  return (memops * nBytes * (1000 / this->getElapsedTime())) / 1024.0 / 1024.0 /
         1024.0;
}

Perf Perf::operator+(const Perf &p) {
  Perf pAdd;
  pAdd.tStop = (p.tStop - p.tStart) + (this->tStop - this->tStart);
  return pAdd;
}

Perf Perf::operator+=(const Perf &p) {
  this->tStop += p.tStop - p.tStart;
  return (*this);
}

unsigned long Perf::getTime() {
  struct timeval t;

  int ret = gettimeofday(&t, NULL);
  assert(ret == 0);

  if (ret == 0)
    return t.tv_sec * 1000000 + t.tv_usec;
  else
    return 0;
}
