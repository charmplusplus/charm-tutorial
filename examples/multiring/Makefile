# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)

OBJS = multiRing_Main.o multiRing_Ring.o

all: multiRing

multiRing: $(OBJS)
	$(CHARMC) -language charm++ -o multiRing $(OBJS)

multiRing_Main.decl.h: multiRing_Main.ci
	$(CHARMC) multiRing_Main.ci

multiRing_Ring.decl.h: multiRing_Ring.ci
	$(CHARMC) multiRing_Ring.ci

multiRing_Main.o: multiRing_Main.C multiRing_Main.decl.h multiRing_Ring.decl.h
	$(CHARMC) -c multiRing_Main.C

multiRing_Ring.o: multiRing_Ring.C multiRing_Ring.decl.h multiRing_Main.decl.h
	$(CHARMC) -c multiRing_Ring.C

clean:
	rm -f *.decl.h *.def.h *.o multiRing charmrun

test: all
	./charmrun ++local +p3 ./multiRing 3 5 3 10 1 8 2
