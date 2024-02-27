/*!
 * \file    Perf.h
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
#ifndef PERF_H_
#define PERF_H_

class Perf {
private:
  unsigned long tStart;
  unsigned long tStop;

public:
  Perf();
  Perf(const Perf &p);
  Perf(float ms);
  virtual ~Perf();

  void start();
  void stop();
  void reset();

  float getElapsedTime();       // ms
  float getGflops(float flops); // Gflops/s
  float getMemoryBandwidth(unsigned long memops, unsigned short nBytes); // Go/s

  Perf operator+(const Perf &p);
  Perf operator+=(const Perf &p);

protected:
  static unsigned long getTime();
};

#endif /* PERF_H_ */
