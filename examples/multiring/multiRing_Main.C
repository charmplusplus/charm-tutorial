#include <cstdlib>
#include <ctime>
#include "multiRing_Main.decl.h"

class Main : public CBase_Main {
  int numRings;
  int finishedCount;

public:
  Main(CkMigrateMessage *m) {}

  Main(CkArgMsg *m) {
    int *ringSize = NULL, *tripCount = NULL;
    numRings = 0;
    finishedCount = 0;

    if (!processCommandLine(m, &numRings, &ringSize, &tripCount)) {
      printUsage(m->argv[0]);
      CkAbort("Bad command line");
    }
    delete m;

    CkPrintf("\"Array Ring (Multi)\" Program\n");
    CkPrintf("  numRings = %d, CkNumPes() = %d\n", numRings, CkNumPes());
    for (int i = 0; i < numRings; i++)
      CkPrintf("  Ring %d : ringSize = %d, tripCount = %d\n",
               i, ringSize[i], tripCount[i]);

    srand(time(NULL));

    // Create the rings. Each is a separate chare array; they never interact.
    for (int i = 0; i < numRings; i++) {
      CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize[i], i, ringSize[i]);
      ring(rand() % ringSize[i]).doSomething(ringSize[i], tripCount[i], -1, -1);
    }

    delete[] ringSize;
    delete[] tripCount;
  }

  // One ring has finished all of its trips. Exit once every ring has reported.
  void ringFinished() {
    if (++finishedCount >= numRings) CkExit();
  }

private:
  // Command line: <numRings> <ringSize_0> <tripCount_0> ... <ringSize_n-1> <tripCount_n-1>
  bool processCommandLine(CkArgMsg *m, int *nr, int **rs, int **tc) {
    if (m->argc < 2) return false;
    int n = atoi(m->argv[1]);
    if (n < 1 || m->argc != 2 + 2 * n) return false;

    *nr = n;
    *rs = new int[n];
    *tc = new int[n];
    for (int i = 0; i < n; i++) {
      (*rs)[i] = atoi(m->argv[2 + 2 * i]);
      (*tc)[i] = atoi(m->argv[3 + 2 * i]);
      if ((*rs)[i] < 1 || (*tc)[i] < 1) return false;
    }
    return true;
  }

  void printUsage(const char *appName) {
    CkPrintf("Usage: %s <numRings> <ringSize> <tripCount> [<ringSize> <tripCount> ...]\n",
             appName);
    CkPrintf("  e.g. %s 3 5 3 10 1 8 2\n", appName);
  }
};

#include "multiRing_Main.def.h"
