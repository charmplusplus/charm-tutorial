#include "tmethod.decl.h"

/*readonly*/ int numWorkers;
/*readonly*/ int maxIter;
/*readonly*/ int lbPeriod;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Worker workerArray;

// ── Message ──────────────────────────────────────────────────────────────────

struct ValMsg : public CMessage_ValMsg {
  double value;
};

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  double t0;
public:
  Main(CkArgMsg* m) {
    numWorkers = (m->argc > 1) ? atoi(m->argv[1]) : 8;
    maxIter    = (m->argc > 2) ? atoi(m->argv[2]) : 20;
    lbPeriod   = (m->argc > 3) ? atoi(m->argv[3]) : 5;
    delete m;
    mainProxy  = thisProxy;
    CkPrintf("tmethod: %d workers  maxIter=%d  lbPeriod=%d  PEs=%d\n",
             numWorkers, maxIter, lbPeriod, CkNumPes());
    t0 = CkWallTimer();
    workerArray = CProxy_Worker::ckNew(numWorkers);
    workerArray.start();
  }

  void allDone() {
    CkPrintf("Done in %.3f s\n", CkWallTimer() - t0);
    CkExit();
  }
};

// ── Worker ───────────────────────────────────────────────────────────────────

class Worker : public CBase_Worker {
  double value;
  int    phase;

public:
  Worker_SDAG_CODE

  Worker() : value(thisIndex + 1.0), phase(0) {
    usesAtSync = true;
  }

  Worker(CkMigrateMessage* m) {}

  // Called by a remote threaded method passing a future: fulfil the future.
  void sendValue(CkFuture replyTo) {
    ValMsg* reply = new ValMsg;
    reply->value  = value;
    CkSendToFuture(replyTo, reply);
  }

  // [sync] variant: called directly from a threaded context; return the message.
  ValMsg* syncGetValue() {
    ValMsg* reply = new ValMsg;
    reply->value  = value;
    return reply;
  }

  // [threaded] phase: runs iterations [s, e) using CkFuture for each step,
  // then signals the SDAG that the thread has finished.
  void runPhase(int s, int e) {
    int right = (thisIndex + 1) % numWorkers;
    for (int iter = s; iter < e; iter++) {
      CkFuture f = CkCreateFuture();
      workerArray[right].sendValue(f);
      ValMsg* m = (ValMsg*)CkWaitFuture(f);
      value = (value + m->value) * 0.5;
      delete m;
    }
    // Thread is about to exit; signal the SDAG so it can call AtSync safely.
    thisProxy[thisIndex].phaseDone();
  }

  void ResumeFromSync() {
    thisProxy[thisIndex].resumeFromSync();
  }

  void pup(PUP::er& p) {
    p | value;
    p | phase;
  }
};

#include "tmethod.def.h"
