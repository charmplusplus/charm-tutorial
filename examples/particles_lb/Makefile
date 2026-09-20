CHARMC=/Users/kale/software/clusterFinding/charm/bin/charmc $(OPTS)
OBJS = particles.o

all: particles

particles: $(OBJS)
	$(CHARMC) -language charm++ -module EveryLB -o particles $(OBJS)

particles.decl.h: particles.ci
	$(CHARMC) particles.ci

particles.o: particles.C particles.decl.h
	$(CHARMC) -c particles.C

clean:
	rm -f *.decl.h *.def.h *.o particles particles-trace particles-summary charmrun
	rm -rf projlogs

# ── Projections / tracing builds ─────────────────────────────────────────────

# Binary Projections logs — open with ~/software/projections-7.0.0/bin/projections
particles-trace: $(OBJS)
	$(CHARMC) -language charm++ -tracemode projections -module EveryLB -o particles-trace $(OBJS)

# Text summary logs — .sum files readable without the GUI
particles-summary: $(OBJS)
	$(CHARMC) -language charm++ -tracemode summary -module EveryLB -o particles-summary $(OBJS)

# Generate binary logs (no LB, to see raw imbalance)
trace-nolb: particles-trace
	mkdir -p projlogs
	./charmrun ++local +p4 ++ppn 2 ./particles-trace 4 10000 20 +traceroot ./projlogs

# Generate binary logs (with GreedyRefineLB, to compare)
trace-lb: particles-trace
	mkdir -p projlogs-lb
	./charmrun ++local +p4 ++ppn 2 ./particles-trace 4 10000 20 +balancer GreedyRefineLB +traceroot ./projlogs-lb

# Generate text summary (no LB)
summary-nolb: particles-summary
	mkdir -p summary-nolb
	./charmrun ++local +p4 ++ppn 2 ./particles-summary 4 10000 20 +traceroot ./summary-nolb

# Generate text summary (with LB)
summary-lb: particles-summary
	mkdir -p summary-lb
	./charmrun ++local +p4 ++ppn 2 ./particles-summary 4 10000 20 +balancer GreedyRefineLB +traceroot ./summary-lb

# No balancer — shows raw imbalance (nPerCell=10000 so compute dominates message overhead)
test-nolb: all
	./charmrun ++local +p4 ++ppn 2 ./particles 4 10000 20
	./charmrun ++local +p4 ++ppn 2 ./particles 16 2000 20

# With GreedyRefineLB
test-lb: all
	./charmrun ++local +p4 ++ppn 2 ./particles 4 10000 20 +balancer GreedyRefineLB
	./charmrun ++local +p4 ++ppn 2 ./particles 16 2000 20 +balancer GreedyRefineLB

test: test-nolb test-lb
