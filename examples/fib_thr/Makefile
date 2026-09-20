# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = fib_thr.o

all: fib_thr

fib_thr: $(OBJS)
	$(CHARMC) -language charm++ -o fib_thr $(OBJS)

fib_thr.decl.h: fib_thr.ci
	$(CHARMC) fib_thr.ci

fib_thr.o: fib_thr.C fib_thr.decl.h
	$(CHARMC) -c fib_thr.C

clean:
	rm -f *.decl.h *.def.h *.o fib_thr charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./fib_thr 20
	./charmrun ++local +p4 ++ppn 2 ./fib_thr 30
