# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = particles.o

all: particles

particles: $(OBJS)
	$(CHARMC) -language charm++ -o particles $(OBJS)

particles.decl.h: particles.ci
	$(CHARMC) particles.ci

particles.o: particles.C particles.decl.h
	$(CHARMC) -c particles.C

clean:
	rm -f *.decl.h *.def.h *.o particles charmrun

# k=4 nPerCell=10 maxIter=20  (small, fast, easy to inspect)
# k=16 nPerCell=100 maxIter=100  (recommended from exercise)
test: all
	./charmrun ++local +p4 ++ppn 2 ./particles 4 10 20
	./charmrun ++local +p4 ++ppn 2 ./particles 16 100 100
