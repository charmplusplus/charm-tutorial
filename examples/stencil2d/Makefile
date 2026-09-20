# Point CHARM_HOME at your Charm++ build directory, e.g.
#   make CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
# or export CHARM_HOME once in your shell.
CHARM_HOME ?= $(HOME)/charm
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)
OBJS = stencil2d.o

all: stencil2d

stencil2d: $(OBJS)
	$(CHARMC) -language charm++ -o stencil2d $(OBJS)

stencil2d.decl.h: stencil2d.ci
	$(CHARMC) stencil2d.ci

stencil2d.o: stencil2d.C stencil2d.decl.h
	$(CHARMC) -c stencil2d.C

clean:
	rm -f *.decl.h *.def.h *.o stencil2d charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./stencil2d 64 8 0.01
