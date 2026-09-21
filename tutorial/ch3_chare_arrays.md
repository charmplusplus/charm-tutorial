# Chapter 3: Chare Arrays

Chapter 2 created chares one at a time. `CProxy_Compute::ckNew(pi)` makes one chare and
hands back one proxy. That is enough to express "do these K independent things", which is
what the primes program did, but it leaves the program holding K unrelated proxies with no
way to talk about them collectively.

Most parallel programs want something different: a **collection** of chares that can be
named by index, operated on as a whole, and used to express a communication pattern.
A simulation of two-dimensional space wants a chare per tile, where the tile at `(i, j)`
exchanges data with `(i±1, j)` and `(i, j±1)`. A sort wants element `i` to talk to element
`i+1`. Writing either with individually created chares means building the index-to-proxy
mapping by hand and shipping it to everyone who needs it.

A **chare array** is that collection: an indexed set of chares, created in one call,
addressed by index, and distributed across the processors by the runtime. It is the most
widely used construct in Charm++, and nearly every program from here on is built from one.

Chare arrays are indexed by integers in one through six dimensions, and they can also be
indexed by bit vectors or by strings. They may be **dense** (every index from 0 to n−1
exists) or **sparse** — a 1D array might hold 10,000 elements with indices scattered over
the range 10 million to 20 million. This chapter uses dense 1D arrays throughout; the other
index types are previewed at the end, and covered fully in Chapter 5.

---

## Declaring a Chare Array

A chare array is declared in the `.ci` file with the `array` keyword and a dimensionality:

```cpp
// ring.ci
array [1D] Ring {
  entry Ring(CProxy_Main mp, int ringSize);
  entry void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE);
};
```

This is the same shape as the `chare` declaration from Chapter 2 — a constructor and some
entry methods — with `array [1D]` in place of `chare`. As before, `charmc` generates a
proxy class, `CProxy_Ring`.

The C++ class inherits from `CBase_Ring`, exactly as an individual chare inherits from
`CBase_Foo`:

```cpp
class Ring : public CBase_Ring {
  CProxy_Main mainProxy;
  int ringSize;
public:
  Ring(CkMigrateMessage *m) {}
  Ring(CProxy_Main mp, int rs) : mainProxy(mp), ringSize(rs) {}
  // ...
};
```

The `CkMigrateMessage *` constructor matters more here than it did in Chapter 2: array
elements are the migratable unit in Charm++, and the load balancer will move them
(Chapter 8). It can stay empty for now.

---

## Creating the Array

The whole array is created with one `ckNew` call:

```cpp
CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize, ringSize);
//                                    \_____constructor args_____/  \_count_/
```

**The element count is the last argument**, after the constructor arguments. Every element
is constructed with the same argument values — here, a proxy to the main chare and the ring
size. The call returns a proxy to the **entire array**, and like every other `ckNew` it is
non-blocking: it returns immediately, and the elements are constructed asynchronously on
whichever PEs the runtime chooses.

The program does not choose where elements go. With 5 elements on 3 PEs, the assignment is
the runtime's to make — and once migration enters the picture, it can change during the run.
A correct Charm++ program never depends on the mapping.

---

## Addressing an Element

Applying the index operator to the array proxy produces a proxy to one element:

```cpp
ring(3).doSomething(...);    // invoke on element 3 only
```

The invocation is asynchronous, exactly like an entry method call on a single chare. The
parameters are packed into a message, the message goes to whichever PE currently holds
element 3, and control returns immediately.

Inside an element, `thisProxy` is a proxy to **its own array**, so a sibling is addressed
the same way:

```cpp
thisProxy(nextIndex).doSomething(...);
```

and `thisIndex` is the element's own index — a plain `int` for a `[1D]` array:

```cpp
int nextI() { return (thisIndex + 1) % ringSize; }
```

`ring[3]` and `ring(3)` are equivalent for a 1D array. This tutorial uses parentheses
throughout, because they extend directly to `grid(i, j)` for a two-dimensional array,
where the bracket form needs an explicit index object.

---

## A Complete Program: The Ring

The ring program passes a single token around a chare array. Element `i` sends to element
`i+1`, wrapping at the end, for a given number of trips around the ring. The token carries
everything needed to continue: how many elements are left in this trip, how many trips
remain, and — purely so the output can be read — where it came from.

The interface file declares one main chare and one array:

```cpp
// ring.ci
mainmodule ring {

  mainchare Main {
    entry Main(CkArgMsg *m);
    entry void ringFinished();
  };

  array [1D] Ring {
    entry Ring(CProxy_Main mp, int ringSize);
    entry void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE);
  };

};
```

The main chare creates the array and starts the token at a random element:

```cpp
// ring.C
class Main : public CBase_Main {
public:
  Main(CkMigrateMessage *m) {}
  Main(CkArgMsg *m) {
    int ringSize  = atoi(m->argv[1]);
    int tripCount = atoi(m->argv[2]);
    delete m;

    CkPrintf("Ring: size=%d trips=%d PEs=%d\n", ringSize, tripCount, CkNumPes());

    CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize, ringSize);
    srand(time(NULL));
    ring(rand() % ringSize).doSomething(ringSize, tripCount, -1, -1);
  }

  void ringFinished() { CkExit(); }
};
```

Starting at a random element is not decoration. No element of a chare array is privileged,
and the program works the same wherever the token starts.

Each element, on receiving the token, prints a line and passes it on:

```cpp
class Ring : public CBase_Ring {
  CProxy_Main mainProxy;
  int ringSize;
public:
  Ring(CkMigrateMessage *m) {}
  Ring(CProxy_Main mp, int rs) : mainProxy(mp), ringSize(rs) {}

  int nextI() { return (thisIndex + 1) % ringSize; }

  void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE) {
    CkPrintf("Ring[%d](%d): tripsLeft=%d from [%d](%d)\n",
             thisIndex, CkMyPe(), tripsLeft, fromIndex, fromPE);
    if (elementsLeft > 1) {
      thisProxy(nextI()).doSomething(elementsLeft-1, tripsLeft, thisIndex, CkMyPe());
    } else if (tripsLeft > 1) {
      thisProxy(nextI()).doSomething(ringSize, tripsLeft-1, thisIndex, CkMyPe());
    } else {
      mainProxy.ringFinished();
    }
  }
};
```

Three cases: elements remain in this trip, so pass the token on; the trip is finished but
more trips remain, so start a new one; or everything is done, so tell the main chare, which
exits. The element does not need to know how far along the ring the token is — that state
travels in the message.

### Build and run

```bash
cd examples/ring
make
./charmrun ++local +p3 ./ring 5 2
```

```
Ring: size=5 trips=2 PEs=3
Ring[4](2): tripsLeft=2 from [-1](-1)
Ring[0](0): tripsLeft=2 from [4](2)
Ring[1](0): tripsLeft=2 from [0](0)
Ring[2](1): tripsLeft=2 from [1](0)
Ring[3](1): tripsLeft=2 from [2](1)
Ring[4](2): tripsLeft=1 from [3](1)
Ring[0](0): tripsLeft=1 from [4](2)
...
```

Each line reads `Ring[element](PE)`. The token started at element 4 (the `from [-1](-1)`
line) and walked 4 → 0 → 1 → 2 → 3, twice. Five elements on three PEs came out as
0,1 → PE 0; 2,3 → PE 1; 4 → PE 2. Nothing in the program asked for that distribution, and
running with `+p2` or `+p4` will produce a different one.

---

## Several Rings at Once

Nothing restricts a program to one chare array. A program may create as many as it likes,
including several of the same chare class, of different sizes, all running at the same time.

The `multiring` program does exactly that. The change to the main chare is a loop:

```cpp
// Create the rings. Each is a separate chare array; they never interact.
for (int i = 0; i < numRings; i++) {
  CProxy_Ring ring = CProxy_Ring::ckNew(thisProxy, ringSize[i], i, ringSize[i]);
  ring(rand() % ringSize[i]).doSomething(ringSize[i], tripCount[i], -1, -1);
}
```

Each `ckNew` produces a **new array** with its own elements and its own proxy. The arrays
are independent: they have different sizes, run different numbers of trips, and never
exchange a message. The runtime places and schedules all of their elements together.

Two details follow from this.

**Elements need an ID if they must know which collection they belong to.** An element can
ask for `thisIndex`, but there is no "which array am I in?" query — the proxy identifies the
array, not the element. So the ring ID is passed to the constructor and stored:

```cpp
Ring(CProxy_Main mp, int rs, int rID) : mainProxy(mp), ringSize(rs), ringID(rID) {}
```

**Completion is counted per collection.** The main chare now waits for *n* rings rather
than one:

```cpp
void ringFinished() {
  if (++finishedCount >= numRings) CkExit();
}
```

`finishedCount` is a member variable of the main chare. It is tempting to write
`static int finishedCount` inside the method instead, and it would even work here — but a
function-local `static` is per-process state, not per-chare state, and it is wrong the
moment a chare class has more than one instance or migrates. Keep per-chare state in
members.

---

## Letting the Sender Choose the Receiver

In the single ring, each element sends to `thisIndex + 1`. The multiple-ring version skips
a random number of elements instead:

```cpp
int nextI(int s) { return (thisIndex + s) % ringSize; }

if (elementsLeft > 1) {
  int skipAmount = (rand() % elementsLeft) + 1;
  thisProxy(nextI(skipAmount))
      .doSomething(elementsLeft - skipAmount, tripsLeft, thisIndex, CkMyPe());
}
```

The skip is a stand-in for non-uniform work, but it also makes a point about the
programming model. The target index is an ordinary run-time expression: the sender decides,
at the moment of sending, which element receives the message. The receiving element does
nothing to prepare, and it does not know a message is coming.

This is one of the clearest differences from two-sided message passing. In MPI the
receiving process must post a matching `recv`, so it has to know that a message is on the
way and from whom. Here the destination is computed from a random number on the sender's
side, and no corresponding call exists on the receiver. Any pattern where "one or more
elements may or may not receive a message, depending on a condition known only to the
sender" is written the same direct way.

An element may also send to itself: `thisProxy(thisIndex)` is a legal target, and the
message goes through the scheduler like any other.

---

## Splitting the Program Across Modules

Both chare classes in `multiring` could live in one `.ci` file, as they do in `ring`. The
program splits them instead, because real applications outgrow a single file, and because
a library is delivered as a module that an application includes.

A program has exactly **one** `mainmodule`, and may have any number of additional modules,
conventionally one per `.ci` file. A module that refers to another module's types declares
`extern module`:

```cpp
// multiRing_Main.ci
mainmodule multiRing_Main {

  mainchare Main {
    entry Main(CkArgMsg *m);
    entry void ringFinished();
  };

  // Include the Ring module
  extern module multiRing_Ring;

};
```

```cpp
// multiRing_Ring.ci
module multiRing_Ring {

  // Include the Main module (for the CProxy_Main parameter below)
  extern module multiRing_Main;

  array [1D] Ring {
    entry Ring(CProxy_Main mp, int rs, int rID);
    entry void doSomething(int elementsLeft, int tripsLeft, int fromIndex, int fromPE);
  };

};
```

The dependency runs both ways here: `Main` creates `Ring` elements, and `Ring`'s
constructor takes a `CProxy_Main`. Each module declares the other as `extern`.

Each `.C` file then includes **its own** module's generated headers — `.decl.h` at the top,
`.def.h` at the bottom — exactly once across the whole program:

```cpp
// multiRing_Main.C
#include "multiRing_Main.decl.h"
class Main : public CBase_Main { /* ... */ };
#include "multiRing_Main.def.h"
```

```cpp
// multiRing_Ring.C
#include "multiRing_Ring.decl.h"
class Ring : public CBase_Ring { /* ... */ };
#include "multiRing_Ring.def.h"
```

The Makefile compiles each `.ci` and each `.C`, then links both objects:

```make
OBJS = multiRing_Main.o multiRing_Ring.o

multiRing: $(OBJS)
	$(CHARMC) -language charm++ -o multiRing $(OBJS)

multiRing_Main.decl.h: multiRing_Main.ci
	$(CHARMC) multiRing_Main.ci

multiRing_Ring.decl.h: multiRing_Ring.ci
	$(CHARMC) multiRing_Ring.ci
```

Run it with a ring size and trip count per ring:

```bash
cd examples/multiring
make
./charmrun ++local +p3 ./multiRing 3 5 3 10 1 8 2
```

That is three rings — 5 elements for 3 trips, 10 elements for 1 trip, 8 elements for 2
trips — all in flight at once.

---

## Reading the Output

The output of that run is confusing on first sight, and the confusion is the lesson:

```
Ring 0[0](0): tripsLeft = 3, from [-1](-1)
Ring 1[2](0): tripsLeft = 1, from [-1](-1)
Ring 0[0](0): tripsLeft = 3, from [0](0)
Ring 1[3](0): tripsLeft = 1, from [2](0)
Ring 2[7](2): tripsLeft = 2, from [-1](-1)
Ring 0[1](0): tripsLeft = 2, from [0](0)
...
```

Three rings are interleaved, and on a larger run the header printed by the main chare —
before any array element existed — can appear in the middle of the output, after lines from
elements it created.

`CkPrintf` output is itself delivered by message. The text is packed up and sent to the
process doing the printing, and those messages are subject to the same absence of ordering
guarantees as everything else. Print order is not program order, and on a busy run it is
not even close.

Two working rules follow:

- **Never infer event ordering from print order.** If you need to know that A happened
  before B, the program must establish that, not the log.
- **Put identity in every line.** `Ring %d[%d](%d) ... from [%d](%d)` records which
  collection, which element, which PE, and where the message came from. That is what makes
  the log sortable after the fact: group by ring, and each ring's traversal reads back in
  order even though the lines were interleaved on screen.

---

## Other Kinds of Index

Everything above used a dense, integer-indexed, one-dimensional array. The same collection
mechanism covers considerably more, and the syntax is a small extension of what you have
already seen.

**Multidimensional.** Up to six dimensions, declared with the dimensionality and created
with one extent per dimension:

```cpp
array [2D] Tile { ... };                      // in the .ci file
CProxy_Tile grid = CProxy_Tile::ckNew(m, n);  // an m x n array
grid(i, j).startStep();                       // address one element
```

Inside an element, `thisIndex` is a struct rather than an `int` — `thisIndex.x` and
`thisIndex.y` for a 2D array. Chapter 5 works through a 2D five-point stencil, which is
where multidimensional indexing earns its keep.

**Sparse.** An array need not be created with a size at all. Start it empty and insert the
elements that exist:

```cpp
CProxy_Compute computeArray = CProxy_Compute::ckNew();   // no elements yet
computeArray[CkArrayIndex6D(x1, y1, z1, x2, y2, z2)].insert();
// ... insert the rest ...
computeArray.doneInserting();
```

This is how a 6D array holding a few thousand elements over an index space of billions is
built — only the indices that exist are inserted. Note the bracket form with an explicit
`CkArrayIndex6D`: that is the general way to address an element, and the `grid(i, j)` form
above is shorthand for it.

**Bit vectors and strings.** An index can also be a bit vector or a string, for collections
whose natural name is not a tuple of integers.

Chapter 5 covers these properly. The molecular dynamics mini-application
[LeanMD](https://github.com/UIUC-PPL/leanmd) is worth looking at alongside it: it pairs a
dense `array [3D] Cell` of spatial cells with a sparse `array [6D] Compute`, where a
Compute element indexed `(x1,y1,z1,x2,y2,z2)` computes forces between the pair of cells
`(x1,y1,z1)` and `(x2,y2,z2)` — an index space of cells-squared, of which only neighboring
pairs are ever inserted.

---

## Pitfalls

1. **The element count goes last in `ckNew`.** `CProxy_Ring::ckNew(thisProxy, ringSize, ringSize)`
   passes two constructor arguments and then the count. Putting the count first, or omitting
   it, gives either a compile error or an array of the wrong size.

2. **Nothing guarantees where an element lives.** `CkMyPe()` inside an element tells you
   where it is right now. Code that depends on a particular element being on a particular PE
   is wrong even before load balancing is introduced.

3. **A function-local `static` is not per-chare state.** It is per-process. Use member
   variables for anything an element or the main chare needs to remember.

4. **Apostrophes in `.ci` comments produce a preprocessor warning.** `charmc` runs the C
   preprocessor over the interface file, so `// the Main object's module` yields
   `warning: missing terminating ' character`. Harmless, but avoid it.

5. **Print order is not program order** — see above. This is the single most common source
   of imagined bugs in a first Charm++ program.

---

## Exercises

**1. Bidirectional ring.** Modify `ring` so that the token alternates direction: on an even
trip it passes to `thisIndex + 1`, on an odd trip to `thisIndex - 1`, wrapping correctly in
both directions. Confirm from the output that the traversal really reverses.

The interesting moment is the handoff between one trip and the next. When the element that
ends a trip passes the token on, which direction should it use — the direction of the trip
that just finished, or of the one about to start? Try the wrong one and watch what the token
does.

**2. Two tokens in one array.** In `multiring` each ring is a separate array. Instead, create
a *single* array of N elements and run two tokens through it at once — one stepping by 1, one
stepping by 2 — each with its own trip count, exiting when both have finished.

Two questions to answer from the code, not from guessing. What does an element need to
remember to keep the two tokens apart? And what happens if both tokens arrive at the same
element at the same moment — can the two entry method invocations interleave?

Then check something about the stride-2 token: have it report which elements it visited
during one trip of N hops. Run it with N even and with N odd. If "one trip" means N hops,
does the stride-2 token actually visit every element? What does the answer depend on?

**3. Communication granularity.** Create an array `A` of 2 elements. `A[0]` is a producer and
`A[1]` a consumer. `A[0]` generates N random doubles in batches of K, sending each batch to
`A[1]` with an entry method `request`; `A[1]` squares each value and returns the batch with
`response`. Take N and K from the command line. Time the whole run for a fixed N over a range
of K from 1 to about 16,000, and explain the shape of both curves — seconds per batch, and
seconds per value.

Two conditions before the numbers mean anything. Build Charm++ **with `--with-production`**:
a build with error checking on prints a banner telling you not to benchmark with it, and it
means it. And decide whether `A[0]` fires all N/K batches immediately or waits for a response
before sending the next. Try both. One of them measures how fast the pipeline runs; the other
measures how long a round trip takes. Which is which, and which one has a memory problem?

---

## Summary

- A **chare array** is an indexed collection of chares, declared `array [1D] Foo` in the
  `.ci` file and created in a single `ckNew` call with the element count **last**.
- Elements are distributed over the PEs by the runtime. The program never chooses, and must
  never depend on, the placement.
- `arr(i)` addresses element `i`; inside an element, `thisIndex` is its own index and
  `thisProxy(j)` addresses a sibling. The target index is an ordinary run-time expression,
  computed by the sender.
- A program may create **any number of arrays**, including several of the same class. They
  are independent; give elements an ID if they must know which collection they are in.
- A program has one `mainmodule` and any number of `module`s, one per `.ci` file, referring
  to each other with `extern module`. Each `.C` file includes its own `.decl.h` and
  `.def.h`.
- `CkPrintf` output arrives by message, so **print order is not program order**. Put
  identity in every line.

Everything here used point-to-point invocations on individual elements. The next chapter adds the
two collective operations that make a chare array more than a list of chares: **broadcast**,
which invokes an entry method on every element, and **reduction**, which combines a value
from every element and delivers the result to one place.
