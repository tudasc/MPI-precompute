# Common definitions
#CC = clang
#CC = gcc
#MPICC = mpicc
#MPICC = scorep mpicc

# module load openmpi/test hwloc/2.5.0

# Compiler flags, paths and libraries
# Include npath showing to openmpoi internals
INCLUDE=-I/home/tj75qeje/openmpi-4.1.1/ -I/home/tj75qeje/openmpi-4.1.1/opal/include -I/home/tj75qeje/openmpi-4.1.1/ompi/include/ -I/home/tj75qeje/openmpi-4.1.1/orte/include

CFLAGS = -std=c11 -Og -g $(INCLUDE) -Wall -Wextra -Wno-unused-parameter
#CFLAGS = -std=c11 -O3 -DNDEBUG $(INCLUDE)
LFLAGS = $(CFLAGS)
LIBS   = -lopen-pal -lucp -lm

TGTS = example one-sided-persistent.c
OBJS = example.o one-sided-persistent.o

# Targets ...
all: $(TGTS)

example: $(OBJS)
	$(MPICC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)
	
one-sided-persistent.c: one-sided-persistent.h
	$(MPICC) -E $(CFLAGS) $< -o $@

one-sided-persistent.o: one-sided-persistent.c
	$(MPICC) -c $(CFLAGS) $<

example.o: example.c 
	$(MPICC) -c $(CFLAGS) $<

clean:
	$(RM) $(OBJS)
	$(RM) $(TGTS)