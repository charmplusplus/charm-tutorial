CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = fib_thr.o

all: fib_thr

fib_thr: $(OBJS)
	$(CHARMC) -language charm++ -o fib_thr $(OBJS)

fib_thr.decl.h: fib_thr.ci
	$(CHARMC) fib_thr.ci

fib_thr.o: fib_thr.C fib_thr.decl.h
	$(CHARMC) -c fib_thr.C

clean:
	rm -f *.decl.h *.def.h *.o fib_thr charmrun

test: all
	./charmrun ++local +p4 ++ppn 2 ./fib_thr 20
	./charmrun ++local +p4 ++ppn 2 ./fib_thr 30
