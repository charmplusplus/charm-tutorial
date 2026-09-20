#include <cstdlib>
#include "reduction.decl.h"

#define DEFAULT_NUM_ELEMS 10

class Main : public CBase_Main {
public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    int numElems = m->argc > 1 ? atoi(m->argv[1]) : DEFAULT_NUM_ELEMS;
    CkPrintf("reduction: %d elements, expected sum = %d\n",
             numElems, numElems*(numElems-1)/2);
    delete m;
    CProxy_Elem::ckNew(thisProxy, numElems);
  }

  void printResult(int result) {
    CkPrintf("result = %d\n", result);
    CkExit();
  }
};

class Elem : public CBase_Elem {
public:
  Elem(CkMigrateMessage *m) {}
  Elem(CProxy_Main mainProxy) {
    int value = thisIndex;
    CkCallback cb(CkReductionTarget(Main, printResult), mainProxy);
    contribute(sizeof(int), &value, CkReduction::sum_int, cb);
  }
};

#include "reduction.def.h"
