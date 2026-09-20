#include <cstdlib>
#include <ctime>
#include "ring.decl.h"

class Main : public CBase_Main {
public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    if (m->argc < 3) {
      CkPrintf("Usage: %s <ringSize> <tripCount>\n", m->argv[0]);
      CkAbort("Need ringSize and tripCount");
    }
    int ringSize  = atoi(m->argv[1]);
    int tripCount = atoi(m->argv[2]);
    delete m;

    CkPrintf("Ring: size=%d trips=%d PEs=%d\n", ringSize, tripCount, CkNumPes());

    CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize, ringSize);
    srand(time(NULL));
    ring(rand() % ringSize).doSomething(ringSize, tripCount, -1, -1);
  }

  void ringFinished() { CkExit(); }
};

class Ring : public CBase_Ring {
  CProxy_Main mainProxy;
  int ringSize;
public:
  Ring(CkMigrateMessage *m) {}
  Ring(CProxy_Main mp, int rs) : mainProxy(mp), ringSize(rs) {}

  int nextI() { return (thisIndex + 1) % ringSize; }

  void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE) {
    CkPrintf("Ring[%d](%d): tripsLeft=%d from [%d](%d)\n",
             thisIndex, CkMyPe(), tripsLeft, fromIndex, fromPE);
    if (elementsLeft > 1) {
      thisProxy(nextI()).doSomething(elementsLeft-1, tripsLeft, thisIndex, CkMyPe());
    } else if (tripsLeft > 1) {
      thisProxy(nextI()).doSomething(ringSize, tripsLeft-1, thisIndex, CkMyPe());
    } else {
      mainProxy.ringFinished();
    }
  }
};

#include "ring.def.h"
