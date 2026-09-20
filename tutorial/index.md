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
| [Chapter 3](ch3_chare_arrays.md) | Chare arrays — indexed collections, element addressing, multiple arrays, modules |
| [Chapter 4](ch4_reductions.md) | Reductions and broadcasts — collective operations over an array, readonly variables |
| [Chapter 5](ch5_array_indices.md) | Array indices — multidimensional, sparse, the five-point stencil |
| [Chapter 6](ch6_sdag.md) | Structured Dagger (SDAG) — sequential notation for message-driven code |
| [Chapter 7](ch7_pup.md) | PUP — serializing objects for messages and migration |
| [Chapter 8](ch8_load_balancing.md) | Load balancing — AtSync, LB strategies, PUP for live variables |
| [Chapter 9](ch9_threaded.md) | Threaded entry methods — CkFuture, [sync], phase pattern |

The [DHT exercise](exercise_dht.md) provides additional practice between Chapters 6 and 7.

## Prerequisites

- Solid C++ knowledge (classes, pointers, templates)
- Basic familiarity with parallel computing concepts (processes, threads, communication)
- A working Charm++ installation — see *Getting started* below

## Getting started

### 1. Install Charm++

Follow the official instructions, which are kept current with each release:

- [Installing Charm++](https://charm.readthedocs.io/en/latest/quickstart.html#installing-charm)
  — the Quickstart: download a release and build it with `./build`. Start here.
- [Installing Charm++ (manual)](https://charm.readthedocs.io/en/latest/charm++/manual.html#installing-charm)
  — the full chapter, covering manual builds, CMake, Spack, and the options for
  specific network layers and platforms.

The build produces a directory named for your platform — `netlrts-linux-x86_64`,
`netlrts-darwin-arm8`, and so on — containing `bin/charmc`. That directory is what
the tutorial calls `CHARM_HOME`.

### 2. Get the tutorial examples

Every chapter is backed by complete, compilable programs. Clone them:

```bash
git clone https://github.com/charmplusplus/charm-tutorial.git
cd charm-tutorial
```

### 3. Build and run one example

Point `CHARM_HOME` at your Charm++ build directory and build the first example:

```bash
export CHARM_HOME=$HOME/charm/netlrts-linux-x86_64   # your build directory

cd examples/primes
make
./charmrun ++local +p4 ./primes 20
```

`primes` tests 20 randomly generated numbers for primality, one chare per number, and
prints a `prime` / `not prime` line for each before `End of program`. If you see that,
your toolchain is working and you are ready for Chapter 1.

If `make` cannot find `charmc`, `CHARM_HOME` is pointing at the wrong directory — it must be the *build* directory (the one named after your
platform), not the top of the Charm++ source tree.

## How to use this tutorial

Read each chapter in order. Every chapter introduces new concepts with short, complete
example programs in the `examples/` directory. Build and run the examples as you go —
seeing the output on your own machine is part of the learning.

Exercises at the end of each chapter are meant to be completed before moving on.
