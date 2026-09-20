# Chapter 5: Array Indices

Every array so far has been a dense, one-dimensional, integer-indexed collection: elements
0 through n−1, all of them present. That is the common case, and it is what `array [1D]`
gives you.

The index is more general than that. A chare array can be indexed by integers in **one
through six dimensions**, and the collection can be **sparse** — only the indices that a
program actually needs have to exist. Indices can also be bit vectors or strings, for
collections whose natural name is not a tuple of integers.

The index is not decoration. It is how a program expresses which element talks to which, so
choosing it well is a design decision. A two-dimensional simulation indexed `(i, j)` gets
its neighbor communication for free — `(i±1, j)` and `(i, j±1)` — where a 1D array would
force the program to compute the same relationships by hand.

---

## Multidimensional Arrays

The dimensionality goes in the declaration:

```cpp
// stencil2d.ci
array [2D] Tile {
  entry Tile();
  entry void startStep();
  entry void recvGhost(int side, int n, float data[n]);
};
```

Creation takes one extent per dimension, and addressing takes one index per dimension:

```cpp
CProxy_Tile grid = CProxy_Tile::ckNew(numTiles, numTiles);   // numTiles x numTiles
grid(i, j).startStep();                                      // one element
grid.startStep();                                            // broadcast, as before
```

Inside an element, `thisIndex` is now a **struct** rather than an `int`:

```cpp
int tx = thisIndex.x, ty = thisIndex.y;
```

The member names depend on the dimensionality:

| Declaration | `thisIndex` members | Component type |
|---|---|---|
| `[1D]` | `thisIndex` is itself an `int` | `int` |
| `[2D]` | `.x`, `.y` | `int` |
| `[3D]` | `.x`, `.y`, `.z` | `int` |
| `[4D]` | `.w`, `.x`, `.y`, `.z` | `short int` |
| `[5D]` | `.v`, `.w`, `.x`, `.y`, `.z` | `short int` |
| `[6D]` | `.x1`, `.y1`, `.z1`, `.x2`, `.y2`, `.z2` | `short int` |

Note the component type for four dimensions and above: those indices are packed into
`short int` fields, so each component is limited to roughly ±32,000. One to three
dimensions use full `int` components.

---

## A Worked Example: The Five-Point Stencil

A five-point stencil repeatedly replaces every point of a 2D grid with the average of
itself and its four neighbors — the standard discretization of the heat equation, and the
shape of a great many scientific kernels.

The decomposition is the natural one: cut the grid into square **tiles** and give each tile
to one element of a 2D chare array. A 40×40 grid in tiles of 10×10 becomes a 4×4 array of
elements.

```cpp
numTiles = gridSize / tileSize;
grid = CProxy_Tile::ckNew(numTiles, numTiles);
grid.startStep();
```

### Ghosts

To update the points on its edge, a tile needs one row or column of values from each of its
four neighbors. Those borrowed values are called **ghosts**, and each tile stores them in a
halo around its own data — a `(tileSize+2)²` array whose interior is the tile's own points
and whose outer ring holds the neighbors' edges:

```cpp
int idx(int row, int col) { return row * (tileSize + 2) + col; }
```

Every step begins with each tile sending its four edges out:

```cpp
void startStep() {
  int tx = thisIndex.x, ty = thisIndex.y;

  // North: send row 1 to the tile above (it lands in that tile's south ghost)
  thisProxy(tx, wrap(ty-1, numTiles)).recvGhost(SOUTH, tileSize, &cur[idx(1,1)]);
  // South: send row tileSize to the tile below
  thisProxy(tx, wrap(ty+1, numTiles)).recvGhost(NORTH, tileSize, &cur[idx(tileSize,1)]);

  // West: send column 1 to the tile on the left — a column is not contiguous, so copy
  for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r,1)];
  thisProxy(wrap(tx-1, numTiles), ty).recvGhost(EAST, tileSize, scratch);

  // East: send column tileSize to the tile on the right
  for (int r = 1; r <= tileSize; r++) scratch[r-1] = cur[idx(r,tileSize)];
  thisProxy(wrap(tx+1, numTiles), ty).recvGhost(WEST, tileSize, scratch);

  countEvent();
}
```

This is where the index earns its place. `thisProxy(tx, wrap(ty-1, numTiles))` *is* the
statement "the tile above me", and `wrap` makes the grid periodic at the edges. No table
maps positions to proxies, and no element had to be told who its neighbors are.

Two details worth noticing. A row of the halo array is contiguous and can be sent directly
from `&cur[idx(1,1)]`; a column is strided, so it is copied into a scratch buffer first.
And the direction in the message is the direction *from the receiver's point of view* — the
tile above receives it as its `SOUTH` ghost.

### Knowing when the data has arrived

A tile can compute only after its own `startStep` has run and all four ghost messages have
arrived. They arrive in any order, and `recvGhost` is an ordinary entry method that runs
whenever a message shows up. So the tile counts events:

```cpp
void recvGhost(int side, int n, float *data) {
  // ... copy the data into the right ghost row or column ...
  countEvent();
}

void countEvent() {
  if (++eventCount < 5) return;   // need startStep + 4 ghost receives
  eventCount = 0;
  doCalc();
}
```

Five events: the broadcast that began the step, and four ghost arrivals. This is the
**always-send** protocol — every tile sends to all four neighbors every step, with no
conditionals, so every tile knows exactly how many messages to expect. Uniform counts are
what make the count meaningful.

This counting is the same bookkeeping pattern as the callback counting of Chapter 2, and it
is the price of writing a multi-step interaction out of entry methods that each run to
completion. Chapter 6 introduces Structured Dagger, which expresses the same logic as a
sequential program and makes the counter disappear.

### Driving the iteration

Once a tile has computed, it contributes the largest change it saw to a `max_float`
reduction; the main chare either stops or broadcasts the next step:

```cpp
CkCallback cb(CkReductionTarget(Main, checkConvergence), mainProxy);
contribute(sizeof(float), &maxDiff, CkReduction::max_float, cb);
```

```cpp
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
```

That is exactly the iterate-to-convergence pattern from Chapter 4, with a 2D array
underneath and a neighbor exchange inside each round.

```bash
cd examples/stencil2d
make
./charmrun ++local +p4 ./stencil2d 40 10 0.01
```

```
Stencil2D: grid=40x40 tiles=4x4 tileSize=10 epsilon=0.0100 PEs=4
Step 1: maxDiff=0.809902
Step 2: maxDiff=0.587029
Step 3: maxDiff=0.469623
...
```

---

## Sparse Arrays

Everything so far created every element between 0 and the extent. Many index spaces are far
too large for that, and mostly empty.

An array can be created with **no elements at all**, then filled in one index at a time:

```cpp
CProxy_Compute computeArray = CProxy_Compute::ckNew();   // empty, no extent
// ... for each index that should exist:
computeArray[CkArrayIndex6D(x1, y1, z1, x2, y2, z2)].insert();
// ...
computeArray.doneInserting();
```

Three pieces: `ckNew()` with no arguments creates an empty array; `insert()` on a specific
index creates that element; and `doneInserting()` tells the runtime that the population is
complete. `insert()` can optionally take a PE number as a placement hint.

Note the bracket form with an explicit `CkArrayIndex6D`. That is the general way to address
an element of a chare array. The `grid(i, j)` form used throughout this tutorial is
shorthand for `grid[CkArrayIndex2D(i, j)]`.

### Where this matters: LeanMD

[LeanMD](https://github.com/UIUC-PPL/leanmd) is a molecular dynamics mini-application, and
its decomposition is the standard illustration of why sparse arrays exist. It uses two
chare arrays:

```cpp
array [3D] Cell    { ... };   // one element per cell of 3D space
array [6D] Compute { ... };   // one element per *interacting pair* of cells
```

A `Cell` holds the atoms in one region of space — a dense 3D array, one element per region.
A `Compute` element indexed `(x1,y1,z1, x2,y2,z2)` computes the forces between the atoms of
cell `(x1,y1,z1)` and those of cell `(x2,y2,z2)`.

The index space of `Compute` is therefore *cells squared*. For a modest 20×20×20 cell grid
that is 8000² = 64 million possible pairs — while the pairs that actually matter are only
the neighboring ones, on the order of 8000 × 13. A dense 6D array is out of the question;
the program inserts only the pairs it needs:

```cpp
CkArrayIndex6D index(px1, py1, pz1, px2, py2, pz2);
computeArray[index].insert((++currPe) % CkNumPes());
```

This is worth studying as a decomposition, not just as syntax. Splitting force computation
into its own chare array — one element per pair, rather than per cell — creates far more
parallel work than there are cells, and lets the runtime balance it. LeanMD also uses
features not covered in this tutorial (custom array maps through `CkArrayOptions::setMap`,
and section proxies for multicast), so read it for the structure rather than line by line.

---

## Bit Vector and String Indices

An index does not have to be a tuple of integers at all. Charm++ supports indexing a chare
array by a bit vector or by a string, for collections whose elements are naturally named by
something else — a key, a path, a node identifier in a tree:

```cpp
// A string-indexed collection, addressed by name rather than by number
array [String] Record { ... };
```

The mechanism is the same underneath: `CkArrayIndex` is a bag of bytes, and the integer and
multidimensional forms are convenience wrappers over it. An element is still created with
`insert`, addressed with brackets, and migrated and load-balanced like any other.

These are rare compared to integer indices, and a program that wants key-based lookup more
often uses a 1D array plus a hash of the key — which is exactly what the distributed hash
table exercise does. They are mentioned here so that the shape of the mechanism is clear.

---

## Pitfalls

1. **`thisIndex` is a struct for multidimensional arrays.** `thisIndex.x` and `thisIndex.y`
   for 2D, not `thisIndex[0]`. Using it as a plain integer will not compile.

2. **Index components are `short int` for 4D and above.** Fine for cell grids, but not for
   indices in the millions.

3. **A sparse array needs `doneInserting()`.** Until it is called, the runtime does not know
   the collection is complete, and collective operations over it are not well defined.

4. **The always-send protocol depends on uniform counts.** Counting five events works
   because every tile sends to four neighbors every step. If some tiles skipped a send —
   for a non-periodic boundary, say — the expected count would differ per element and the
   count would have to be computed per element rather than hard-coded.

5. **A strided slice is not contiguous.** Sending a column of a 2D block means copying it
   into a buffer first; the entry-method array parameter packs whatever contiguous range it
   is given.

---

## Exercises

**1. Non-periodic boundaries.** The stencil wraps at the edges, which makes every tile have
exactly four neighbors. Change it so the grid has fixed boundaries: edge tiles have three
neighbors and corner tiles two, with the outer boundary held at a constant value. What does
`countEvent` have to become, and where does the expected count come from?

**2. Three dimensions.** Extend the stencil to a 3D grid with a seven-point stencil —
`array [3D]`, six neighbors, six ghost faces per step. A face of a 3D block is strided in
two ways rather than one; how much of the tile code has to change, and how much of the main
chare?

**3. A sparse array of your own.** Create a 2D chare array in which only the elements on the
diagonal and the first row exist, using `ckNew()`, `insert()` and `doneInserting()`. Have
each element print its index and PE. Confirm that a broadcast reaches exactly the elements
you inserted, and that a reduction over the array completes with only those elements
contributing.

---

## Summary

- A chare array can be indexed in **one through six dimensions**; the dimensionality is in
  the declaration, the extents are the arguments to `ckNew`, and `grid(i, j)` addresses an
  element.
- `thisIndex` is an `int` for 1D and a **struct** for higher dimensions, with `int`
  components up to 3D and `short int` components for 4D–6D.
- A multidimensional index lets a program name its neighbors directly, which is what makes
  the five-point stencil's ghost exchange a few lines of index arithmetic.
- The **always-send** protocol plus event counting is how a multi-step exchange is expressed
  with plain entry methods. Uniform message counts are what make the counting work.
- An array can be **sparse**: `ckNew()` with no extent, `insert()` per index, and
  `doneInserting()` when the population is complete. LeanMD's 6D `Compute` array over pairs
  of cells is the canonical case.
- Bit vector and string indices exist for collections whose natural name is not a tuple of
  integers.

Chapters 3 to 5 have built collections, collective operations, and index spaces — but every
chare's logic has been scattered across entry methods, with counters standing in for "wait
until these things have happened". The next chapter removes that scaffolding: **Structured
Dagger** lets a chare's lifecycle be written as a sequential program.
