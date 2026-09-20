#include <cstdlib>
#include "primes.decl.h"

static int isPrime(const long number) {
  if (number <= 1) return 0;
  for (long i = 2; i < number; i++) {
    if (0 == number % i) return 0;
  }
  return 1;
}

struct Result {
  long number;
  bool prime;
};

class Main : public CBase_Main {
  int K;
  int count;
  Result *results;

public:
  Main(CkArgMsg *m) {
    if (m->argc < 2) {
      CkPrintf("Usage: %s <K>\n", m->argv[0]);
      CkAbort("Need K as command-line argument");
    }
    K = atoi(m->argv[1]);
    delete m;

    results = new Result[K];
    count = 0;

    for (int i = 0; i < K; i++) {
      long number = rand();
      results[i].number = number;
      results[i].prime = false;
      CProxy_CheckPrimality::ckNew(i, number, thisProxy);
    }
  }

  void done(int index, int result) {
    results[index].prime = (result != 0);
    if (++count == K) {
      for (int i = 0; i < K; i++)
        CkPrintf("%ld: %s\n", results[i].number, results[i].prime ? "prime" : "not prime");
      delete[] results;
      CkExit();
    }
  }
};

class CheckPrimality : public CBase_CheckPrimality {
public:
  CheckPrimality(int index, long number, CProxy_Main mainProxy) {
    mainProxy.done(index, isPrime(number));
  }
};

#include "primes.def.h"
