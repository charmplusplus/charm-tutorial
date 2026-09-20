# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = sort.o

all: sort

sort: $(OBJS)
	$(CHARMC) -language charm++ -o sort $(OBJS)

sort.decl.h: sort.ci
	$(CHARMC) sort.ci

sort.o: sort.C sort.decl.h
	$(CHARMC) -c sort.C

clean:
	rm -f *.decl.h *.def.h *.o sort charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./sort 8
	./charmrun ++local +p4 ++ppn 2 ./sort 100
