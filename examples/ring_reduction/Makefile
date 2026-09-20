# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = ring_reduction.o

all: ring_reduction

ring_reduction: $(OBJS)
	$(CHARMC) -language charm++ -o ring_reduction $(OBJS)

ring_reduction.decl.h: ring_reduction.ci
	$(CHARMC) ring_reduction.ci

ring_reduction.o: ring_reduction.C ring_reduction.decl.h
	$(CHARMC) -c ring_reduction.C

clean:
	rm -f *.decl.h *.def.h *.o ring_reduction charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./ring_reduction 5 3
