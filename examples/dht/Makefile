CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = dht.o

all: dht

dht: $(OBJS)
	$(CHARMC) -language charm++ -o dht $(OBJS)

dht.decl.h: dht.ci
	$(CHARMC) dht.ci

dht.o: dht.C dht.decl.h
	$(CHARMC) -c dht.C

clean:
	rm -f *.decl.h *.def.h *.o dht charmrun

# numChares=8 keysPerChare=100 queriesPerChare=20
test: all
	./charmrun ++local +p4 ++ppn 2 ./dht 8 100 20
	./charmrun ++local +p4 ++ppn 2 ./dht 16 200 50
