#include <cstdlib>
#include <cstring>
#include "stencil_lb.decl.h"

/*readonly*/ int gridSize;
/*readonly*/ int tileSize;
/*readonly*/ int numTiles;
/*readonly*/ int maxIter;
/*readonly*/ int lbPeriod;
/*readonly*/ CProxy_Main mainProxy;

#define NORTH 0
#define SOUTH 1
#define WEST  2
#define EAST  3

static int wrap(int i, int n) { return (i + n) % n; }

// ── Main ─────────────────────────────────────────────────────────────────────

class Main : public CBase_Main {
  CProxy_Tile grid;
  double t0;
public:
  Main(CkArgMsg* m) {
    gridSize = (m->argc > 1) ? atoi(m->argv[1]) : 64;
    tileSize = (m->argc > 2) ? atoi(m->argv[2]) : 8;
    maxIter  = (m->argc > 3) ? atoi(m->argv[3]) : 100;
    lbPeriod = (m->argc > 4) ? atoi(m->argv[4]) : 999;
    delete m;

    if (gridSize % tileSize != 0) CkAbort("gridSize must be divisible by tileSize");
    numTiles  = gridSize / tileSize;
    mainProxy = thisProxy;

    CkPrintf("stencil_lb: grid=%dx%d  tiles=%dx%d  maxIter=%d  lbPeriod=%d  PEs=%d\n",
             gridSize, gridSize, numTiles, numTiles, maxIter, lbPeriod, CkNumPes());

    t0   = CkWallTimer();
    grid = CProxy_Tile::ckNew(numTiles, numTiles);
    grid.run();
  }

  void iterDone() { grid.nextIter(); }

  void allDone() {
    CkPrintf("Done in %.3f s\n", CkWallTimer() - t0);
    CkExit();
  }
};

// ── Tile ─────────────────────────────────────────────────────────────────────

class Tile : public CBase_Tile {
  float *cur;       // current field values — live at AtSync
  float *nxt;       // double-buffer scratch — overwritten before read, not PUPped
  float *scratch;   // column-copy scratch for non-contiguous ghost sends
  int    iter;
  int    ghostCount; // counts incoming recvGhost calls; always 0 at AtSync

  int sz()              { return (tileSize + 2) * (tileSize + 2); }
  int idx(int r, int c) { return r * (tileSize + 2) + c; }

  void sendGhosts() {
    int tx = thisIndex.x, ty = thisIndex.y;
    // Tag = the side from the *receiver's* perspective.
    thisProxy(tx, wrap(ty-1, numTiles)).recvGhost(SOUTH, tileSize, &cur[idx(1, 1)]);
    thisProxy(tx, wrap(ty+1, numTiles)).recvGhost(NORTH, tileSize, &cur[idx(tileSize, 1)]);
    for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r, 1)];
    thisProxy(wrap(tx-1, numTiles), ty).recvGhost(EAST, tileSize, scratch);
    for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r, tileSize)];
    thisProxy(wrap(tx+1, numTiles), ty).recvGhost(WEST, tileSize, scratch);
  }

  void doCalc() {
    for (int r = 1; r <= tileSize; r++)
      for (int c = 1; c <= tileSize; c++)
        nxt[idx(r,c)] = 0.25f * (cur[idx(r-1,c)] + cur[idx(r+1,c)] +
                                  cur[idx(r,c-1)] + cur[idx(r,c+1)]);
    float *tmp = cur;  cur = nxt;  nxt = tmp;
  }

public:
  Tile_SDAG_CODE

  Tile() : iter(0), ghostCount(0) {
    cur     = new float[sz()];
    nxt     = new float[sz()];
    scratch = new float[tileSize];
    for (int i = 0; i < sz(); i++) cur[i] = 0.0f;
    for (int r = 1; r <= tileSize; r++)
      for (int c = 1; c <= tileSize; c++)
        cur[idx(r,c)] = (float)(thisIndex.x * numTiles + thisIndex.y) /
                        (float)(numTiles * numTiles);
    usesAtSync = true;
  }

  Tile(CkMigrateMessage* m) : cur(nullptr), nxt(nullptr), scratch(nullptr),
                               iter(0), ghostCount(0) {}

  ~Tile() {
    delete[] cur;
    delete[] nxt;
    delete[] scratch;
  }

  void recvGhost(int side, int n, float *data) {
    switch (side) {
      case NORTH: memcpy(&cur[idx(0,          1)], data, n * sizeof(float)); break;
      case SOUTH: memcpy(&cur[idx(tileSize+1,  1)], data, n * sizeof(float)); break;
      case WEST:  for (int r = 1; r <= n; r++) cur[idx(r, 0)]          = data[r-1]; break;
      case EAST:  for (int r = 1; r <= n; r++) cur[idx(r, tileSize+1)] = data[r-1]; break;
    }
    if (++ghostCount == 4) {
      ghostCount = 0;
      thisProxy[thisIndex].ghostsDone();
    }
  }

  void ResumeFromSync() {
    thisProxy[thisIndex].resumeFromSync();
  }

  void pup(PUP::er& p) {
    p | iter;
    if (p.isUnpacking()) {
      cur     = new float[sz()];
      nxt     = new float[sz()];   // allocate but do not pack
      scratch = new float[tileSize];
    }
    PUParray(p, cur, sz());
    // nxt is double-buffer scratch: doCalc() writes it completely before reading.
    // Not live at AtSync — skipping it halves the migration data volume.
    // ghostCount is always 0 at AtSync time; no need to pack it.
  }
};

#include "stencil_lb.def.h"
