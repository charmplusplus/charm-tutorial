#include <cstdlib>
#include <cmath>
#include <cstring>
#include "stencil2d.decl.h"

/*readonly*/ int          gridSize;
/*readonly*/ int          tileSize;
/*readonly*/ int          numTiles;
/*readonly*/ float        epsilon;
/*readonly*/ CProxy_Main  mainProxy;

// Side indices for recvGhost
#define NORTH 0
#define SOUTH 1
#define WEST  2
#define EAST  3

// Wrapping neighbor index
static int wrap(int i, int n) { return (i + n) % n; }

// ─── Main ────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  CProxy_Tile grid;
  int step;
public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    gridSize = (m->argc > 1) ? atoi(m->argv[1]) : 64;
    tileSize = (m->argc > 2) ? atoi(m->argv[2]) : 8;
    epsilon  = (m->argc > 3) ? atof(m->argv[3]) : 0.01f;
    delete m;

    if (gridSize % tileSize != 0) CkAbort("gridSize must be divisible by tileSize");
    numTiles  = gridSize / tileSize;
    mainProxy = thisProxy;
    step = 0;

    CkPrintf("Stencil2D: grid=%dx%d tiles=%dx%d tileSize=%d epsilon=%.4f PEs=%d\n",
             gridSize, gridSize, numTiles, numTiles, tileSize, epsilon, CkNumPes());

    grid = CProxy_Tile::ckNew(numTiles, numTiles);
    grid.startStep();
  }

  void checkConvergence(float maxDiff) {
    step++;
    CkPrintf("Step %d: maxDiff=%.6f\n", step, maxDiff);
    if (maxDiff <= epsilon) {
      CkPrintf("Converged after %d steps.\n", step);
      CkExit();
    } else {
      grid.startStep();
    }
  }
};

// ─── Tile ────────────────────────────────────────────────────────────────────

class Tile : public CBase_Tile {
  // Data layout: (tileSize+2) x (tileSize+2), row-major.
  // Row 0 = north ghost, row tileSize+1 = south ghost.
  // Col 0 = west ghost,  col tileSize+1 = east ghost.
  float *cur;   // current values
  float *next;  // scratch for next step
  float *scratch; // column-copy scratch for ghost send
  int eventCount;

  // Index into the (tileSize+2) x (tileSize+2) array
  int idx(int row, int col) { return row * (tileSize + 2) + col; }

public:
  Tile(CkMigrateMessage *m) {}

  Tile() : eventCount(0) {
    int sz = (tileSize + 2) * (tileSize + 2);
    cur     = new float[sz];
    next    = new float[sz];
    scratch = new float[tileSize];

    // Initialize interior randomly; ghost borders start at 0
    srand(thisIndex.x * 1000 + thisIndex.y);
    for (int i = 0; i < sz; i++) cur[i] = 0.0f;
    for (int r = 1; r <= tileSize; r++)
      for (int c = 1; c <= tileSize; c++)
        cur[idx(r,c)] = (float)rand() / RAND_MAX;

    // Pin one element as a heat source so the field has a fixed reference
    if (thisIndex.x == 0 && thisIndex.y == 0)
      cur[idx(1,1)] = 1.0f;
  }

  // ── Ghost exchange ─────────────────────────────────────────────────────────

  void startStep() {
    int tx = thisIndex.x, ty = thisIndex.y;

    // North: send row 1 to tile above (its south ghost)
    thisProxy(tx, wrap(ty-1, numTiles)).recvGhost(SOUTH, tileSize, &cur[idx(1,1)]);
    // South: send row tileSize to tile below (its north ghost)
    thisProxy(tx, wrap(ty+1, numTiles)).recvGhost(NORTH, tileSize, &cur[idx(tileSize,1)]);

    // West: send col 1 to tile left (its east ghost) — not contiguous, copy first
    for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r,1)];
    thisProxy(wrap(tx-1, numTiles), ty).recvGhost(EAST, tileSize, scratch);

    // East: send col tileSize to tile right (its west ghost)
    for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r,tileSize)];
    thisProxy(wrap(tx+1, numTiles), ty).recvGhost(WEST, tileSize, scratch);

    countEvent();
  }

  void recvGhost(int side, int n, float *data) {
    // Copy incoming data into the appropriate ghost row/column
    switch (side) {
      case NORTH:
        memcpy(&cur[idx(0, 1)], data, n * sizeof(float));
        break;
      case SOUTH:
        memcpy(&cur[idx(tileSize+1, 1)], data, n * sizeof(float));
        break;
      case WEST:
        for (int r = 1; r <= n; r++) cur[idx(r, 0)] = data[r-1];
        break;
      case EAST:
        for (int r = 1; r <= n; r++) cur[idx(r, tileSize+1)] = data[r-1];
        break;
    }
    countEvent();
  }

  // ── Local computation ──────────────────────────────────────────────────────

  void countEvent() {
    if (++eventCount < 5) return;  // need startStep + 4 ghost receives
    eventCount = 0;
    doCalc();
  }

  void doCalc() {
    float maxDiff = 0.0f;

    for (int r = 1; r <= tileSize; r++) {
      for (int c = 1; c <= tileSize; c++) {
        // Skip the pinned heat-source element
        if (thisIndex.x == 0 && thisIndex.y == 0 && r == 1 && c == 1) continue;

        float newVal = 0.25f * (cur[idx(r-1,c)] + cur[idx(r+1,c)] +
                                cur[idx(r,c-1)] + cur[idx(r,c+1)]);
        next[idx(r,c)] = newVal;
        float diff = fabsf(newVal - cur[idx(r,c)]);
        if (diff > maxDiff) maxDiff = diff;
      }
    }

    // Swap buffers
    float *tmp = cur; cur = next; next = tmp;

    // Restore pinned element
    if (thisIndex.x == 0 && thisIndex.y == 0) cur[idx(1,1)] = 1.0f;

    // Contribute max diff to global reduction
    CkCallback cb(CkReductionTarget(Main, checkConvergence), mainProxy);
    contribute(sizeof(float), &maxDiff, CkReduction::max_float, cb);
  }
};

#include "stencil2d.def.h"
