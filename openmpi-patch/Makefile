# Common definitions
#CC = clang
#CC = gcc
#MPICC = mpicc
#MPICC = scorep mpicc

# module load openmpi/test hwloc/2.5.0

# Compiler flags, paths and libraries
# Include path showing to openmpi internals
MPI_SRC_PATH=/home/tj75qeje/openmpi-4.1.1

INCLUDE=-I$(MPI_SRC_PATH) -I$(MPI_SRC_PATH)/opal/include -I$(MPI_SRC_PATH)/ompi/include/ -I$(MPI_SRC_PATH)/orte/include

CFLAGS = -std=c11 -Og -g $(INCLUDE) -Wall -Wextra -Wno-unused-parameter
#CFLAGS = -std=c11 -O3 -march=native -mtune=native -DNDEBUG $(INCLUDE)
LFLAGS = $(CFLAGS)
LIBS   = -lopen-pal -lucp -lm

TGTS = example one-sided-persistent.c
# if build from a single source file
OBJS = example.o one-sided-persistent.o
# if build from multiple obj files
ALLOBJS = example.o globals.o interface.o finalization.o handshake.o initialization.o start.o test.o wait.o
SOURCES = finalization.c globals.c interface.h handshake.c initialization.c start.c test.c wait.c
HEADERS = one-sided-persistent.h settings.h request_type.h mpi-internals.h interface.h finalization.h handshake.h initialization.h start.h test.h wait.h

# default is example_debug
all: one-sided-persistent.c example_debug

example_debug: $(ALLOBJS)
	$(MPICC) $(LFLAGS) -o example $(ALLOBJS) $(LIBS)

example: $(OBJS)
	$(MPICC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)
	
one-sided-persistent.c: $(SOURCES) $(HEADERS)
	$(MPICC) -E $(CFLAGS) -DLINKAGE_TYPE=static one-sided-persistent.h -o $@

%.o: %.c
	$(MPICC) -c $(CFLAGS) $<

clean:
	$(RM) *.o
	$(RM) $(TGTS)
