#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cfloat>
#include "kmeans.decl.h"

/*readonly*/ int K;
/*readonly*/ int numPoints;
/*readonly*/ int numChares;
/*readonly*/ CProxy_Main mainProxy;

static const double THRESHOLD = 0.001;

class Main : public CBase_Main {
  CProxy_Points pointsArray;
  double *centroids;   // flat: [x0,y0, x1,y1, ...]
  int    *counts;
  double *coordSums;
  int reductionsReceived;
  int iteration;

public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    if (m->argc < 4) {
      CkPrintf("Usage: %s <M> <N> <K>\n", m->argv[0]);
      CkAbort("Need M N K");
    }
    numPoints = atoi(m->argv[1]);
    numChares = atoi(m->argv[2]);
    K         = atoi(m->argv[3]);
    delete m;

    CkPrintf("K-means: M=%d N=%d K=%d PEs=%d\n", numPoints, numChares, K, CkNumPes());

    mainProxy  = thisProxy;
    centroids  = new double[2*K];
    counts     = new int[K];
    coordSums  = new double[2*K];
    reductionsReceived = 0;
    iteration  = 0;

    srand(42);
    for (int k = 0; k < 2*K; k++)
      centroids[k] = (double)rand() / RAND_MAX;

    pointsArray = CProxy_Points::ckNew(numChares);
    pointsArray.assign(2*K, centroids);
  }

  void updateCounts(int n, int *incoming) {
    memcpy(counts, incoming, n * sizeof(int));
    if (++reductionsReceived == 2) recomputeCentroids();
  }

  void updateCoords(int n, double *incoming) {
    memcpy(coordSums, incoming, n * sizeof(double));
    if (++reductionsReceived == 2) recomputeCentroids();
  }

  void recomputeCentroids() {
    reductionsReceived = 0;
    iteration++;

    double maxChange = 0.0;
    for (int k = 0; k < K; k++) {
      if (counts[k] == 0) continue;
      double newX = coordSums[2*k]   / counts[k];
      double newY = coordSums[2*k+1] / counts[k];
      double dx = newX - centroids[2*k];
      double dy = newY - centroids[2*k+1];
      maxChange = fmax(maxChange, fabs(dx));
      maxChange = fmax(maxChange, fabs(dy));
      centroids[2*k]   = newX;
      centroids[2*k+1] = newY;
    }

    CkPrintf("Iteration %d: maxChange=%.6f\n", iteration, maxChange);

    if (maxChange < THRESHOLD) {
      CkPrintf("Converged after %d iterations. Centroids:\n", iteration);
      for (int k = 0; k < K; k++)
        CkPrintf("  cluster %d: (%.4f, %.4f) count=%d\n",
                 k, centroids[2*k], centroids[2*k+1], counts[k]);
      CkExit();
    } else {
      pointsArray.assign(2*K, centroids);
    }
  }
};

class Points : public CBase_Points {
  int myNumPoints;
  double *pts;   // flat: [x0,y0, x1,y1, ...]

public:
  Points(CkMigrateMessage *m) {}
  Points() {
    // Distribute points: chare i gets numPoints/numChares, last chare gets remainder
    int base = numPoints / numChares;
    myNumPoints = (thisIndex < numChares - 1) ? base : numPoints - base * (numChares - 1);

    pts = new double[2 * myNumPoints];
    srand(thisIndex * 1234567);
    for (int i = 0; i < 2 * myNumPoints; i++)
      pts[i] = (double)rand() / RAND_MAX;
  }

  void assign(int K2, double *centroids) {
    int localK = K2 / 2;
    int    *localCounts = new int[localK]();
    double *localCoords = new double[K2]();

    for (int i = 0; i < myNumPoints; i++) {
      double px = pts[2*i], py = pts[2*i+1];
      int best = 0;
      double bestDist = DBL_MAX;
      for (int k = 0; k < localK; k++) {
        double dx = px - centroids[2*k];
        double dy = py - centroids[2*k+1];
        double d  = dx*dx + dy*dy;
        if (d < bestDist) { bestDist = d; best = k; }
      }
      localCounts[best]++;
      localCoords[2*best]   += px;
      localCoords[2*best+1] += py;
    }

    CkCallback cbCounts(CkReductionTarget(Main, updateCounts), mainProxy);
    contribute(localK * sizeof(int), localCounts, CkReduction::sum_int, cbCounts);

    CkCallback cbCoords(CkReductionTarget(Main, updateCoords), mainProxy);
    contribute(K2 * sizeof(double), localCoords, CkReduction::sum_double, cbCoords);

    delete[] localCounts;
    delete[] localCoords;
  }
};

#include "kmeans.def.h"
