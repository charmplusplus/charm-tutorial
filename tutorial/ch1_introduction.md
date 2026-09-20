# Chapter 1: Introduction and the XMAPP Model

## Why Parallel Computing?

The free lunch from sequential hardware improvements ended around 2003. Clock frequencies
stopped rising because chips would generate too much heat. Since then, silicon engineers have
responded by putting more cores on a chip rather than making individual cores faster. The
result: parallelism is no longer a specialty — it is the default mode of modern hardware.
Even a laptop has 8 or more cores. Supercomputers today have millions.

This shift means that extracting performance from hardware requires parallel programming.
But parallel programming is hard. The central challenge is coordination: how do you divide
work across many processors, keep them busy, and let them communicate — without the
programmer having to micromanage every detail?

## The XMAPP Model

Charm++ is built on a parallel programming model called **XMAPP**. Rather than a new
language, XMAPP is a set of principles for structuring parallel interactions. Sequential code
stays in C++; XMAPP governs how parallel objects are decomposed, mapped, and scheduled.

XMAPP rests on three complementary ideas: **overdecomposition**,
**asynchronous message-driven execution**, and **migratability**.

---

### Overdecomposition

In traditional parallel programming (such as MPI), the programmer divides the problem
into exactly as many pieces as there are processors. This ties the decomposition to the
hardware: change the processor count and you may need to restructure the program.

XMAPP takes a different approach. The programmer decomposes the problem into
**more pieces than there are processors** — typically many more. These pieces are called
*work units*. The runtime system then maps work units to processors and schedules their
execution.

```
User's view                    Runtime's view
-----------                    --------------
  A[0]  A[1]  A[2]              Processor 0    Processor 1
  B[0]  B[1]                    A[0] A[2] B[1]   A[1] B[0]
  C[0]  C[1]  C[2]  C[3]        C[0] C[2]        C[1] C[3]
```

The programmer writes the program entirely in terms of work units. No programmer-written
code coordinates which units live on which processor. That mapping is the runtime's job.

This separation has several consequences:

- **Communication-computation overlap**: when one work unit is waiting for data, another
  on the same processor can run.
- **Modularity**: independent modules (e.g., a fluid solver and a solid solver in a rocket
  simulation) can each be decomposed naturally, and the runtime interleaves them.
- **Adaptivity**: because work units can be moved, the runtime can rebalance load as
  execution proceeds.

---

### Asynchronous Message-Driven Execution

Work units interact by sending **messages**. Sending a message in XMAPP is
non-blocking: the sender does not wait for the recipient to act. This is the key distinction
from a function call or RPC.

A useful analogy: a function call is like a phone call — the caller waits on the line until
the recipient answers and replies. An XMAPP message is like an email — the sender sends
it and immediately returns to other work. The recipient processes the message at its leisure.

On each processor, the runtime maintains a **scheduler** with a queue of pending
messages. After a work unit finishes processing one message, the scheduler picks the next
message from the queue and dispatches it. The programmer never writes a scheduling loop;
the runtime drives execution entirely through message arrivals.

This style is called *message-driven execution*. Combined with overdecomposition — many
work units per processor — it lets the runtime overlap communication with computation
automatically: while one work unit waits for a message to arrive, others are already in the
queue ready to execute.

---

### Migratability

Because work units are not tied to specific processors, the runtime can **migrate** them
— move them from one processor to another — at any point during execution. The runtime
knows where every work unit lives (via a location management service), so messages are
always delivered to the right place even after migration.

Migratability is what makes the following runtime services possible without programmer
effort:

- **Dynamic load balancing**: if some processors are idle while others are overloaded, the
  runtime moves work units to equalize the load.
- **Fault tolerance**: a work unit's state can be checkpointed and restored on a replacement
  processor.
- **Malleability**: the job can shrink or expand (return or acquire processors) at runtime.

---

## The Adaptive Runtime System

The three pillars together give the runtime system two powerful capabilities:

**Introspection** — because the runtime mediates all message sends and schedules all
entry method invocations, it can observe exactly how much computation each work unit
does and who communicates with whom. This information is gathered automatically, without
any instrumentation code from the programmer.

**Adaptivity** — armed with introspection data, the runtime can act: migrate work units
for load balance, co-locate heavily communicating units, prefetch data, or reorder message
execution by priority.

The programmer provides what only they can know: how to decompose the problem domain.
The runtime provides what only it can know: the state of the hardware and the execution.
This division of labor is the core design philosophy of XMAPP.

---

## XMAPP Instantiations

XMAPP has been implemented in several languages. This tutorial focuses on **Charm++**,
the C++ implementation. Other implementations include Charm4Py (Python) and AMPI
(which allows existing MPI programs to benefit from overdecomposition and migratability
without rewriting).

---

## Benefits Summary

The table below summarizes the main benefits of Charm++ and their sources:

| Benefit | Source |
|---------|--------|
| Communication-computation overlap | Overdecomposition + async execution |
| Automatic dynamic load balancing | Migratability + introspection |
| Compositionality of modules | Message-driven scheduling |
| Prioritized execution | Scheduler queue reordering |
| Checkpointing and fault tolerance | Migratability |
| Malleability (shrink/expand) | Migratability |

One contrast with MPI-style bulk-synchronous programs is worth highlighting. In a typical
MPI program, processors compute for a while, then all communicate at once, then compute
again. The network sits idle during the compute phase and becomes a bottleneck during the
communication phase. With Charm++, many chares per processor means communication
is spread across the entire execution and overlapped with computation — the network is
used more uniformly and is rarely on the critical path.
