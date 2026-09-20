CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = ring.o

all: ring

ring: $(OBJS)
	$(CHARMC) -language charm++ -o ring $(OBJS)

ring.decl.h: ring.ci
	$(CHARMC) ring.ci

ring.o: ring.C ring.decl.h
	$(CHARMC) -c ring.C

clean:
	rm -f *.decl.h *.def.h *.o ring charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./ring 5 3
