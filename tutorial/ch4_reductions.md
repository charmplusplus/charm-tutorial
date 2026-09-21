# Chapter 4: Reductions and Broadcasts

Chapter 3 addressed one element at a time. `arr(i).method()` sends a message to element `i`,
and that is enough to build a ring, a neighbor exchange, or any other point-to-point pattern.

Two operations involve the collection as a whole:

- A **broadcast** invokes an entry method on *every* element of an array.
- A **reduction** combines a value from *every* element into a single result, delivered to
  one place.

They are duals, and most iterative programs alternate between them: broadcast the current
state to everyone, let them compute, reduce the results back, decide what to do next.

---

## Broadcast

A broadcast is an entry method invocation on the array proxy with **no index**:

```cpp
pointsArray.assign(2*K, centroids);   // runs on every element
```

Compare with `pointsArray(3).assign(...)`, which runs on element 3 only. Dropping the index
is the entire difference in syntax.

The semantics are what you would expect from Chapter 2, applied to every element at once:

- It is **asynchronous**. The call returns immediately; the elements run `assign` later.
- The elements run it **independently and in no particular order**, on whatever PEs they
  occupy. Two elements may be executing it at the same moment.
- There is **no completion signal**. The broadcast tells you nothing about when the
  elements finish, or whether they have started.

That last point is why broadcasts and reductions travel together. The broadcast pushes work
out; the reduction is how the program learns it is done.

---

## Readonly Variables

Before the reduction itself, a small piece of machinery that every non-trivial program uses.

Array elements usually need configuration values: the problem size, a convergence
threshold, a proxy to the main chare so they can send results back. Chapter 3 passed these
through the constructor — `Ring(CProxy_Main mp, int rs, int rID)` — which works, but does
not scale. Values needed by every chare in the program end up threaded through every
constructor and stored in every element.

A **readonly** variable is declared in the `.ci` file and defined as a global in the `.C`
file:

```cpp
// kmeans.ci
readonly int K;                   // number of clusters
readonly int numPoints;           // total points M
readonly int numChares;           // number of Points chares N
readonly CProxy_Main mainProxy;   // so Points can address Main for reductions
```

```cpp
// kmeans.C
/*readonly*/ int K;
/*readonly*/ int numPoints;
/*readonly*/ int numChares;
/*readonly*/ CProxy_Main mainProxy;
```

The `/*readonly*/` comment is convention, not syntax — it marks the definitions for a human
reader. The main chare assigns them in its constructor:

```cpp
numPoints = atoi(m->argv[1]);
numChares = atoi(m->argv[2]);
K         = atoi(m->argv[3]);
mainProxy = thisProxy;
```

The runtime then propagates them to every PE **before any other entry method runs anywhere**
in the program. That guarantee is what makes them usable: an element constructed later, on
any PE, can read them without asking anyone.

The effect on the element is visible in its constructor, which now takes no arguments at all:

```cpp
Points() {
  int base = numPoints / numChares;
  myNumPoints = (thisIndex < numChares - 1) ? base : numPoints - base * (numChares - 1);
  // ...
}
```

Each element works out its own share of the data from the readonly values and its own
`thisIndex`. Nothing had to be sent to it.

Two rules come with the mechanism. Assign readonly variables **only in the main chare
constructor** — they are read-only everywhere else, and for the whole run after that. And
note that this is not a license for global variables generally: a mutable global in a
Charm++ program is not legal, because chares on the same PE would share it and chares on
different PEs would not, which is never the behavior anyone wants.

---

## A Reduction

`examples/reduction` is the smallest program that does one. Each element of an array
contributes its own index, and the sum arrives at the main chare:

```cpp
// reduction.ci
mainmodule reduction {

  mainchare Main {
    entry Main(CkArgMsg *m);
    entry [reductiontarget] void printResult(int result);
  };

  array [1D] Elem {
    entry Elem(CProxy_Main mainProxy);
  };

};
```

```cpp
// reduction.C
class Main : public CBase_Main {
public:
  Main(CkArgMsg *m) {
    int numElems = m->argc > 1 ? atoi(m->argv[1]) : 10;
    CkPrintf("reduction: %d elements, expected sum = %d\n",
             numElems, numElems*(numElems-1)/2);
    delete m;
    CProxy_Elem::ckNew(thisProxy, numElems);
  }

  void printResult(int result) {
    CkPrintf("result = %d\n", result);
    CkExit();
  }
};

class Elem : public CBase_Elem {
public:
  Elem(CProxy_Main mainProxy) {
    int value = thisIndex;
    CkCallback cb(CkReductionTarget(Main, printResult), mainProxy);
    contribute(sizeof(int), &value, CkReduction::sum_int, cb);
  }
};
```

```
$ ./charmrun ++local +p3 ./reduction 8
reduction: 8 elements, expected sum = 28
result = 28
```

Three pieces make that work.

**`contribute`** is called by each element. Its arguments are the size of the contribution
**in bytes**, a pointer to the data, the reducer to combine with, and a callback saying
where the result goes:

```cpp
contribute(sizeof(int), &value, CkReduction::sum_int, cb);
```

**`[reductiontarget]`** marks the receiving entry method in the `.ci` file. A reduction
target receives the combined value as an ordinary typed argument — `void printResult(int
result)` — with no message to unpack.

**`CkReductionTarget`** builds the callback that names it:

```cpp
CkCallback cb(CkReductionTarget(Main, printResult), mainProxy);
```

`CkReductionTarget(Class, method)` identifies the entry method, and the proxy says which
chare to deliver to.

### What the runtime actually does

The reduction is **not** a gather. The runtime combines contributions up a spanning tree
across the PEs: each PE combines the contributions of the elements it holds, then combines
with its children in the tree, and only one value per PE climbs to the next level. The cost
is logarithmic in the number of processors, and no single chare ever receives *n* messages.

Two consequences follow from the tree:

- **The combining order is unspecified.** The reduction operation must be associative and
  commutative, or the answer depends on element placement and the number of PEs.
- **Every element must contribute.** The reduction completes when all of them have. If one
  element does not call `contribute`, nothing happens — no error, no timeout, just a program
  that never finishes. An element with nothing to add must still contribute an identity
  value.

Compare this with the counting pattern from Chapter 2, where each of K chares sent a result
back to the main chare, which decremented a counter and combined the values itself. That is
K messages arriving at one chare and a serial combine. The reduction replaces it with a
tree, and replaces the counter with the runtime's own completion detection.

### Built-in reducers

| Reducer | Combines |
|---|---|
| `CkReduction::sum_int`, `sum_long`, `sum_float`, `sum_double` | sum |
| `CkReduction::product_int`, `product_double`, … | product |
| `CkReduction::max_int`, `max_float`, `max_double`, … | maximum |
| `CkReduction::min_int`, `min_float`, `min_double`, … | minimum |
| `CkReduction::logical_and`, `logical_or` | logical combine |
| `CkReduction::nop` | nothing — the reduction is used purely as a completion signal |

`CkReduction::nop` deserves a note: a reduction with no data still tells the target that
every element has reached that point. That is the standard way to detect that a phase is
complete.

---

## Multiple Reductions in Flight

A program is not limited to one reduction at a time. `examples/ring_reduction` adds a
reduction to the ring from Chapter 3: every element contributes `tripsLeft` each time the
token passes through it, so one reduction completes per trip around the ring.

The only change to the element is three lines:

```cpp
void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE) {
  CkPrintf("Ring[%d](%d): tripsLeft=%d from [%d](%d)\n",
           thisIndex, CkMyPe(), tripsLeft, fromIndex, fromPE);

  // Each element contributes tripsLeft into this trip's sum reduction
  CkCallback cb(CkReductionTarget(Main, reductionDone), mainProxy);
  contribute(sizeof(int), &tripsLeft, CkReduction::sum_int, cb);

  // ... pass the token on, exactly as before ...
}
```

and the main chare counts the completions instead of exiting on the first:

```cpp
void reductionDone(int sum) {
  CkPrintf("Reduction result: %d\n", sum);
  if (++reductionsDone == tripCount) CkExit();
}
```

```
$ ./charmrun ++local +p3 ./ring_reduction 5 2
Ring+Reduction: size=5 trips=2 PEs=3
Ring[2](1): tripsLeft=2 from [-1](-1)
Ring[3](1): tripsLeft=2 from [2](1)
Ring[4](2): tripsLeft=2 from [3](1)
Ring[0](0): tripsLeft=2 from [4](2)
Ring[1](0): tripsLeft=2 from [0](0)
Reduction result: 10
Ring[2](1): tripsLeft=1 from [1](0)
...
Reduction result: 5
```

Five elements contributing `tripsLeft = 2` sum to 10, then five contributing 1 sum to 5.

The interesting part is what the runtime had to do. Contributions to the first reduction and
to the second are in flight at the same time — element 2 has already contributed to the
second trip's reduction while element 1 is still working on the first. The runtime keeps
them apart by **contribution order**: the *n*th `contribute` from every element belongs to
the *n*th reduction. Nothing in the message says which reduction it is for.

That gives the rule for using several reductions: **every element must take part in every
reduction, in the same order.** An element that skips one, or performs two in the opposite
order from its siblings, silently corrupts the results — its contribution is combined into
the wrong reduction.

---

## Vector Reductions

A contribution does not have to be a single value. An element can contribute an array, and
the reducer combines elementwise:

```cpp
CkCallback cbCounts(CkReductionTarget(Main, updateCounts), mainProxy);
contribute(localK * sizeof(int), localCounts, CkReduction::sum_int, cbCounts);
```

The target receives the length and a pointer, declared in the `.ci` file with the same
array-parameter syntax as Chapter 2:

```cpp
entry [reductiontarget] void updateCounts(int n, int counts[n]);
```

```cpp
void updateCounts(int n, int *incoming) {
  memcpy(counts, incoming, n * sizeof(int));
  // ...
}
```

`sum_int` over a vector of length *k* produces a vector of length *k*, each position summed
independently across all elements. This is how a program reduces a histogram, a per-category
total, or any other fixed-size collection of values in one operation rather than *k*.

---

## Putting It Together: K-means

`examples/kmeans` uses all of it. M points are spread over N chares; the main chare holds K
cluster centroids and improves them by iteration.

Each round is one broadcast and two reductions:

```cpp
// Main: start a round by sending the current centroids to everyone
pointsArray.assign(2*K, centroids);
```

```cpp
// Points::assign — classify my own points against the broadcast centroids
void assign(int K2, double *centroids) {
  int localK = K2 / 2;
  int    *localCounts = new int[localK]();
  double *localCoords = new double[K2]();

  for (int i = 0; i < myNumPoints; i++) {
    // ... find the nearest centroid to point i ...
    localCounts[best]++;
    localCoords[2*best]   += px;
    localCoords[2*best+1] += py;
  }

  CkCallback cbCounts(CkReductionTarget(Main, updateCounts), mainProxy);
  contribute(localK * sizeof(int), localCounts, CkReduction::sum_int, cbCounts);

  CkCallback cbCoords(CkReductionTarget(Main, updateCoords), mainProxy);
  contribute(K2 * sizeof(double), localCoords, CkReduction::sum_double, cbCoords);
}
```

Each element reports how many of its points fell into each cluster, and the sum of their
coordinates. Summed across all elements, those two vectors are exactly what is needed to
place the new centroids: total coordinates divided by total count, per cluster.

The two reductions complete independently, in no guaranteed order, so the main chare counts
them:

```cpp
void updateCounts(int n, int *incoming) {
  memcpy(counts, incoming, n * sizeof(int));
  if (++reductionsReceived == 2) recomputeCentroids();
}

void updateCoords(int n, double *incoming) {
  memcpy(coordSums, incoming, n * sizeof(double));
  if (++reductionsReceived == 2) recomputeCentroids();
}
```

`recomputeCentroids` divides, measures how far the centroids moved, and either exits or
broadcasts the new centroids to start another round:

```cpp
if (maxChange < THRESHOLD) {
  // ... print the centroids ...
  CkExit();
} else {
  pointsArray.assign(2*K, centroids);
}
```

```
$ ./charmrun ++local +p4 ./kmeans 10000 8 4
K-means: M=10000 N=8 K=4 PEs=4
Iteration 1: maxChange=0.213005
Iteration 2: maxChange=0.050871
...
Iteration 10: maxChange=0.000718
Converged after 10 iterations. Centroids:
  cluster 0: (0.2475, 0.7528) count=2549
  cluster 1: (0.7462, 0.2570) count=2554
```

This is the **iterate-to-convergence** pattern, and it is worth naming because so many
parallel programs have this shape:

1. The main chare broadcasts the current global state.
2. Every element computes on its own data and contributes a summary.
3. A reduction delivers the combined summary to one place.
4. That one place tests the termination condition and either stops or starts another round.

The termination test happens in exactly one place, on a value that is already global. No
element decides for itself whether the computation is finished, and no element needs to see
any other element's data.

---

## Pitfalls

1. **A missing contributor hangs the program silently.** Every element must call
   `contribute`, including elements with nothing to say. There is no error message — the
   reduction simply never completes.

2. **`contribute` takes a byte count.** `contribute(sizeof(int), &v, ...)`, and
   `contribute(k * sizeof(int), vec, ...)` for a vector. Passing a count of elements
   corrupts the reduction.

3. **Reductions are matched by order, not by name.** If different elements perform their
   reductions in different orders, contributions land in the wrong reduction. The results
   will be wrong without any warning.

4. **The reducer must be associative and commutative.** The spanning tree combines in an
   unspecified order that depends on placement and PE count.

5. **A broadcast tells you nothing about completion.** If the program needs to know that
   every element has finished the broadcast work, the elements must contribute to a
   reduction — `CkReduction::nop` if there is no data to combine.

6. **Assign readonly variables only in the main chare constructor.** Writing to one later
   changes it on that PE alone, which is a data-dependent bug of the worst kind.

---

## Exercises

**1. Mean and variance.** Give each element of a 1D array a vector of random doubles.
Compute the mean and the variance of all the values across the whole array using reductions.

Start with the one-pass version: a single vector reduction carrying `[count, Σx, Σx²]`, from
which mean and variance follow directly. Note that a contribution is one array of one type,
so the count travels as a double along with the sums.

Then test it. Generate the values as `uniform[0,1) + offset` and run with offset 0, then
10³, then 10⁶, then 10⁸. The variance is the same in every case — the offset shifts the data
without spreading it. Watch what your program reports. At some offset the answer starts to
drift, and at some larger offset it goes **negative**, which a variance cannot be.

Explain where the digits went, then fix it. The fix reuses the structure of this chapter:
reduce something, broadcast it back, reduce again.

**2. Maximum and where it is.** Find the largest value held by any element, *and* the index
of the element holding it. Try it first with `CkReduction::max_double` alone and see what
goes wrong: the maximum arrives with no indication of where it came from, and there is no
built-in reducer that carries an index alongside a value.

Solve it with the concepts you have — reduce the maximum, broadcast it back, and have the
elements that match report themselves through a second reduction. Two things to get right.
Make the answer deterministic when two elements hold the same maximum. And think about what
the matching element compares: it is testing a floating-point value for exact equality, which
is usually a mistake. Why is it safe here, and what would have to change about the first
reduction to make it unsafe?

If you generate the per-element values with `srand(some function of thisIndex)` followed by
`rand()`, print them all before you start. You may find the maximum is always at the same
end of the array, for reasons that have nothing to do with Charm++.

**3. Rewrite the primes program.** Chapter 2's primality program counted completions in the
main chare with a callback per chare. Rewrite it so the chares form an array, each testing a
block of the range, and report the total through a single reduction. Compare the two versions
for the amount of bookkeeping code in the main chare — that is where the difference shows up,
not in the running time on a laptop with two worker PEs.

---

## Summary

- A **broadcast** is an entry method invocation on the array proxy with no index. It reaches
  every element, asynchronously, in no particular order, and reports nothing about
  completion.
- **Readonly variables** are declared in the `.ci`, defined as globals in the `.C`, and
  assigned in the main chare constructor. The runtime makes them available on every PE
  before any other entry method runs, so elements can read configuration without being told.
- A **reduction** combines a value from every element up a spanning tree and delivers the
  result to a `[reductiontarget]` entry method named by `CkReductionTarget`. Every element
  must contribute; the combine must be associative and commutative.
- Contributions are **matched by order**, so several reductions can be in flight at once as
  long as every element performs them in the same sequence.
- A contribution may be a **vector**; the reducer combines elementwise and the target
  receives `(int n, T data[n])`.
- The **iterate-to-convergence** pattern — broadcast, compute, reduce, test, repeat — covers
  a large fraction of real parallel applications.

Chapters 3 and 4 have built everything out of entry methods that return immediately, with
each chare's logic spread across the methods that happen to be invoked on it. The next
chapter takes up the other kinds of array index — multidimensional, sparse, and non-integer
— and the five-point stencil that makes two-dimensional indexing worth having.
