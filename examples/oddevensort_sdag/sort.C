#include "sort.decl.h"
#include <algorithm>
#include <unistd.h>

/*readonly*/ int numElems;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Sorter sortArr;

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        numElems = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
        delete msg;
        mainProxy = thisProxy;
        sortArr = CProxy_Sorter::ckNew(numElems);
        sortArr.start();
    }
    void allDone(int numCorrect) {
        CkPrintf("%d/%d elements correct\n", numCorrect, numElems);
        CkExit();
    }
};

class Sorter : public CBase_Sorter {
    int value, round;
    bool amLeft, amRight;
public:
    Sorter_SDAG_CODE

    Sorter() : value(numElems - thisIndex), round(0), amLeft(false), amRight(false) {
        srand(thisIndex * 1009 + 7);
    }
    Sorter(CkMigrateMessage* m) {}
};

#include "sort.def.h"
