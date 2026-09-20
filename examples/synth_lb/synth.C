#include "synth.decl.h"

/*readonly*/ int numWorkers;
/*readonly*/ int maxIter;
/*readonly*/ int lbPeriod;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Worker workerArray;

// Base steps per unit.  Actual work per iteration uses a two-term formula that
// shifts load from high-index to low-index chares over the course of the run:
//   n(i,t) = [(maxIter-t)*(i+1) + t*(N-i)] * WORK_UNIT / maxIter
// At t=0: n = (i+1)*WORK_UNIT  (last chare heaviest).
// At t=maxIter: n = (N-i)*WORK_UNIT  (first chare heaviest).
// At t=maxIter/2: all chares equal  (perfectly balanced midpoint).
static const int WORK_UNIT = 500000;

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  double t0;
public:
  Main(CkArgMsg* msg) {
    numWorkers = (msg->argc > 1) ? atoi(msg->argv[1]) : 16;
    maxIter    = (msg->argc > 2) ? atoi(msg->argv[2]) : 20;
    lbPeriod   = (msg->argc > 3) ? atoi(msg->argv[3]) : 5;
    delete msg;
    mainProxy  = thisProxy;
    CkPrintf("synth: %d workers  maxIter=%d  lbPeriod=%d  PEs=%d\n",
             numWorkers, maxIter, lbPeriod, CkNumPes());
    t0 = CkWallTimer();
    workerArray = CProxy_Worker::ckNew(numWorkers);
    workerArray.start();
  }

  void iterDone() {
    workerArray.nextIter();
  }

  void allDone() {
    CkPrintf("Done in %.3f s\n", CkWallTimer() - t0);
    CkExit();
  }
};

// ── Worker ───────────────────────────────────────────────────────────────────

class Worker : public CBase_Worker {
  int iteration;
  long long workResult;   // stores Fibonacci result; prevents dead-code elimination

  void doWork() {
    int N = numWorkers;
    int t = iteration;
    int n = ((maxIter - t) * (thisIndex + 1) + t * (N - thisIndex)) * WORK_UNIT / maxIter;
    long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
      long long c = a + b; a = b; b = c;
    }
    workResult = b;
  }

public:
  Worker_SDAG_CODE

  Worker() : iteration(0) {
    usesAtSync = true;
  }

  Worker(CkMigrateMessage* m) {}

  // Called by the LB runtime after migration; bridge to the SDAG when-clause.
  void ResumeFromSync() {
    thisProxy[thisIndex].resumeFromSync();
  }

  void pup(PUP::er& p) {
    p | iteration;
    p | workResult;
  }
};

#include "synth.def.h"
