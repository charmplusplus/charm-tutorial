#include "fib_thr.decl.h"

// Compute sequentially below this threshold to avoid chare explosion.
static const int THRESHOLD = 10;

static long seqFib(int n) {
    if (n <= 1) return n;
    long a = 0, b = 1;
    for (int i = 2; i <= n; i++) { long c = a + b; a = b; b = c; }
    return b;
}

struct LongMsg : public CMessage_LongMsg {
    long value;
};

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  int    n;
  double t0;
public:
  Main(CkArgMsg* m) {
    n = (m->argc > 1) ? atoi(m->argv[1]) : 20;
    delete m;
    CkPrintf("fib_thr: fib(%d)  PEs=%d\n", n, CkNumPes());
    t0 = CkWallTimer();
    thisProxy.run();
  }

  // [threaded]: allowed to block at CkWaitFuture
  void run() {
    CkFuture f = CkCreateFuture();
    CProxy_Fib::ckNew(n, f);
    LongMsg* m = (LongMsg*)CkWaitFuture(f);
    CkPrintf("fib(%d) = %ld  (%.3f s)\n", n, m->value, CkWallTimer() - t0);
    delete m;
    CkExit();
  }
};

// ── Fib ──────────────────────────────────────────────────────────────────────

class Fib : public CBase_Fib {
  int     n;
  CkFuture parentFut;
public:
  // Regular constructor: store arguments, then queue the threaded compute().
  Fib(int n_, CkFuture f) : n(n_), parentFut(f) {
    thisProxy.compute();
  }

  // [threaded]: runs in its own thread, free to block at CkWaitFuture.
  void compute() {
    long result;
    if (n <= THRESHOLD) {
      result = seqFib(n);
    } else {
      // Spawn both sub-problems concurrently.
      // While this thread blocks at CkWaitFuture, the scheduler runs the
      // two sub-chares — they execute in parallel across available PEs.
      CkFuture f1 = CkCreateFuture();
      CkFuture f2 = CkCreateFuture();
      CProxy_Fib::ckNew(n - 1, f1);
      CProxy_Fib::ckNew(n - 2, f2);
      LongMsg* m1 = (LongMsg*)CkWaitFuture(f1);
      LongMsg* m2 = (LongMsg*)CkWaitFuture(f2);
      result = m1->value + m2->value;
      delete m1;
      delete m2;
    }
    LongMsg* reply = new LongMsg;
    reply->value = result;
    CkSendToFuture(parentFut, reply);
  }
};

#include "fib_thr.def.h"
