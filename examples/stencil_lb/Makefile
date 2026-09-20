CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = stencil_lb.o

all: stencil_lb

stencil_lb: $(OBJS)
	$(CHARMC) -language charm++ -module EveryLB -o stencil_lb $(OBJS)

stencil_lb.decl.h: stencil_lb.ci
	$(CHARMC) stencil_lb.ci

stencil_lb.o: stencil_lb.C stencil_lb.decl.h
	$(CHARMC) -c stencil_lb.C

clean:
	rm -f *.decl.h *.def.h *.o stencil_lb charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./stencil_lb 64 8 50 999
	./charmrun ++local +p4 ++ppn 2 ./stencil_lb 64 8 50 10 +balancer GreedyRefineLB
