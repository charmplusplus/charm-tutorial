# Charm++ Tutorial

A hands-on introduction to parallel programming with [Charm++](https://charmplusplus.org),
by Laxmikant V. Kale (University of Illinois Urbana-Champaign).

**Read it online: https://charmplusplus.org/charm-tutorial/**

## Contents

| Chapter | Topic |
|---------|-------|
| 1 | Introduction — the XMAPP model |
| 2 | Simple programs and basic chares |
| 3 | Structured Dagger (SDAG) |
| — | Exercise: distributed hash table |
| 4 | PUP — serializing objects for messages and migration |
| 5 | Load balancing |
| 6 | Threaded entry methods |

Chapter text lives in `tutorial/`. Every chapter is backed by complete, compilable
example programs in `examples/`; build and run them as you read.

## Building the example programs

Each example directory has its own `Makefile`. Point `CHARM_HOME` at your Charm++
build directory — the one named for your platform, containing `bin/charmc` — and
run `make`:

    export CHARM_HOME=$HOME/charm/netlrts-linux-x86_64
    cd examples/primes
    make
    ./charmrun ++local +p4 ./primes 20

See [Getting started](https://charmplusplus.org/charm-tutorial/#getting-started)
for installing Charm++ itself.

## Building the website

    pip install mkdocs mkdocs-material
    mkdocs serve      # live preview at http://127.0.0.1:8000
    mkdocs build      # static HTML into site/
    mkdocs gh-deploy  # publish to the gh-pages branch
