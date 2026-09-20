# Charm++ Tutorial

A hands-on introduction to parallel programming with **Charm++**, written by
Laxmikant V. Kale (University of Illinois Urbana-Champaign).

Charm++ is a C++-based parallel programming system built around the idea that
the programmer decomposes the problem into objects (*chares*) and the runtime
system handles mapping them to processors, load balancing, and communication.
This separation lets programs adapt at runtime to heterogeneous and changing
hardware.

## What this tutorial covers

| Chapter | Topic |
|---------|-------|
| [Chapter 1](ch1_introduction.md) | The XMAPP model — overdecomposition, asynchrony, migration, adaptivity |
| [Chapter 2](ch2_basic_chares.md) | Chares, proxies, entry methods, grainsize |
| [Chapter 3](ch3_sdag.md) | Structured Dagger (SDAG) — sequential notation for message-driven code |
| [Chapter 4](ch4_pup.md) | PUP — serializing objects for messages and migration |
| [Chapter 5](ch5_load_balancing.md) | Load balancing — AtSync, LB strategies, PUP for live variables |
| [Chapter 6](ch6_threaded.md) | Threaded entry methods — CkFuture, [sync], phase pattern |

The [DHT exercise](exercise_dht.md) provides additional practice between Chapters 3 and 4.

## Prerequisites

- Solid C++ knowledge (classes, pointers, templates)
- Basic familiarity with parallel computing concepts (processes, threads, communication)
- A working Charm++ installation

## How to use this tutorial

Read each chapter in order. Every chapter introduces new concepts with short, complete
example programs in the `examples/` directory. Build and run the examples as you go —
seeing the output on your own machine is part of the learning.

Exercises at the end of each chapter are meant to be completed before moving on.
