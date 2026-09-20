CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = kmeans.o

all: kmeans

kmeans: $(OBJS)
	$(CHARMC) -language charm++ -o kmeans $(OBJS)

kmeans.decl.h: kmeans.ci
	$(CHARMC) kmeans.ci

kmeans.o: kmeans.C kmeans.decl.h
	$(CHARMC) -c kmeans.C

clean:
	rm -f *.decl.h *.def.h *.o kmeans charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./kmeans 10000 8 5
