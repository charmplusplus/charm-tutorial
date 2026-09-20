# Chapter 5: Load Balancing

## The Problem: Load Imbalance

In a parallel program with a global barrier — a reduction, a synchronization step, an
exchange of boundary values — **every PE waits for the slowest one**. If one PE has
twice as much work as the others, half the machine sits idle at each barrier.

This is the load imbalance problem. It is not a pathological case: any simulation where
particles cluster, reactions concentrate, or mesh refinement is uneven will produce it.

Charm++ addresses this with an automated **load balancing (LB)** framework. The
runtime instruments every entry method, measuring how much time each chare consumes. At
a user-chosen synchronization point, a load balancing strategy reads those measurements,
decides which chares to move to which PEs, and migrates them. The chares restart on
their new PEs and subsequent iterations run with better balance.

---

## A Synthetic Workload

To demonstrate the mechanism cleanly, this chapter uses a deliberately imbalanced
synthetic example rather than a physics simulation. The example is in
`examples/synth_lb/`.

There are `numWorkers` chares in a 1D array. Each chare runs `maxIter` iterations.
In each iteration, **worker `i` computes the `(i+1)*WORK_UNIT`-th Fibonacci number**
iteratively. Because each Fibonacci step depends on the previous, the loop cannot be
optimized away or vectorized. Worker 0 does `WORK_UNIT` steps; worker 15 does
`16 * WORK_UNIT` steps — sixteen times as much.

The default element placement in Charm++ is **block**: for a 1D array of 16 elements
on 4 PEs, elements 0–3 go to PE 0, 4–7 to PE 1, and so on. With 4 PEs the PE
assignments are:

- PE 0: workers 0–3 → 1+2+3+4 = **10 units** of work per iteration
- PE 1: workers 4–7 → 5+6+7+8 = **26 units**
- PE 2: workers 8–11 → 9+10+11+12 = **42 units**
- PE 3: workers 12–15 → 13+14+15+16 = **58 units**

The mean is 34 units per PE. PE 3 is carrying 71% more than average, making it the
bottleneck at every barrier. Load balancing should bring the bottleneck close to the mean.

---

## Base Version: No Load Balancing

The `.ci` file for the base version is straightforward. The SDAG `start()` entry
method runs a `for` loop, calling `doWork()` each iteration and contributing to a
reduction when finished:

```cpp
// synth.ci (no LB)
mainmodule synth {
  readonly int numWorkers;
  readonly int maxIter;
  readonly CProxy_Main mainProxy;
  readonly CProxy_Worker workerArray;

  mainchare Main {
    entry Main(CkArgMsg*);
    entry [reductiontarget] void allDone();
  };

  array [1D] Worker {
    entry Worker();
    entry void start() {
      for (iteration = 0; iteration < maxIter; iteration++) {
        serial { doWork(); }
      }
      serial {
        CkCallback cb(CkReductionTarget(Main, allDone), mainProxy);
        contribute(cb);
      }
    };
  };
};
```

The `.C` file holds the work kernel and the chare class:

```cpp
// synth.C (no LB)
static const int WORK_UNIT = 500000;

class Worker : public CBase_Worker {
  int iteration;
  long long workResult;   // prevents dead-code elimination

  void doWork() {
    long long a = 0, b = 1;
    int n = (thisIndex + 1) * WORK_UNIT;
    for (int i = 0; i < n; i++) {
      long long c = a + b;  a = b;  b = c;
    }
    workResult = b;
  }

public:
  Worker_SDAG_CODE
  Worker() : iteration(0), workResult(0) {}
  Worker(CkMigrateMessage* m) {}
};
```

---

## Adding Load Balancing

Four additions are needed: two in the `.ci` file and two in the `.C` file.

### 1. Declare the LB period and the resume entry method

In the `.ci` file, add a readonly for the LB period and a new entry method that the
SDAG will wait on after each load balancing step:

```cpp
readonly int lbPeriod;
// ...
array [1D] Worker {
  // ...
  entry void resumeFromSync();   // ← new
```

### 2. Add the AtSync/resume pattern to the SDAG loop

Inside the `start()` for loop, after `doWork()`, check whether this is a load
balancing iteration and call `AtSync()` if so:

```cpp
entry void start() {
  for (iteration = 0; iteration < maxIter; iteration++) {
    serial { doWork(); }
    if ((iteration + 1) % lbPeriod == 0) {
      serial { AtSync(); }
      when resumeFromSync() serial { }
    }
  }
  serial {
    CkCallback cb(CkReductionTarget(Main, allDone), mainProxy);
    contribute(cb);
  }
};
```

`AtSync()` tells the runtime "I have reached a safe migration point." The runtime
collects `AtSync()` calls from every chare. When all have called it, the LB strategy
runs, migrates chares as needed, and then sends `resumeFromSync()` to each chare. The
SDAG `when resumeFromSync()` receives that message and the for loop continues.

### 3. Enable migration and add `pup()`

In the constructor, set `usesAtSync = true` to register this chare for LB
participation. Add a `pup()` method so that migrated chares carry their state:

```cpp
Worker() : iteration(0), workResult(0) {
  usesAtSync = true;         // ← new
}

void pup(PUP::er& p) {       // ← new
  p | iteration;
  p | workResult;
}
```

Every member variable must appear in `pup()`. The SDAG framework automatically handles
its own continuation state (the `Worker_SDAG_CODE` macro generates the required pup
support); you only need to list your own members.

### 4. Bridge `ResumeFromSync` to the SDAG entry method

The Charm++ LB runtime calls the virtual function `ResumeFromSync()` (inherited from
`CBase_Worker`) when a chare is ready to continue after migration. The SDAG, however,
is waiting on the entry method `resumeFromSync()`. These are two different things;
without a bridge the chare hangs forever.

The fix is a one-line override:

```cpp
void ResumeFromSync() {
  thisProxy[thisIndex].resumeFromSync();
}
```

This sends the chare a message to itself, which the `when resumeFromSync()` in the
SDAG picks up and uses to advance the for loop.

---

## Build-Time and Runtime Options

### Including an LB strategy at build time

A load balancing strategy must be compiled into the binary. Link with
`-module EveryLB` to include all built-in strategies:

```makefile
synth: $(OBJS)
    $(CHARMC) -language charm++ -module EveryLB -o synth $(OBJS)
```

To include only specific strategies (smaller binary):

```
-module GreedyLB
-module RefineLB
-module GreedyRefineLB
```

### Selecting a strategy at runtime

Pass `+balancer <StrategyName>` on the command line:

```
./charmrun +p4 ./synth 16 100 10 +balancer GreedyLB
./charmrun +p4 ./synth 16 100 10 +balancer GreedyRefineLB
```

Three commonly used strategies:

| Strategy | What it does |
|---|---|
| `GreedyLB` | Assigns chares to PEs greedily: heaviest chare to lightest PE. Global moves; may migrate many chares. |
| `RefineLB` | Makes only small adjustments — migrates a chare only if moving it reduces the maximum PE load. Low overhead; conservative. |
| `GreedyRefineLB` | Runs Greedy followed by Refine. Good default for most applications. |

---

## Results: Static Workload

With the static formula (`n = (i+1) * WORK_UNIT`), the imbalance is fixed: PE 3 always
carries 58 units while the mean is 34. Running on 4 PEs:

```
# No load balancing
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 999
Done in 5.85 s

# GreedyRefineLB, rebalance every 10 iterations (10 calls)
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 10 +balancer GreedyRefineLB
Done in 5.40 s

# GreedyRefineLB, rebalance every 5 iterations (20 calls)
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 5 +balancer GreedyRefineLB
Done in 4.74 s
```

A 19% improvement at lbPeriod=5. The pattern here: for a **static** workload the first
LB call achieves near-optimal balance and every subsequent call adds overhead with no
benefit. Calling more frequently (lbPeriod=5) triggers the first rebalance sooner — at
iteration 4 rather than 9 — so fewer iterations run at full imbalance before the state
is corrected. Beyond a certain frequency the added overhead outweighs the earlier
first-call benefit.

---

## Dynamic Workloads: Why Periodic LB Matters

In real applications load rarely stays constant. A reaction front propagates, particles
cluster and disperse, adaptive refinement concentrates computation in a moving region.
The initial LB placement becomes stale as the simulation evolves, and a single
correction early in the run is not enough.

To model this, `synth.C` uses a **two-term dynamic formula** for the work in each
iteration:

```
n(i, t) = [(maxIter - t) * (i + 1)  +  t * (numWorkers - i)] * WORK_UNIT / maxIter
```

- At `t = 0`: reduces to `(i+1) * WORK_UNIT` — the familiar static formula, PE 3 heaviest.
- At `t = maxIter`: reduces to `(numWorkers - i) * WORK_UNIT` — reversed, PE 0 heaviest.
- At `t = maxIter/2`: all workers carry the same load — a perfectly balanced midpoint.

The load drifts slowly from high-index workers toward low-index workers over the run.
An LB call made early moves heavy chares off PE 3; by mid-run those chares have become
light and PE 0's chares have grown heavy — the migration is now backward. Without
periodic rebalancing the placement becomes actively harmful.

Because the load shifts across iterations, a global **per-iteration barrier** is added:
all workers reduce to Main, which broadcasts `nextIter()`, before proceeding. This
makes the per-iteration imbalance visible in wall time.

```
# No load balancing
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 999
Done in 4.88 s

# GreedyRefineLB, lbPeriod=50 (2 calls — too infrequent)
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 50 +balancer GreedyRefineLB
Done in 4.32 s

# GreedyRefineLB, lbPeriod=20 (5 calls)
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 20 +balancer GreedyRefineLB
Done in 4.07 s

# GreedyRefineLB, lbPeriod=10 (10 calls)  ← best
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 10 +balancer GreedyRefineLB
Done in 3.54 s

# GreedyRefineLB, lbPeriod=5 (20 calls — overhead dominates)
./charmrun ++local +p4 ++ppn 2 ./synth 16 100 5 +balancer GreedyRefineLB
Done in 3.91 s
```

A 27% improvement at `lbPeriod=10`. The optimal period is now in the middle of the
range: too infrequent and the stale placement accumulates error as load drifts; too
frequent and overhead outpaces the per-call benefit.

---

## LB Period: Choosing the Right Frequency

The table below summarizes both workload types:

| Workload | Best period | Why |
|---|---|---|
| Static | Small (early first call) | One call fixes it; call it early |
| Dynamic (slow drift) | Moderate (tracks drift) | Match frequency to load's rate of change |
| Dynamic (fast changes) | Smaller, but LB cost grows | May need autobalancer (see below) |

The right period is application-specific and is often determined empirically. A useful
heuristic: start at a period of 5–10% of total iteration count and sweep from there.

---

## Autobalancer: Adaptive Period Selection

Charm++ includes a built-in **autobalancer** that measures the cost and benefit of each
load balancing call and skips it when it would not help. With the autobalancer you can
safely call `AtSync()` every iteration (or every few iterations); the runtime decides
whether to actually migrate chares — most calls simply return `resumeFromSync()` with
no migration.

> **Note:** The autobalancer exists in recent Charm++ versions and works well for
> applications with smoothly varying load. Its use will be covered here once we have
> verified its behavior on our target platform.

---

## PUP for Complex Chares: Only Migrate Live State

Chapter 4 introduced the PUP framework. When a chare with heap-allocated members
migrates, the `pup()` method must correctly pack everything needed to **resume
computation correctly** on the new PE. But it should **not** pack state that will be
overwritten before it is read — migrating dead data wastes network bandwidth.

The rule: a variable is **live** at AtSync time if it will be **read before it is
written** after migration. A variable is **dead** if it will be completely overwritten
before any read.

### Double-buffer arrays in a Jacobi stencil

The `examples/stencil_lb/` example adds load balancing to a 2D Jacobi stencil. Each
tile holds two arrays:

```cpp
float *cur;   // current field values — live at AtSync
float *nxt;   // double-buffer scratch — overwritten before read
```

Each step:
1. Read ghost borders from neighbors into `cur`
2. Compute Jacobi average from `cur`, write result into `nxt`
3. Swap: `float *tmp = cur;  cur = nxt;  nxt = tmp;`

`AtSync()` is called after step 3 (at the end of each LB-period iteration, after the
reduction barrier). At that moment:

- `cur` holds the freshly computed field. The next step 2 will **read** `cur`, so it
  **must** be PUPped.
- `nxt` holds the previous step's values. The next step 2 will **overwrite** `nxt`
  completely before reading it, so it **need not** be PUPped.

```cpp
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
```

`nxt` is still allocated during unpack — `doCalc()` needs somewhere to write — but
its initial contents on the new PE are irrelevant. The savings are one full grid tile
per chare: for large tiles this can cut migration traffic in half.

The same logic applies to any write-before-read buffer: communication scratch arrays,
cached values that will be recomputed from `cur`, temporary residuals.

### What about ghost border values in `cur`?

At AtSync time the ghost borders of `cur` (the rows and columns filled by neighbor
messages in the previous iteration) are stale — they will be refreshed by the next
ghost exchange before `doCalc()` reads them. Strictly speaking they do not need to be
packed either. The example packs the full `(tileSize+2)²` array for simplicity; a
bandwidth-critical production code would pack only the interior `tileSize²` elements.

---

## Summary

Load balancing in Charm++ is opt-in and instrumentation-based. The runtime measures
chare costs automatically; you choose when to rebalance and which strategy to use.

The five additions to an existing chare:

1. `usesAtSync = true` in the constructor.
2. `AtSync()` call at each desired rebalance point (within a SDAG `serial` block).
3. `entry void resumeFromSync()` in the `.ci` file; `when resumeFromSync()` in the SDAG.
4. A `ResumeFromSync()` override that bridges the LB virtual call to the SDAG entry method.
5. A `pup()` method listing every **live** member variable; a `Chare(CkMigrateMessage*)` constructor.

At build time: `-module EveryLB` (or a named strategy). At runtime: `+balancer StrategyName`.

For the PUP method: pack only state that will be **read before written** after
migration. Double buffers, scratch arrays, and cached values that will be recomputed
do not need to cross the network.

The PUP chapter already covered `pup()` in detail. The migration constructor does
nothing — it exists so Charm++ has a constructor to call when reconstructing the chare
on the destination PE before unpacking its state.
