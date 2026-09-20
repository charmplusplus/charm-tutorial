# Chapter 9: Threaded Entry Methods

## Motivation

SDAG `when` handles linear waiting patterns well: wait for one or two specific messages,
then continue. But some algorithms have more complex control flow — recursive
divide-and-conquer, nested request-reply chains, or loops that wait at each iteration.
Expressing these as continuation graphs (the SDAG model) produces code that is hard to
read and hard to get right.

Threaded entry methods provide an alternative. They run in **user-level cooperative
threads**, letting you write ordinary sequential C++ — loops, blocking waits, recursive
calls — inside a Charm++ chare.

---

## User-Level Threads

The `[threaded]` attribute causes an entry method to run in a **user-level (Converse)
thread**. These are not OS threads:

- **No preemption.** The scheduler only gains control when the running thread explicitly
  suspends — at a future wait, a sync call, or a direct `CthSuspend()`.
- **No locking needed** for chare state. At most one thread per chare runs at a time.
- **Cooperative concurrency.** While a thread is blocked, the PE's scheduler processes
  other messages — including regular entry methods for the **same** chare. This is how
  the `sendValue` method in Chapter 6's neighbor-exchange example can execute on the
  same PE as the thread that is waiting for it.

The primary example in this chapter is `examples/fib_thr/`, which computes Fibonacci
numbers using parallel recursive chares. The LB phase pattern is shown separately in
`examples/tmethod/`.

---

## The `[threaded]` Attribute

In the `.ci` file, prepend `[threaded]` to an entry method declaration:

```cpp
entry [threaded] void run();
entry [threaded] void compute();
```

The implementation is an ordinary C++ method. You can declare local variables, write
loops, call helper functions, and block at futures — all in straight-line code.

**Restriction:** Charm++ does not allow `[threaded]` constructors. If you need to start
threaded work from object construction, write a plain constructor that stores its
arguments and immediately sends a `[threaded]` entry method to `thisProxy`. The
construction completes, the message is queued, and the scheduler picks it up and starts
the thread. You will see this pattern in the Fibonacci example below.

---

## Running Example: Parallel Fibonacci

Fibonacci is a textbook divide-and-conquer problem. Computing `fib(n)` recursively
spawns two subproblems; with threaded entry methods, we can launch both as chares and
wait for both results before summing.

### `.ci` file

```ci
mainmodule fib_thr {

  message LongMsg;

  mainchare Main {
    entry Main(CkArgMsg*);
    entry [threaded] void run();
  };

  chare Fib {
    entry Fib(int n, CkFuture parentFut);   // plain constructor
    entry [threaded] void compute();         // threaded work method
  };
};
```

`Main::run()` is `[threaded]` so it can block at `CkWaitFuture`. `Fib::compute()` is
`[threaded]` so it can recursively wait for its own sub-results.

### Main chare

```cpp
void run() {                              // [threaded]
    CkFuture f = CkCreateFuture();
    CProxy_Fib::ckNew(n, f);             // spawn root Fib chare
    LongMsg* m = (LongMsg*)CkWaitFuture(f);
    CkPrintf("fib(%d) = %ld\n", n, m->value);
    delete m;
    CkExit();
}
```

`CkCreateFuture()` creates a one-shot channel. `CkWaitFuture(f)` suspends the thread
until someone sends to it. The root `Fib` chare is created with the future so it knows
where to deliver its result.

### Fib chare — constructor restriction pattern

```cpp
class Fib : public CBase_Fib {
    int n;
    CkFuture parentFut;
public:
    // Constructor stores state, queues the threaded compute().
    Fib(int n_, CkFuture f) : n(n_), parentFut(f) {
        thisProxy.compute();             // constructors may not be [threaded]
    }

    void compute() {                     // [threaded]
        long result;
        if (n <= THRESHOLD) {
            result = seqFib(n);
        } else {
            CkFuture f1 = CkCreateFuture();
            CkFuture f2 = CkCreateFuture();
            CProxy_Fib::ckNew(n - 1, f1);
            CProxy_Fib::ckNew(n - 2, f2);
            LongMsg* m1 = (LongMsg*)CkWaitFuture(f1);
            LongMsg* m2 = (LongMsg*)CkWaitFuture(f2);
            result = m1->value + m2->value;
            delete m1; delete m2;
        }
        LongMsg* reply = new LongMsg;
        reply->value = result;
        CkSendToFuture(parentFut, reply);  // deliver result to parent
    }
};
```

The key points:

1. **Constructor restriction.** The constructor stores `n` and `parentFut`, then sends
   `thisProxy.compute()` to itself. The `[threaded]` annotation is on `compute()`, not
   on the constructor.

2. **Both sub-problems fire before either wait.** `CProxy_Fib::ckNew(n-1, f1)` and
   `ckNew(n-2, f2)` are called one after the other, both before any `CkWaitFuture`.
   This means both child chares are created and can run concurrently. The current
   thread then waits for `f1`; while it is suspended, the scheduler can start both
   children. Waiting for `f2` afterwards is often instant because `fib(n-2)` finished
   while `fib(n-1)` was running.

3. **Below threshold, no chares.** To avoid explosive chare creation for small `n`,
   computation falls back to sequential `seqFib` when `n <= THRESHOLD` (default 10).

### Running

```
charmrun ++local +p4 ++ppn 2 ./fib_thr 30
fib(30) = 832040  (0.956 s)
```

---

## CthSuspend and CthAwaken

`CkFuture` is built on two lower-level primitives. Understanding them clarifies exactly
what "blocking" means.

```cpp
// Member variables of the chare:
CthThread waitingThread;
int       receivedVal;

// Inside the [threaded] entry method:
void threadedMethod() {
    waitingThread = CthSelf();          // capture handle to this thread
    someProxy.asyncRequest(thisIndex);  // will eventually call notifyDone()
    CthSuspend();                       // yield PE; scheduler runs
    // execution resumes here after CthAwaken
    CkPrintf("Got: %d\n", receivedVal);
}

// Regular entry method called by the remote chare:
void notifyDone(int val) {
    receivedVal = val;
    CthAwaken(waitingThread);           // re-schedule the suspended thread
}
```

`CthSelf()` returns a handle to the currently executing thread. `CthSuspend()` yields
it — control returns to the scheduler. `CthAwaken(t)` puts thread `t` back on the
ready queue; it resumes at the statement after `CthSuspend()`.

The `waitingThread` member must be saved **before** the async send, because (in
principle) a fast reply could deliver `notifyDone()` on the same PE before
`CthSuspend()` is reached. The Charm++ scheduler guarantees this can't happen within
one message execution (no preemption), but storing the handle early is the correct
idiom.

Use `CthSuspend`/`CthAwaken` when you need custom waiting logic — for example,
waiting until a counter reaches zero, or until *any* of several events fires. For
simple one-result waits, `CkFuture` is more convenient; for request-reply, `[sync]` is
cleanest.

---

## CkFuture

`CkFuture` provides structured wait-for-one-result without manually storing a thread
handle:

```cpp
CkFuture f = CkCreateFuture();
remoteProxy.requestValue(f);           // responder receives f
ValMsg* m = (ValMsg*)CkWaitFuture(f); // suspend until CkSendToFuture(f, ...)
// use m->value ...
delete m;
```

The responder:
```cpp
void requestValue(CkFuture replyTo) {
    ValMsg* reply = new ValMsg;
    reply->value  = value;
    CkSendToFuture(replyTo, reply);    // wakes the waiting thread
}
```

`CkWaitFuture` calls `CthSuspend`; `CkSendToFuture` calls `CthAwaken`. The `CkFuture`
type is a small struct (id + PE); it must be passed as a **value** parameter (`CkFuture
replyTo`), not as a `CkFutureID`, which is only the unsigned-short id field and lacks
the PE needed for cross-PE delivery.

### Multiple outstanding futures

Fire N requests before collecting any results so they proceed in parallel:

```cpp
CkFuture futures[N];
for (int i = 0; i < N; i++) {
    futures[i] = CkCreateFuture();
    workers[i].sendValue(futures[i]);
}
for (int i = 0; i < N; i++) {
    ValMsg* m = (ValMsg*)CkWaitFuture(futures[i]);
    total += m->value;
    delete m;
}
```

All N sends go out before the first wait. The Fibonacci example uses this pattern with
N=2 — both `ckNew` calls happen before either `CkWaitFuture`.

---

## `[sync]` Entry Methods

For simple request-reply, `[sync]` offers the cleanest syntax. The caller blocks; the
callee looks like an ordinary method that returns a value.

Declare in the `.ci` file with a Charm++ message return type:

```cpp
entry [sync] ValMsg* syncGetValue();
```

Call from any `[threaded]` entry method:

```cpp
ValMsg* m = workerArray[right].syncGetValue();  // blocks caller's thread
value = (value + m->value) * 0.5;
delete m;
```

The callee is a plain C++ method that returns a message:

```cpp
ValMsg* syncGetValue() {
    ValMsg* reply = new ValMsg;
    reply->value  = value;
    return reply;
}
```

Internally Charm++ creates a temporary future, sends the call, waits for the return, and
frees the future — equivalent to the three-line `CkFuture` pattern written as a single
blocking call.

**Constraints:**
- `[sync]` can only be called from within a `[threaded]` entry method. Calling it from a
  regular entry method deadlocks.
- The return type must be a Charm++ message class (a subclass of `CMessage_Foo`).
- The callee does not need to know it was called synchronously.

---

## SDAG vs Threaded: When to Use Which

| | SDAG `when` | Threaded + futures |
|---|---|---|
| Programming model | Event-driven (continuations) | Sequential-looking |
| Memory per chare | Small (continuation state on heap) | Thread stack (default 32 KB) |
| Simple wait-for-one | Natural | Natural |
| Loop with wait per iteration | Awkward | Natural |
| Nested or recursive waits | Very awkward | Natural |
| Multiple concurrent waits | Natural with `overlap` | Explicit (fire N, collect in loop) |
| Entry method overhead | Low | Slightly higher (thread context switch) |
| Migration while active | Always safe | **Never safe** (see below) |

Use SDAG for most patterns. Reach for threaded methods when the control flow is
inherently sequential or recursive — divide-and-conquer, deeply nested request-reply, or
algorithms that read most naturally as straight-line blocking code.

---

## Migration Restriction and the LB Phase Pattern

**A chare with an active threaded entry method cannot be migrated.** The LB framework
serializes chare state with `pup()` and reconstructs it on the target PE. Serializing a
live thread stack requires the `isomalloc` feature (position-independent stack
allocation), which is a research capability, not production-ready.

The solution is the **phase pattern**: the threaded method runs for a bounded number of
iterations (one "phase") and *returns*. The SDAG calls `AtSync()` after the thread has
exited, in the quiet gap between phases where migration is safe.

The full example is in `examples/tmethod/`. Workers form a 1D array; each iteration a
worker asks its right neighbor for its value and averages it in. Iterations are grouped
into phases of length `lbPeriod`.

### `.ci` file — outer SDAG loop

```ci
entry void start() {
    for (phase = 0; phase * lbPeriod < maxIter; phase++) {
        serial {
            int s = phase * lbPeriod;
            int e = (s + lbPeriod < maxIter) ? s + lbPeriod : maxIter;
            thisProxy[thisIndex].runPhase(s, e);   // start the thread
        }
        when phaseDone() serial { AtSync(); }      // thread has returned; safe to migrate
        when resumeFromSync() serial { }           // resume after LB
    }
    serial { contribute(allDone); }
};
```

### `.C` file — `runPhase` (threaded)

```cpp
void runPhase(int s, int e) {
    int right = (thisIndex + 1) % numWorkers;
    for (int iter = s; iter < e; iter++) {
        CkFuture f = CkCreateFuture();
        workerArray[right].sendValue(f);
        ValMsg* m = (ValMsg*)CkWaitFuture(f);
        value = (value + m->value) * 0.5;
        delete m;
    }
    thisProxy[thisIndex].phaseDone();   // signal SDAG; thread then exits
}
```

### Flow for each phase

1. SDAG sends `runPhase(s, e)` to self and waits at `when phaseDone()`.
2. Scheduler starts the threaded method.
3. `runPhase` runs all iterations using `CkFuture`, then sends `phaseDone()` and
   **returns**. The thread is now gone.
4. SDAG wakes, calls `AtSync()` — safe because no thread is active.
5. LB may migrate the chare. After any migration, `ResumeFromSync()` sends
   `resumeFromSync()` to the SDAG.
6. Loop repeats for the next phase.

### `pup()` for this pattern

Because migration happens between phases, `pup()` only needs the chare's persistent
state — no thread state to capture:

```cpp
Worker(CkMigrateMessage* m) {}

void pup(PUP::er& p) {
    p | value;
    p | phase;
}

void ResumeFromSync() {
    thisProxy[thisIndex].resumeFromSync();
}
```

---

## Summary

- `[threaded]` entry methods run in user-level cooperative threads, enabling sequential
  blocking code inside Charm++ chares.
- While a thread is blocked, the PE processes other messages — including entry methods
  for the same chare.
- **CthSuspend / CthAwaken**: raw thread control. Store `CthSelf()`, issue an async
  request, call `CthSuspend()`; the responder calls `CthAwaken()` to resume. Use when
  you need custom wake conditions.
- **CkFuture**: structured wait. `CkCreateFuture()` → pass to responder → `CkWaitFuture()`.
  Fire multiple futures before the first wait to run sub-problems in parallel (as in
  Fibonacci). Pass as `CkFuture`, not `CkFutureID`.
- **`[sync]` methods**: cleanest request-reply API. Callable only from threaded context;
  return type must be a Charm++ message class.
- **Constructors may not be `[threaded]`**. The pattern: plain constructor stores
  arguments and sends `thisProxy.compute()` to self; `compute()` carries the
  `[threaded]` attribute.
- **Threaded methods cannot be migrated while active.** Use the phase pattern: threaded
  method runs a bounded loop and returns; SDAG calls `AtSync()` in the gap; after
  `resumeFromSync()` the next phase begins.

---

## Exercises

**Exercise 1 — Odd-even sort with threaded entry methods.**

In Chapter 6 you implemented odd-even transposition sort using SDAG. Rewrite the sort
using a `[threaded]` entry method instead. Each array element should run a loop over
rounds; in each round it uses a `CkFuture` to fetch its neighbor's value and conditionally
swap. Compare the code clarity to the SDAG version.

**Exercise 2 — Parallel prefix sum.**

Implement a parallel prefix sum (scan) over a 1D chare array using threaded entry
methods. In the standard parallel algorithm, the computation proceeds in `log2(N)` steps;
in step `k`, element `i` (for `i >= 2^k`) adds the value from element `i - 2^k`. Use a
`CkFuture` (or `[sync]` method) for each inter-element fetch. The result at element `i`
after all steps should be the sum of the original values at indices 0 through `i`.
