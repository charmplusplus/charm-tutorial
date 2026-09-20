# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = prefix.o

all: prefix

prefix: $(OBJS)
	$(CHARMC) -language charm++ -o prefix $(OBJS)

prefix.decl.h: prefix.ci
	$(CHARMC) prefix.ci

prefix.o: prefix.C prefix.decl.h
	$(CHARMC) -c prefix.C

clean:
	rm -f *.decl.h *.def.h *.o prefix charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./prefix 8
	./charmrun ++local +p4 ++ppn 2 ./prefix 16
	./charmrun ++local +p1 ++ppn 1 ./prefix 8
	./charmrun ++local +p2 ++ppn 2 ./prefix 8
