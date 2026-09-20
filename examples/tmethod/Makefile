# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = tmethod.o

all: tmethod

tmethod: $(OBJS)
	$(CHARMC) -language charm++ -module EveryLB -o tmethod $(OBJS)

tmethod.decl.h: tmethod.ci
	$(CHARMC) tmethod.ci

tmethod.o: tmethod.C tmethod.decl.h
	$(CHARMC) -c tmethod.C

clean:
	rm -f *.decl.h *.def.h *.o tmethod charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./tmethod 8 20 5
