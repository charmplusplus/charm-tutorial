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
static const double MOVE_MAX = 1.5;   // max displacement per step per axis

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  int statCount;
public:
  Main(CkMigrateMessage* m) {}
  Main(CkArgMsg* msg) {
    k        = (msg->argc > 1) ? atoi(msg->argv[1]) : 16;
    nPerCell = (msg->argc > 2) ? atoi(msg->argv[2]) : 100;
    maxIter  = (msg->argc > 3) ? atoi(msg->argv[3]) : 100;
    delete msg;
    statCount = 0;
    mainProxy = thisProxy;
    CkPrintf("Particles: k=%d  nPerCell=%d  maxIter=%d  totalParticles=%d  PEs=%d\n",
             k, nPerCell, maxIter, nPerCell * k * k, CkNumPes());
    cellArray = CProxy_Cell::ckNew(k, k);
    cellArray.start();
  }

  void reportMax(int maxCount) {
    statCount++;
    CkPrintf("  iter %3d: max particles/cell = %d\n", statCount * 10, maxCount);
  }

  void allDone() {
    CkPrintf("Done.\n");
    CkExit();
  }
};

// ── Cell ─────────────────────────────────────────────────────────────────────

class Cell : public CBase_Cell {
  std::vector<double> px, py;  // particle coordinates
  int numNeighbors;            // count of valid neighbor cells (3, 5, or 8)
  int iteration;               // SDAG for-loop variable — must be a member
  int neighborsDone;           // SDAG inner for-loop variable — must be a member
  unsigned seed;               // per-chare RNG state (avoids global rand() mutex)

  // Move all particles, then send outgoing ones to their destination cells.
  // Always sends to every valid neighbor (even if 0 particles go there),
  // so each cell knows exactly numNeighbors messages arrive per iteration.
  void sendParticles() {
    int ix = thisIndex.x, iy = thisIndex.y;
    double cs = WORLD / k;

    // Compute new positions; revert if outside global boundary.
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

    // Bucket particles by destination cell offset (di, dj) in [-1,1]^2.
    // Index: outX[di+1][dj+1] holds x-coords of particles going to (ix+di, iy+dj).
    std::vector<double> outX[3][3], outY[3][3];
    std::vector<double> stayX, stayY;
    for (int i = 0; i < np; i++) {
      int ni = std::min((int)(newX[i] / cs), k - 1);
      int nj = std::min((int)(newY[i] / cs), k - 1);
      int di = ni - ix, dj = nj - iy;
      // Clamp to [-1,1]: with MOVE_MAX < cs, particles reach at most one cell away.
      di = std::max(-1, std::min(1, di));
      dj = std::max(-1, std::min(1, dj));
      if (di == 0 && dj == 0) {
        stayX.push_back(newX[i]);
        stayY.push_back(newY[i]);
      } else {
        outX[di+1][dj+1].push_back(newX[i]);
        outY[di+1][dj+1].push_back(newY[i]);
      }
    }

    // Send to every valid neighbor (pass a non-null pointer even for n=0).
    double placeholder = 0.0;
    for (int di = -1; di <= 1; di++) {
      for (int dj = -1; dj <= 1; dj++) {
        if (di == 0 && dj == 0) continue;
        int ni = ix + di, nj = iy + dj;
        if (ni < 0 || ni >= k || nj < 0 || nj >= k) continue;
        std::vector<double>& vx = outX[di+1][dj+1];
        std::vector<double>& vy = outY[di+1][dj+1];
        int n = (int)vx.size();
        double* xp = n > 0 ? vx.data() : &placeholder;
        double* yp = n > 0 ? vy.data() : &placeholder;
        cellArray(ni, nj).receiveParticles(iteration, n, xp, yp);
      }
    }

    // Replace particle list with the particles staying in this cell.
    px = std::move(stayX);
    py = std::move(stayY);
  }

public:
  Cell_SDAG_CODE

  Cell() : numNeighbors(0), iteration(0), neighborsDone(0),
           seed((unsigned)(thisIndex.x * 1009 + thisIndex.y * 997 + 7)) {
    int ix = thisIndex.x, iy = thisIndex.y;

    // Count valid neighbors (depends on position in the k×k grid).
    for (int di = -1; di <= 1; di++)
      for (int dj = -1; dj <= 1; dj++) {
        if (di == 0 && dj == 0) continue;
        if (ix+di >= 0 && ix+di < k && iy+dj >= 0 && iy+dj < k)
          numNeighbors++;
      }

    // Initialize particles uniformly within this cell's bounding box.
    double cs = WORLD / k;
    double xlo = ix * cs, ylo = iy * cs;
    px.reserve(nPerCell);
    py.reserve(nPerCell);
    for (int i = 0; i < nPerCell; i++) {
      px.push_back(xlo + (rand_r(&seed) / (double)RAND_MAX) * cs);
      py.push_back(ylo + (rand_r(&seed) / (double)RAND_MAX) * cs);
    }
  }

  Cell(CkMigrateMessage* m) {}
};

#include "particles.def.h"
