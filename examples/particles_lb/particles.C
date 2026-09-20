#include "particles.decl.h"
#include <vector>
#include <algorithm>
#include <cstdlib>

/*readonly*/ int k;
/*readonly*/ int nPerCell;
/*readonly*/ int maxIter;
/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Cell cellArray;

static const double WORLD    = 100.0;
static const double MOVE_MAX = 1.5;

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  int statCount;
  int expectedTotal;
  double t0;
public:
  Main(CkMigrateMessage* m) {}
  Main(CkArgMsg* msg) {
    k        = (msg->argc > 1) ? atoi(msg->argv[1]) : 16;
    nPerCell = (msg->argc > 2) ? atoi(msg->argv[2]) : 100;
    maxIter  = (msg->argc > 3) ? atoi(msg->argv[3]) : 100;
    delete msg;
    statCount = 0;
    mainProxy = thisProxy;

    // Non-uniform initial total: cell [i][j] starts with nPerCell*(1 + 5*(i+j)/k).
    // Integer division on (i+j)/k: result is 0 for i+j < k, 1 for i+j in [k, 2k).
    // Worst cell (k-1,k-1): 5*(2k-2)/k = 5*1 = 5 → 6× the minimum cell.
    // Use the same integer arithmetic as the Cell constructor to get the exact count.
    expectedTotal = 0;
    for (int i = 0; i < k; i++)
      for (int j = 0; j < k; j++)
        expectedTotal += nPerCell + nPerCell * (5 * (i + j) / k);

    CkPrintf("Particles (LB): k=%d  nPerCell=%d  maxIter=%d  expectedTotal=%d  PEs=%d\n",
             k, nPerCell, maxIter, expectedTotal, CkNumPes());
    CkPrintf("  Cell [0][0]   starts with %d particles\n", nPerCell);
    CkPrintf("  Cell [%d][%d] starts with %d particles\n",
             k-1, k-1, nPerCell + nPerCell * (5 * 2*(k-1) / k));

    t0 = CkWallTimer();
    cellArray = CProxy_Cell::ckNew(k, k);
    cellArray.start();
  }

  void reportMax(int maxCount) {
    statCount++;
    CkPrintf("  iter %3d: max particles/cell = %d\n", statCount * 5, maxCount);
  }

  void allDone(int totalParticles) {
    double elapsed = CkWallTimer() - t0;
    CkPrintf("Done in %.3f s.  Final total = %d  (expected %d)  %s\n",
             elapsed, totalParticles, expectedTotal,
             (totalParticles == expectedTotal) ? "OK" : "CONSERVATION ERROR");
    CkExit();
  }
};

// ── Cell ─────────────────────────────────────────────────────────────────────

class Cell : public CBase_Cell {
  std::vector<double> px, py;
  int numNeighbors;
  int iteration;
  int neighborsDone;
  unsigned seed;

  void sendParticles() {
    int ix = thisIndex.x, iy = thisIndex.y;
    double cs = WORLD / k;

    int np = (int)px.size();

    std::vector<double> newX(np), newY(np);
    for (int i = 0; i < np; i++) {
      double nx = px[i] + (rand_r(&seed) / (double)RAND_MAX - 0.5) * 2.0 * MOVE_MAX;
      double ny = py[i] + (rand_r(&seed) / (double)RAND_MAX - 0.5) * 2.0 * MOVE_MAX;
      if (nx < 0.0 || nx >= WORLD || ny < 0.0 || ny >= WORLD)
        { nx = px[i]; ny = py[i]; }
      newX[i] = nx;
      newY[i] = ny;
    }

    std::vector<double> outX[3][3], outY[3][3];
    std::vector<double> stayX, stayY;
    for (int i = 0; i < np; i++) {
      int ni = std::min((int)(newX[i] / cs), k - 1);
      int nj = std::min((int)(newY[i] / cs), k - 1);
      int di = std::max(-1, std::min(1, ni - ix));
      int dj = std::max(-1, std::min(1, nj - iy));
      if (di == 0 && dj == 0) {
        stayX.push_back(newX[i]);
        stayY.push_back(newY[i]);
      } else {
        outX[di+1][dj+1].push_back(newX[i]);
        outY[di+1][dj+1].push_back(newY[i]);
      }
    }

    double placeholder = 0.0;
    for (int di = -1; di <= 1; di++) {
      for (int dj = -1; dj <= 1; dj++) {
        if (di == 0 && dj == 0) continue;
        int ni = ix + di, nj = iy + dj;
        if (ni < 0 || ni >= k || nj < 0 || nj >= k) continue;
        std::vector<double>& vx = outX[di+1][dj+1];
        std::vector<double>& vy = outY[di+1][dj+1];
        int n = (int)vx.size();
        cellArray(ni, nj).receiveParticles(iteration, n,
          n > 0 ? vx.data() : &placeholder,
          n > 0 ? vy.data() : &placeholder);
      }
    }

    px = std::move(stayX);
    py = std::move(stayY);
  }

public:
  Cell_SDAG_CODE

  // PUP: required for migration during load balancing.
  void pup(PUP::er& p) {
    p | px;
    p | py;
    p | numNeighbors;
    p | iteration;
    p | neighborsDone;
    p | seed;
  }

  Cell() : numNeighbors(0), iteration(0), neighborsDone(0),
           seed((unsigned)(thisIndex.x * 1009 + thisIndex.y * 997 + 7)) {
    usesAtSync = true;  // required to call AtSync()
    int ix = thisIndex.x, iy = thisIndex.y;

    for (int di = -1; di <= 1; di++)
      for (int dj = -1; dj <= 1; dj++) {
        if (di == 0 && dj == 0) continue;
        if (ix+di >= 0 && ix+di < k && iy+dj >= 0 && iy+dj < k)
          numNeighbors++;
      }

    // Non-uniform initial distribution: nPerCell*(1 + 5*(ix+iy)/k).
    // Integer division on (ix+iy)/k: 0 when ix+iy < k, 1 when ix+iy in [k,2k).
    // Worst cell (k-1,k-1): 5*(2k-2)/k = 5 → starts with 6× the lightest cell.
    int myN = nPerCell + nPerCell * (5 * (ix + iy) / k);
    double cs = WORLD / k;
    double xlo = ix * cs, ylo = iy * cs;
    px.reserve(myN);
    py.reserve(myN);
    for (int i = 0; i < myN; i++) {
      px.push_back(xlo + (rand_r(&seed) / (double)RAND_MAX) * cs);
      py.push_back(ylo + (rand_r(&seed) / (double)RAND_MAX) * cs);
    }
  }

  Cell(CkMigrateMessage* m) {}
};

#include "particles.def.h"
