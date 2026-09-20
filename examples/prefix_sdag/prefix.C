#include "prefix.decl.h"
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
        prefixArray.start();
    }
    void done() { CkExit(); }
};

class Prefix : public CBase_Prefix {
    int value, dist;
public:
    Prefix_SDAG_CODE

    Prefix() : value(thisIndex + 1), dist(0) {
        srand(thisIndex * 1009 + 7);
    }
    Prefix(CkMigrateMessage* m) {}
};

#include "prefix.def.h"
