#include "sort.decl.h"
#include <algorithm>
#include <unistd.h>

/*readonly*/ int numElems;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Sorter sortArr;

// Odd-even transposition sort: N chares, each holding one value.
//
// Round r, offset = r%2:
//   LEFT  (thisIndex%2 == offset, thisIndex+1 < N): send right, receive from right
//   RIGHT (thisIndex%2 != offset, thisIndex > 0):   send left,  receive from left
//   IDLE  (neither): skip
//
// Design: advance() sends for the current active round and returns.
// receiveVal() either applies directly (current round) or buffers (future round).
//
// Two buffer slots are needed, indexed by r%2 (round parity):
//   - Even-round messages always arrive from one neighbor direction.
//   - Odd-round messages always arrive from the other direction.
// Because message ordering is not guaranteed, both a future even-round message
// and a future odd-round message can be in flight simultaneously before the
// current round's reply arrives.

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        numElems = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
        delete msg;
        mainProxy = thisProxy;
        sortArr = CProxy_Sorter::ckNew(numElems);
    }

    void allDone(int numCorrect) {
        CkPrintf("%d/%d elements correct\n", numCorrect, numElems);
        CkExit();
    }
};

class Sorter : public CBase_Sorter {
    int value, round;
    int bufRound[2], bufVal[2];  // indexed by r%2; -1 means empty
    unsigned seed;

    void advance() {
        if (round >= numElems) {
            int expected = thisIndex + 1;
            CkPrintf("Sorter[%d] = %d  (expected %d)  %s\n",
                     thisIndex, value, expected,
                     (value == expected) ? "OK" : "WRONG");
            int correct = (value == expected) ? 1 : 0;
            CkCallback cb(CkReductionTarget(Main, allDone), mainProxy);
            contribute(sizeof(int), &correct, CkReduction::sum_int, cb);
            return;
        }
        int offset = round % 2;
        bool amLeft = ((thisIndex % 2) == offset) && (thisIndex + 1 < numElems);
        bool amRight= ((thisIndex % 2) != offset) && (thisIndex > 0);

        if (!amLeft && !amRight) {
            round++;
            advance();
            return;
        }

        usleep(rand_r(&seed) % 5001);
        if (amLeft) thisProxy[thisIndex + 1].receiveVal(value, round);
        else        thisProxy[thisIndex - 1].receiveVal(value, round);

        int slot = round % 2;
        if (bufRound[slot] == round) {
            int pv = bufVal[slot];  bufRound[slot] = -1;
            value = amLeft ? std::min(value, pv) : std::max(value, pv);
            round++;
            advance();
        }
    }

public:
    Sorter() : value(numElems - thisIndex), round(0),
               bufRound{-1, -1}, bufVal{0, 0},
               seed((unsigned)(thisIndex * 1009 + 7)) {
        advance();
    }

    Sorter(CkMigrateMessage* m) {}

    void receiveVal(int partnerVal, int r) {
        if (r == round) {
            bool amLeft = ((thisIndex % 2) == (r % 2));
            value = amLeft ? std::min(value, partnerVal) : std::max(value, partnerVal);
            round++;
            advance();
        } else {
            int slot = r % 2;
            bufRound[slot] = r;
            bufVal[slot]   = partnerVal;
        }
    }
};

#include "sort.def.h"
