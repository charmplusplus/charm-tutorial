#include "prefix.decl.h"
#include <map>
#include <unistd.h>

/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Prefix prefixArray;
/*readonly*/ int numElements;

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        numElements = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
        delete msg;
        mainProxy = thisProxy;
        prefixArray = CProxy_Prefix::ckNew(numElements);
    }
    void done() { CkExit(); }
};

// Each element i holds initial value (i+1), so expected prefix sum at i is (i+1)(i+2)/2.
// Recursive doubling: in each step with distance d, element i sends its current
// partial sum to element i+d, then waits to receive from element i-d (if valid).
//
// Bug in original: passValue had no step identifier, so out-of-order arrival from
// different senders caused elements to apply receives in the wrong step order,
// corrupting the values they forward to downstream elements.
//
// Fix: pass stepDist with each passValue so the receiver can buffer future-step
// messages and apply them only when the algorithm reaches that step.
// sentAtCurrentDist prevents advance() from re-sending when called multiple times.

class Prefix : public CBase_Prefix {
    int value, distance;
    bool sentAtCurrentDist, isDone;
    std::map<int,int> recvBuf;

    void advance() {
        while (distance < numElements) {
            // Send current partial sum to right neighbor — but only once per step.
            if (!sentAtCurrentDist) {
                if (thisIndex + distance < numElements) {
                    usleep(rand() % 5001);  // 0-5ms blocking delay (stresses ordering)
                    thisProxy[thisIndex + distance].passValue(distance, value);
                }
                sentAtCurrentDist = true;
            }
            // Advance if no receive needed, or if receive already buffered.
            if (thisIndex - distance < 0) {
                distance *= 2;
                sentAtCurrentDist = false;
            } else if (recvBuf.count(distance)) {
                value += recvBuf[distance];
                recvBuf.erase(distance);
                distance *= 2;
                sentAtCurrentDist = false;
            } else {
                return;  // waiting for passValue at this distance
            }
        }
        isDone = true;
        int expected = (thisIndex + 1) * (thisIndex + 2) / 2;
        CkPrintf("Prefix[%d] = %d  (expected %d)  %s\n",
                 thisIndex, value, expected,
                 (value == expected) ? "OK" : "WRONG");
        CkCallback cb(CkReductionTarget(Main, done), mainProxy);
        contribute(0, NULL, CkReduction::nop, cb);
    }

public:
    Prefix() : value(thisIndex + 1), distance(1), sentAtCurrentDist(false), isDone(false) {
        srand(thisIndex * 1009 + 7);  // distinct seed per chare
        advance();
    }

    Prefix(CkMigrateMessage* m) {}

    void step() {}  // kept for .ci compatibility; advance() drives the algorithm

    void passValue(int stepDist, int val) {
        if (isDone) return;
        recvBuf[stepDist] = val;
        if (recvBuf.count(distance))
            advance();
    }
};

#include "prefix.def.h"
