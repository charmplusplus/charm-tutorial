# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = synth.o

all: synth

synth: $(OBJS)
	$(CHARMC) -language charm++ -module EveryLB -o synth $(OBJS)

synth.decl.h: synth.ci
	$(CHARMC) synth.ci

synth.o: synth.C synth.decl.h
	$(CHARMC) -c synth.C

clean:
	rm -f *.decl.h *.def.h *.o synth charmrun

# lbPeriod > maxIter disables LB; +balancer selects the strategy at runtime
test: all
	./charmrun ++local +p4 ++ppn 2 ./synth 16 100 999
	./charmrun ++local +p4 ++ppn 2 ./synth 16 100 10 +balancer GreedyRefineLB
