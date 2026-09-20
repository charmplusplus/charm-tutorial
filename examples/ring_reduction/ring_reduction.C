#include <cstdlib>
#include <ctime>
#include "ring_reduction.decl.h"

class Main : public CBase_Main {
  int tripCount;
  int reductionsDone;
public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    if (m->argc < 3) {
      CkPrintf("Usage: %s <ringSize> <tripCount>\n", m->argv[0]);
      CkAbort("Need ringSize and tripCount");
    }
    int ringSize = atoi(m->argv[1]);
    tripCount    = atoi(m->argv[2]);
    reductionsDone = 0;
    delete m;

    CkPrintf("Ring+Reduction: size=%d trips=%d PEs=%d\n",
             ringSize, tripCount, CkNumPes());

    CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize, tripCount, ringSize);
    srand(time(NULL));
    ring(rand() % ringSize).doSomething(ringSize, tripCount, -1, -1);
  }

  // Called once per completed trip (T times total).
  // Expected sum for trip with tripsLeft=t: ringSize * t
  void reductionDone(int sum) {
    CkPrintf("Reduction result: %d\n", sum);
    if (++reductionsDone == tripCount) CkExit();
  }
};

class Ring : public CBase_Ring {
  CProxy_Main mainProxy;
  int ringSize;
  int tripCount;
public:
  Ring(CkMigrateMessage *m) {}
  Ring(CProxy_Main mp, int rs, int tc) : mainProxy(mp), ringSize(rs), tripCount(tc) {}

  int nextI() { return (thisIndex + 1) % ringSize; }

  void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE) {
    CkPrintf("Ring[%d](%d): tripsLeft=%d from [%d](%d)\n",
             thisIndex, CkMyPe(), tripsLeft, fromIndex, fromPE);

    // Each element contributes tripsLeft into this trip's sum reduction
    CkCallback cb(CkReductionTarget(Main, reductionDone), mainProxy);
    contribute(sizeof(int), &tripsLeft, CkReduction::sum_int, cb);

    if (elementsLeft > 1) {
      thisProxy(nextI()).doSomething(elementsLeft-1, tripsLeft, thisIndex, CkMyPe());
    } else if (tripsLeft > 1) {
      thisProxy(nextI()).doSomething(ringSize, tripsLeft-1, thisIndex, CkMyPe());
    }
    // On last element of last trip: ring is done; CkExit comes from reductionDone
  }
};

#include "ring_reduction.def.h"
