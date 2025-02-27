# Common definitions
CFLAGS = -std=c++17 -Og -g -Wall -Wextra
#CFLAGS = -std=c++17 -O3 -DNDEBUG
LFLAGS = $(CFLAGS)
LIBS   = -lm

TGTS = heated-plate-parallel_mpi
# if build from a single source file
OBJS = heated-plate-parallel_mpi.o
# if build from multiple obj files

# default is example_debug
all: heated-plate-parallel_mpi

heated-plate-parallel_mpi: $(OBJS)
	$(MPICXX) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.cpp
	$(MPICXX) -c $(CFLAGS) $<

clean:
	$(RM) *.o
	$(RM) $(TGTS)