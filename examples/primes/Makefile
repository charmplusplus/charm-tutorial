# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = primes.o

all: primes

primes: $(OBJS)
	$(CHARMC) -language charm++ -o primes $(OBJS)

primes.decl.h: primes.ci
	$(CHARMC) primes.ci

primes.o: primes.C primes.decl.h
	$(CHARMC) -c primes.C

clean:
	rm -f *.decl.h *.def.h *.o primes charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./primes 20
