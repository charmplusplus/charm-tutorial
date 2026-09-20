#include <cstdlib>
#include "primes_b.decl.h"

static int isPrime(const long number) {
  if (number <= 1) return 0;
  if (number == 2) return 1;
  if (number % 2 == 0) return 0;
  for (long i = 3; i * i <= number; i += 2) {
    if (0 == number % i) return 0;
  }
  return 1;
}

class Main : public CBase_Main {
  int K, M;
  int numChares;
  int count;
  long *numbers;
  bool *primeResults;
  double startTime;

public:
  Main(CkArgMsg *m) {
    if (m->argc < 3) {
      CkPrintf("Usage: %s <K> <M>\n", m->argv[0]);
      CkAbort("Need K and M as command-line arguments");
    }
    K = atoi(m->argv[1]);
    M = atoi(m->argv[2]);
    delete m;

    numbers = new long[K];
    primeResults = new bool[K];
    count = 0;
    numChares = (K + M - 1) / M;

    for (int i = 0; i < K; i++) {
      numbers[i] = rand();
      primeResults[i] = false;
    }

    startTime = CkWallTimer();

    for (int i = 0; i < numChares; i++) {
      int lo = i * M;
      int batchSize = (lo + M <= K) ? M : (K - lo);
      CProxy_CheckPrimality::ckNew(i, batchSize, numbers + lo, thisProxy);
    }
  }

  void done(int batchIdx, int batchSize, int *results) {
    int lo = batchIdx * M;
    for (int j = 0; j < batchSize; j++)
      primeResults[lo + j] = (results[j] != 0);

    if (++count == numChares) {
      CkPrintf("K=%d M=%d time=%.4f s\n", K, M, CkWallTimer() - startTime);
      // Uncomment to verify results:
      // for (int i = 0; i < K; i++)
      //   CkPrintf("%ld: %s\n", numbers[i], primeResults[i] ? "prime" : "not prime");
      delete[] numbers;
      delete[] primeResults;
      CkExit();
    }
  }
};

class CheckPrimality : public CBase_CheckPrimality {
public:
  CheckPrimality(int batchIdx, int batchSize, long *numbers, CProxy_Main mainProxy) {
    int *results = new int[batchSize];
    for (int i = 0; i < batchSize; i++)
      results[i] = isPrime(numbers[i]);
    mainProxy.done(batchIdx, batchSize, results);
    delete[] results;
  }
};

#include "primes_b.def.h"
