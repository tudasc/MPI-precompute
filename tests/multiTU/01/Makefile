# Common definitions
CFLAGS ?= -Og -g -Wall -Wextra
LFLAGS ?= $(CFLAGS)
# for loading the pass:
LFLAGS ?= -Og -g -fpass-plugin=$(MPI_COMPILER_ASSISTANCE_PASS)
LIBS   = -lm

TGTS = a.out
OBJSBC = main.bc comm.bc
OBJS = all.bc

all: a.out

a.out: $(OBJS)
	$(MPICC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

all.bc: $(OBJSBC)
	llvm-link -o $@ $(OBJSBC)

%.bc: %.c
	$(MPICC) -c -emit-llvm $(CFLAGS) -o $@ $<

clean:
	$(RM) *.bc
	$(RM) $(TGTS)