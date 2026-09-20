CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
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
