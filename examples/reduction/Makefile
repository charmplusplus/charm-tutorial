CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = reduction.o

all: reduction

reduction: $(OBJS)
	$(CHARMC) -language charm++ -o reduction $(OBJS)

reduction.decl.h: reduction.ci
	$(CHARMC) reduction.ci

reduction.o: reduction.C reduction.decl.h
	$(CHARMC) -c reduction.C

clean:
	rm -f *.decl.h *.def.h *.o reduction charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./reduction 10
