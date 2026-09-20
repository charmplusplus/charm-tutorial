# Chapter 6: Structured Dagger (SDAG)

The programs you wrote in the previous exercises — parallel prefix sum and odd-even
transposition sort — are correct, but they share an uncomfortable structural property.
The logic of each chare is fundamentally sequential: "first do step 1, then wait for a
message, then do step 2, ..." Yet the code does not look sequential at all. It is
spread across a constructor, a private `advance()` method, and a `receiveVal` entry
method, with a message buffer and a flag or two to stitch them together. Every time
the runtime delivers a message, `receiveVal` figures out what state the chare is in,
applies the received value, and re-enters `advance()` to push the chare forward.

The code works, but reasoning about it requires holding the entire state machine in your
head simultaneously. If you wanted to add a third phase, you would need to extend the
buffer, add another flag, and add another case to `advance()`.

**Structured Dagger (SDAG)** is a Charm++ extension that lets you write the chare's
lifecycle as a sequential program — one that can block waiting for a message, then
resume. The underlying execution model is unchanged (messages still arrive in any order,
the scheduler still runs other chares while a chare is "blocked"), but the code reads
like a straight-line program.

---

## The Idea

In SDAG, the body of certain entry methods is written directly in the `.ci` file, not
in the `.C` file. The Charm++ interface compiler (`charmc`) translates those bodies into
a state machine — but you never see the state machine. You see the sequential version.

Two constructs do the work:

- **`serial { ... }`** — a block of ordinary C++ code. The scheduler cannot interrupt
  execution inside a serial block; from the chare's point of view, it runs atomically.

- **`when method(args) serial { ... }`** — a blocking receive. Execution suspends here
  until an invocation of `method` on this chare arrives. When it does, the body runs.
  If the message has already arrived before `when` is reached, it is consumed immediately
  without suspending.

The SDAG runtime buffers messages that arrive before a `when` is ready to consume them.
You do not manage the buffer; you just write `when`.

---

## A Minimal Example

Here is the simplest use of SDAG: a chare that sends a value to another chare and then
waits for a reply before printing.

```cpp
// .ci file
array [1D] Ping {
  entry Ping();
  entry void reply(int val);
  entry void start() {
    serial {
      pongArray[thisIndex ^ 1].ping(myVal);
    }
    when reply(int val) serial {
      CkPrintf("[%d] received %d\n", thisIndex, val);
    }
    serial {
      contribute(CkCallback(CkReductionTarget(Main, done), mainProxy));
    }
  };
};
```

```cpp
// .C file
class Ping : public CBase_Ping {
  int myVal;
public:
  Ping_SDAG_CODE

  Ping() : myVal(thisIndex + 1) {}
  Ping(CkMigrateMessage* m) {}
};
```

The `.ci` body of `start()` reads like a sequential program. The `when reply(...)` line
blocks until the other chare sends a `reply` message. Everything after it runs in order.

---

## `serial` Blocks

A `serial` block is arbitrary C++. It can call any function, modify member variables,
send messages to other chares, or call Charm++ utilities:

```cpp
serial {
  int x = compute(value);
  value += x;
  otherProxy.sendData(value);
  CkPrintf("[%d] value is now %d\n", thisIndex, value);
}
```

Within a `serial` block the scheduler cannot deliver any other message to this PE.
The block should therefore be short — it is not a place for long computation. Long
computation in a serial block stalls all other chares on the same PE.

---

## `when` Clauses and Buffering

A `when` clause designates an entry method as the source of the next expected message:

```cpp
when receiveData(int n, double val) serial {
  value += val;
}
```

If the message has not arrived yet, the Charm++ runtime returns this chare's thread
to the scheduler and resumes it when the message arrives. Any other chares on this PE
continue to run normally.

If the message has already arrived (because the sender was fast), `when` finds it in
the runtime's buffer and proceeds without suspending. Either way, from the chare's
perspective, `when` simply blocks and then continues.

A crucial consequence: **the order of `when` clauses in the `.ci` body defines the
order in which messages are consumed**, regardless of the order they arrive. The runtime
buffers all early arrivals. This is the SDAG replacement for the manual message-buffer
map (`std::map<int,int> recvBuf`) you wrote in the prefix sum.

---

## Reference Numbers

The buffering just described handles a single `when` per entry method cleanly, but what
about a loop that receives many messages of the same type, one per iteration?

```cpp
// naive — WRONG in a loop
for (round = 0; round < N; round++) {
  serial { neighbor.update(round, value); }
  when update(int r, int val) serial { value += val; }
}
```

The problem: all arriving `update` messages go into the same pool. When the `when` in
round 0 fires, it might consume a message from round 5. The result is incorrect.

SDAG solves this with **reference numbers**. The first `int` parameter of an entry
method is its implicit reference number. When you write `when update[round](...)`, the
`when` only matches a message whose first argument equals the current value of `round`:

```cpp
entry void update(int round, int val);   // 'round' is the reference number
```

```cpp
// correct — with reference numbers
for (round = 0; round < N; round++) {
  serial { neighbor.update(round, value); }
  when update[round](int r, int val) serial { value += val; }
}
```

Now `when update[round]` in iteration 3 will only consume an `update` message whose
first argument is 3. Messages from future rounds (round = 5, 7, ...) are buffered by
the SDAG runtime and delivered when the loop reaches those iterations. You no longer
need a buffer, flags, or a re-entrant `advance()`.

This is the mechanism that replaces the entire `bufRound`/`bufVal` scalar buffer from
the odd-even sort, and the `recvBuf` map from the prefix sum.

---

## SDAG `for` Loops

SDAG supports `for`, `while`, and `if` constructs inside entry method bodies. The `for`
loop follows C++ syntax, but with one constraint: **the loop variable must be a class
member**, not declared in the loop header. This is because the SDAG compiler needs to
store the variable between suspension points.

```cpp
// .ci
entry void start() {
  for (dist = 1; dist < numElements; dist <<= 1) {
    serial { /* ... */ }
    when recv[dist](int ref, int val) serial { /* ... */ }
  }
};

// .C — dist must be a member variable
class Foo : public CBase_Foo {
  int dist;   // loop variable must be a member
  ...
};
```

Any combination of `serial`, `when`, and `if` blocks can appear inside the loop body.
Iterations run sequentially: the body of iteration k completes (including any `when`
suspensions) before iteration k+1 begins. This is correct for algorithms like prefix
sum and odd-even sort, where each step depends on the previous one.

---

## SDAG `if`

SDAG `if` works exactly as you expect. It can gate `serial` blocks, `when` clauses, or
nested constructs:

```cpp
if (thisIndex + dist < numElements) serial {
  otherProxy[thisIndex + dist].send(dist, value);
}
if (thisIndex >= dist)
  when recv[dist](int ref, int val) serial {
    value += val;
  }
```

If the condition references values computed in a preceding `serial` block, those values
must be stored in member variables (not local variables) so the SDAG compiler can access
them across suspension points. For example, in odd-even sort, `amLeft` and `amRight` are
set in a `serial` block and then used in the `if` condition that gates the `when`:

```cpp
// .ci
for (round = 0; round < numElems; round++) {
  serial {
    int offset = round % 2;
    amLeft  = ((thisIndex % 2) == offset) && (thisIndex + 1 < numElems);
    amRight = ((thisIndex % 2) != offset) && (thisIndex > 0);
    // ... send if active ...
  }
  if (amLeft || amRight)
    when receiveVal[round](int r, int partnerVal) serial {
      if (amLeft) value = std::min(value, partnerVal);
      else        value = std::max(value, partnerVal);
    }
}

// .C — amLeft and amRight must be members
class Sorter : public CBase_Sorter {
  int value, round;
  bool amLeft, amRight;
  ...
};
```

IDLE elements — those where both `amLeft` and `amRight` are false — skip the `when`
entirely for that round. The loop counter increments and the chare continues to the
next round without waiting for any message. No special handling is required; the `if`
does the right thing.

---

## The `ClassName_SDAG_CODE` Macro

Any chare class that contains SDAG code must include the macro `ClassName_SDAG_CODE`
inside its class body:

```cpp
class Prefix : public CBase_Prefix {
public:
  Prefix_SDAG_CODE   // required — generates entry points and scheduler hooks

  Prefix() { ... }
  Prefix(CkMigrateMessage* m) {}
};
```

The macro is generated by `charmc` from the `.ci` file. It inserts the internal
machinery that implements the `when` buffering and the suspension/resume mechanism.
Omitting it causes a compile error. There is nothing else to do; the macro takes care
of everything.

---

## Complete Example: Parallel Prefix Sum

The SDAG version of the parallel prefix sum lives in `examples/prefix_sdag/`. Here is
the `.ci` file in full, with annotations.

```cpp
mainmodule prefix {
  readonly CProxy_Main mainProxy;
  readonly int numElements;
  readonly CProxy_Prefix prefixArray;

  mainchare Main {
    entry Main(CkArgMsg*);
    entry [reductiontarget] void done();
  };

  array [1D] Prefix {
    entry Prefix();
    entry void passValue(int stepDist, int val);  // (1)
    entry void start() {                           // (2)
      for (dist = 1; dist < numElements; dist <<= 1) {   // (3)
        if (thisIndex + dist < numElements) serial {      // (4)
          usleep(rand() % 5001);
          prefixArray[thisIndex + dist].passValue(dist, value);
        }
        if (thisIndex >= dist)                            // (5)
          when passValue[dist](int stepDist, int val) serial {  // (6)
            value += val;
          }
      }
      serial {                                             // (7)
        int expected = (thisIndex + 1) * (thisIndex + 2) / 2;
        CkPrintf("Prefix[%d] = %d  (expected %d)  %s\n",
                 thisIndex, value, expected,
                 (value == expected) ? "OK" : "WRONG");
        CkCallback cb(CkReductionTarget(Main, done), mainProxy);
        contribute(0, NULL, CkReduction::nop, cb);
      }
    };
  };
};
```

**(1)** `passValue(int stepDist, int val)` — the first parameter is `int`, so
`stepDist` is the reference number. When this chare sends
`prefixArray[i].passValue(dist, value)`, the message is tagged with the value `dist`.

**(2)** `start()` is an SDAG entry method — its body is written here in the `.ci` file.
`Main` calls `prefixArray.start()` to kick off the algorithm.

**(3)** The loop variable `dist` is a class member. The step `dist <<= 1` doubles
the stride each iteration, giving the log(N) steps of recursive doubling.

**(4)** The right-neighbor send. The `if` condition ensures element N-1 does not try
to send out of bounds. The `serial` block includes a random delay (`usleep`) to stress-
test that out-of-order delivery is handled correctly.

**(5)** The left-neighbor receive. Elements with no left neighbor at this step (those
with `thisIndex < dist`) skip the `when` entirely.

**(6)** `when passValue[dist]` — the `[dist]` tag means: match only a `passValue`
message whose first argument equals the current value of `dist`. Messages arriving early
(from a larger step) are held in the SDAG buffer until the loop reaches the matching
iteration.

**(7)** After all steps complete, every element has its prefix sum. Print and contribute
to a no-op reduction to signal `Main` that the algorithm is finished.

The corresponding `.C` file is remarkably thin:

```cpp
#include "prefix.decl.h"
#include <unistd.h>

/*readonly*/ CProxy_Main mainProxy;
/*readonly*/ CProxy_Prefix prefixArray;
/*readonly*/ int numElements;

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        numElements = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
        delete msg;
        mainProxy = thisProxy;
        prefixArray = CProxy_Prefix::ckNew(numElements);
        prefixArray.start();    // kick off the SDAG entry method
    }
    void done() { CkExit(); }
};

class Prefix : public CBase_Prefix {
    int value, dist;    // dist is the SDAG for-loop variable; must be a member
public:
    Prefix_SDAG_CODE

    Prefix() : value(thisIndex + 1), dist(0) {
        srand(thisIndex * 1009 + 7);
    }
    Prefix(CkMigrateMessage* m) {}
};

#include "prefix.def.h"
```

Compare this with the manual version in `examples/prefix/prefix.C`: no `recvBuf` map,
no `sentAtCurrentDist` flag, no `isDone` guard, no `advance()` method. The logic has
moved into the `.ci` file where it reads sequentially.

---

## Before and After: Odd-Even Transposition Sort

The cleanup is even more striking for the odd-even sort, where the manual version
required a carefully-reasoned scalar buffer to prevent deadlock.

| Manual (`examples/oddevensort/`) | SDAG (`examples/oddevensort_sdag/`) |
|---|---|
| `bufRound`, `bufVal` — one-slot buffer for a future-round message | automatic — SDAG runtime buffers by reference number |
| `sentRound` flag — prevents re-sending on re-entry | one `serial` per loop iteration, executes once |
| `advance()` — re-entrant, called from constructor and `receiveVal` | eliminated — `start()` in the `.ci` is the control flow |
| `receiveVal(int partnerVal, int r)` — splits current/future round handling | `receiveVal(int round, int val)` — round first, plain receive |
| 115 lines total | 45 lines total |

In the manual version, the argument to `receiveVal` was `(int val, int round)` — value
first, round second. The SDAG version reverses this to `(int round, int val)` because
the reference number mechanism uses the **first** `int` parameter. This is a mechanical
consequence of the tagging rule, not an algorithmic change.

The proof of buffer sufficiency (at most one future-round message can be in flight at
any time) was necessary to justify the one-slot buffer in the manual version. In the
SDAG version, that proof is no longer needed: SDAG buffers arbitrarily many early-
arriving messages. The program is correct for a more general reason.

---

## Exercise: Choose One

Convert one of your two manual implementations to use SDAG. Both exercises teach the
same constructs. Choose based on which implementation you found harder to write and
reason about.

### Option A: Parallel Prefix Sum

Convert `examples/prefix/` to use SDAG. Start with a new directory so you can compare.

Key steps:

1. Add an `entry void start()` body to the `.ci` file.
2. Write a `for (dist = 1; dist < numElements; dist <<= 1)` loop in the body.
3. Inside the loop, send to the right neighbor (inside a `serial` block) and receive
   from the left neighbor using `when passValue[dist](...)`.
4. After the loop, print the result and call `contribute` (inside a `serial` block).
5. In the `.C` file, declare `dist` as a member variable. Place `Prefix_SDAG_CODE`
   in the class body. Have `Main` call `prefixArray.start()`.
6. Remove `advance()`, `recvBuf`, `sentAtCurrentDist`, and `isDone`.

The result should closely resemble Figure 7.3 in the SDAG chapter of the Charm++
tutorial book.

### Option B: Odd-Even Transposition Sort

Convert `examples/oddevensort/` to use SDAG.

Key steps:

1. Add an `entry void start()` body to the `.ci` file.
2. Write a `for (round = 0; round < numElems; round++)` loop.
3. Inside the loop, compute `amLeft` and `amRight` in a `serial` block and send
   if active.
4. Gate the `when` with `if (amLeft || amRight)`:
   `when receiveVal[round](int r, int partnerVal) serial { ... }`
5. Swap the parameter order in `receiveVal` to `(int round, int val)` — round must be
   first to serve as the reference number.
6. Declare `round`, `amLeft`, `amRight` as class members.
7. Remove `advance()`, `bufRound`, `bufVal`, `sentRound`.

IDLE elements (both `amLeft` and `amRight` false) will skip the `when` via the `if`
condition. No special-casing is needed.

### Testing

Build with `make` and run:

```bash
./charmrun ++local +p4 ++ppn 2 ./program 8
./charmrun ++local +p4 ++ppn 2 ./program 16
./charmrun ++local +p2 ++ppn 2 ./program 8
```

All elements should print `OK`. The print order will vary between runs (chares complete
in non-deterministic order), but all values should be correct.

---

## Further Exercise: K-Means Clustering

Implement Lloyd's algorithm for K-means clustering as a Charm++ program.

**Setup.** Each of N chares owns a subset of M/N data points in 2D space. The main
chare holds K cluster centroids (initially chosen at random). Each iteration:

1. Main broadcasts the current centroids to all chares.
2. Each chare assigns every local point to its nearest centroid, then contributes two
   reductions back to Main: the per-cluster point count and the per-cluster coordinate
   sum.
3. Main receives both reductions, recomputes the centroids (sum / count), and starts the
   next iteration if the centroid shift exceeds a convergence threshold.

**Charm++ concepts exercised.** SDAG `for` loop (or `while`) for iterations, array
parameters for broadcasting centroids and receiving partial sums, reductions with custom
data, and the interaction between broadcasts and reductions within a single iteration.

**Challenge.** A single iteration requires two separate reductions (counts and coordinate
sums). Think carefully about how Main coordinates receiving both before recomputing
centroids.

**Parameters.** Accept `M` (total points), `N` (number of chares), and `K` (clusters)
on the command line. Print the final centroids and the number of iterations to convergence.

---

## SDAG and the Chare Lifecycle

A common misconception: SDAG changes the execution model.

It does not. Messages still arrive in any order. The scheduler still interleaves chares
on each PE. `serial` blocks run without interruption, but only within a single block —
between any two sequential statements that span a `when`, the scheduler may run any
number of other chares.

What SDAG provides is:

1. **A sequential notation** for control flow that spans multiple message receives.
2. **A runtime-managed buffer**, keyed by reference number, that holds early-arriving
   messages until the matching `when` is ready to consume them.

The state machine that the manual version forced you to implement by hand is generated
for you by `charmc`. You write the sequential intent; the compiler generates the correct
asynchronous implementation.

---

## Further Constructs

This chapter covered `serial`, `when`, `for`, and `if` — sufficient for the exercises
and for most practical SDAG programs. Three additional constructs exist for more
complex dependency patterns:

- **`forall [i] (lo:hi, stride) { ... }`** — iterations that are independent of each
  other and can execute in any order (though not in parallel).
- **`overlap { ... }`** — a block containing multiple SDAG statements that may execute
  in any order; control exits when all of them have completed.
- **`case { when A(...) {...}  when B(...) {...} }`** — a disjunction: execute whichever
  branch's message arrives first.

These are documented in the Charm++ readthedocs manual and in the SDAG chapter of the
tutorial book.

---

## Summary

SDAG lets you write the sequential logic of a chare's lifecycle directly in the `.ci`
file, using `serial` and `when` to express C++ computation and message receives.

The key points:

1. `serial { ... }` — atomic C++ block; the scheduler cannot preempt it.
2. `when method(args) serial { ... }` — blocking receive; runtime buffers early arrivals.
3. `when method[ref](int tag, ...)` — tagged receive; only matches messages whose first
   `int` argument equals `ref`. Essential for loops with one receive per iteration.
4. The `for` loop variable must be a class member.
5. `if` conditions that depend on values set in preceding `serial` blocks must store
   those values in member variables.
6. Every class with SDAG code must include `ClassName_SDAG_CODE` in its class body.
7. SDAG entry method bodies go in the `.ci` file; the `.C` file holds only member
   declarations and the constructor.

The underlying execution model is unchanged. SDAG is a notation for expressing
sequential intent; `charmc` translates it into correct asynchronous code.
