#include <cstdlib>
#include "multiRing_Ring.decl.h"

class Ring : public CBase_Ring {
  CProxy_Main mainProxy;  // proxy to the main chare
  int ringSize;           // number of elements in this ring
  int ringID;             // which ring this element belongs to

public:
  Ring(CkMigrateMessage *m) {}

  Ring(CProxy_Main mp, int rs, int rID)
      : mainProxy(mp), ringSize(rs), ringID(rID) {}

  // Index s positions further along this ring, wrapping around.
  int nextI(int s) { return (thisIndex + s) % ringSize; }

  void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE) {
    CkPrintf("Ring %d[%d](%d): tripsLeft = %d, from [%d](%d)\n",
             ringID, thisIndex, CkMyPe(), tripsLeft, fromIndex, fromPE);

    if (elementsLeft > 1) {
      // Skip a random number of elements rather than stepping to thisIndex+1.
      int skipAmount = (rand() % elementsLeft) + 1;
      thisProxy(nextI(skipAmount))
          .doSomething(elementsLeft - skipAmount, tripsLeft, thisIndex, CkMyPe());
    } else if (tripsLeft > 1) {
      thisProxy(nextI(1)).doSomething(ringSize, tripsLeft - 1, thisIndex, CkMyPe());
    } else {
      mainProxy.ringFinished();
    }
  }
};

#include "multiRing_Ring.def.h"
