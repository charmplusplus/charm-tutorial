# Exercise: Distributed Hash Table

This exercise applies the SDAG skills from Chapter 6 to a classic distributed-systems
pattern: a **distributed hash table** (DHT). No new Charm++ concepts are introduced; the
goal is to recognize where SDAG and plain entry methods each belong, and to appreciate
what asynchronous messaging makes easy.

*(If you already know MPI, try to sketch how you would implement this with MPI without
resorting to an expensive all-to-all. You will quickly see why asynchronous messaging
changes the picture.)*

---

## Problem Description

There is a key-value table too large to fit on one process. The table is partitioned
across a 1D chare array of size `numChares`: element `i` owns all keys in the range
`[i * keysPerChare, (i+1) * keysPerChare)`.

At startup, every chare:

1. **Populates** its slice of the table with deterministic values.
2. **Issues** `queriesPerChare` random-key lookup requests to whichever element owns
   each key.
3. **Serves** lookup requests that arrive from other chares.
4. When all of its own responses have been received, **contributes** to a reduction
   that counts total errors across all chares.

The same chare array plays both roles simultaneously: each element is a table shard
*and* a client issuing queries.

---

## Design

### Dual Role and Entry Method Placement

The SDAG `start()` method handles the client role: send K requests, then wait (via a
`for`/`when` loop) for K responses. While `start()` is suspended in its `when response`
loop, the chare must still be able to accept `request()` messages from other elements
asking for table lookups.

This is the key design point: **`request()` is a plain entry method, not SDAG**. Plain
entry methods run whenever the scheduler dispatches them, regardless of what the SDAG
entry method is waiting for. The scheduler interleaves them naturally.

```
dht.ci
──────
entry void request(int key, int callerIndex);   ← plain; runs at any time
entry void response(int key, int value);        ← consumed by SDAG when-loop
entry void start() { ... };                    ← SDAG: sends queries, awaits replies
```

### The `start()` SDAG Body

```cpp
entry void start() {
  serial {
    for (int q = 0; q < queriesPerChare; q++) {
      int key  = rand_r(&seed) % (numChares * keysPerChare);
      int owner = key / keysPerChare;
      dhtArray[owner].request(key, thisIndex);   // thisIndex is the reply address
    }
  }
  for (responsesReceived = 0; responsesReceived < queriesPerChare;
       responsesReceived++) {
    when response(int key, int value) serial {
      if (value != tableValue(key)) errorCount++;
    }
  }
  serial {
    CkCallback cb(CkReductionTarget(Main, allDone), mainProxy);
    contribute(sizeof(int), &errorCount, CkReduction::sum_int, cb);
  }
};
```

There is no `reference` number (tag) on the `when response` — responses can arrive
in any order and each one simply satisfies the next iteration of the loop. The
`responsesReceived` loop variable must be a class member, as with any SDAG `for`.

### The `request()` Handler

```cpp
void request(int key, int callerIndex) {
    int value = table.count(key) ? table[key] : -1;
    dhtArray[callerIndex].response(key, value);
}
```

The caller identifies itself by passing `thisIndex` in the request; the handler sends
the reply directly back. No central coordinator is needed, and no chare needs to
know in advance how many requests it will receive.

### Verification Without Transmitting Expected Values

Both the table builder and the query checker use the same deterministic function:

```cpp
static inline int tableValue(int key) { return key * 13 + 7; }
```

Because this function is compiled into every element, expected values never need to
be transmitted, and each chare can verify its own responses locally.

---

## What This Illustrates

**Asynchronous request-reply** is one of the most natural patterns in Charm++. Each
chare sends its K requests and immediately suspends in the `when` loop — it is not
blocked on each individual reply, and the scheduler runs other work (including serving
`request()` messages from other chares) in the meantime.

In MPI, implementing the same pattern without an all-to-all requires each process to
know how many `MPI_Recv` calls to issue, which in turn requires either a prior
announcement phase or a global coordination step. In Charm++, the `when` loop
consumes exactly K responses with no pre-announcement needed; the runtime buffers any
that arrive early.

---

## Running the Example

```
cd examples/dht
make
./charmrun ++local +p4 ++ppn 2 ./dht 8 100 20
./charmrun ++local +p4 ++ppn 2 ./dht 16 200 50
```

Parameters: `numChares`, `keysPerChare`, `queriesPerChare`.

Expected output (both runs):

```
160/160 queries verified correctly.  Errors: 0  OK
800/800 queries verified correctly.  Errors: 0  OK
```

---

## Exercise

Implement the DHT from scratch using `examples/dht/` as a reference only after you
have made an attempt.

1. Write the `.ci` file. Decide which entry methods are SDAG and which are plain.
2. In the `.C` file, implement `DHT()` to populate the table, `request()` to serve
   lookups, and the SDAG `start()` body.
3. Verify that all queries return correct values for several combinations of
   `numChares`, `keysPerChare`, and `queriesPerChare`.

**Stretch goal:** Add a second query type — for example, a range query that returns
the count of keys in `[lo, hi)`. How does the SDAG structure change, if at all?
