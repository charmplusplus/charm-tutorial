#include "dht.decl.h"
#include <cstdlib>
#include <map>

/*readonly*/ int numChares;
/*readonly*/ int keysPerChare;
/*readonly*/ int queriesPerChare;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_DHT dhtArray;

// Deterministic value for a key — used both when building the table and
// when verifying responses, so it never needs to be transmitted.
static inline int tableValue(int key) { return key * 13 + 7; }

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
public:
  Main(CkMigrateMessage* m) {}
  Main(CkArgMsg* msg) {
    numChares       = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
    keysPerChare    = (msg->argc > 2) ? atoi(msg->argv[2]) : 100;
    queriesPerChare = (msg->argc > 3) ? atoi(msg->argv[3]) : 20;
    delete msg;

    mainProxy = thisProxy;
    CkPrintf("DHT: %d chares  %d keys/chare  %d queries/chare  totalKeys=%d\n",
             numChares, keysPerChare, queriesPerChare,
             numChares * keysPerChare);

    dhtArray = CProxy_DHT::ckNew(numChares);
    dhtArray.start();
  }

  void allDone(int totalErrors) {
    int totalQueries = numChares * queriesPerChare;
    CkPrintf("Done. %d/%d queries verified correctly.  Errors: %d  %s\n",
             totalQueries - totalErrors, totalQueries, totalErrors,
             totalErrors == 0 ? "OK" : "WRONG");
    CkExit();
  }
};

// ── DHT ──────────────────────────────────────────────────────────────────────

class DHT : public CBase_DHT {
  std::map<int,int> table;   // this chare's slice of the key-value store
  int responsesReceived;     // SDAG for-loop variable — must be a member
  int errorCount;
  unsigned seed;

public:
  DHT_SDAG_CODE

  DHT() : responsesReceived(0), errorCount(0),
          seed((unsigned)(thisIndex * 1009 + 3)) {
    // Populate this chare's key range with deterministic values.
    int base = thisIndex * keysPerChare;
    for (int i = 0; i < keysPerChare; i++)
      table[base + i] = tableValue(base + i);
  }

  DHT(CkMigrateMessage* m) {}

  // Handle a lookup request from another chare.
  // This is a plain entry method — not part of SDAG — so it runs whenever
  // the scheduler dispatches it, even while this chare is suspended in its
  // "when response" loop waiting for its own replies.
  void request(int key, int callerIndex) {
    int value = table.count(key) ? table[key] : -1;
    dhtArray[callerIndex].response(key, value);
  }
};

#include "dht.def.h"
