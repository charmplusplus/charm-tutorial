# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = primes_b.o

all: primes_b

primes_b: $(OBJS)
	$(CHARMC) -language charm++ -o primes_b $(OBJS)

primes_b.decl.h: primes_b.ci
	$(CHARMC) primes_b.ci

primes_b.o: primes_b.C primes_b.decl.h
	$(CHARMC) -c primes_b.C

clean:
	rm -f *.decl.h *.def.h *.o primes_b charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./primes_b 1000 1
