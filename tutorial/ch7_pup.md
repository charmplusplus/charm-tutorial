# Chapter 7: PUP — Packing and Unpacking

Two things in Charm++ require serializing an object's state:

1. **Passing a user-defined type as an entry method parameter.** Charm++ needs to pack
   the value into a message, send it across processes, and unpack it on the other side.
2. **Migrating a chare.** When the load balancer moves a chare to a different PE, Charm++
   must serialize the chare's entire state, transfer it, and reconstruct it.

Both use exactly the same mechanism: the **PUP framework** (Pack/UnPack). If you write
a `pup()` method for a type, that type can be used as an entry method parameter *and*
as the basis for chare migration.

---

## The `pup()` Method

The `pup()` method has this signature:

```cpp
void pup(PUP::er& p) {
    p | member1;
    p | member2;
    // ... one line per member
}
```

The `PUP::er` object `p` is polymorphic: depending on context it may be sizing, packing,
or unpacking. The `p | x` operator does the right thing in all three cases. You write the
method once; Charm++ calls it in whichever mode is needed.

**Every member must appear in `pup()`.** A missing member means garbage or a crash after
migration or deserialization.

---

## What `p | x` Handles

`p | x` works directly for:

| Category | Examples |
|---|---|
| Primitive types | `int`, `double`, `float`, `bool`, `char`, `unsigned`, ... |
| Enums | Any `enum` or `enum class` |
| Charm++ proxies | `CProxy_Foo`, `CkCallback` |
| Fixed-size nested structs | Any type that itself has `void pup(PUP::er&)` |

For STL containers, include `pup_stl.h` and then `p | x` works for:
`std::vector`, `std::string`, `std::list`, `std::set`, `std::map`, `std::pair`.

```cpp
#include "pup_stl.h"

void pup(PUP::er& p) {
    p | myVector;     // std::vector<double>
    p | myString;     // std::string
    p | myMap;        // std::map<int, double>
}
```

---

## C Arrays: `PUParray`

For a raw C array of fixed size `n`:

```cpp
PUParray(p, arr, n);
```

If the array is heap-allocated and its size is stored in a member variable, you must
pack the size first, allocate on unpack, then pack the contents:

```cpp
void pup(PUP::er& p) {
    p | n;
    if (p.isUnpacking()) arr = new double[n];
    PUParray(p, arr, n);
}
```

`p.isUnpacking()` returns `true` only during reconstruction. The three query methods are:

| Method | True when |
|---|---|
| `p.isPacking()` | serializing to send or write |
| `p.isUnpacking()` | reconstructing from bytes |
| `p.isSizing()` | computing the buffer size (called before packing) |

---

## Shortcut for Plain Fixed-Size Structs: `PUPbytes`

For a struct that contains only primitive fields (no pointers, no STL containers, no
virtual methods), you can skip writing `pup()` field-by-field and instead tell Charm++
to copy the raw bytes:

```cpp
struct Point {
    double x, y;
};

void pup(PUP::er& p) {
    PUPbytes(p, pt, sizeof(pt));   // pack/unpack the entire Point as raw bytes
}
```

`PUPbytes` is a macro that expands to `p.bytes(&x, n)`. It is equivalent to writing
`p | pt.x; p | pt.y;` but requires no changes if you add more plain fields later.

**When not to use it:** if the struct contains a pointer (the address would be packed,
not the pointed-to data), a `std::string` or any container, or virtual methods (the
vtable pointer is implementation-defined). For those types, write `pup()` field-by-field.

---

## Using Custom Types as Entry Method Parameters

Any type with a `pup()` method can be used as a parameter to an entry method. Declare
the entry method normally in the `.ci` file:

```cpp
entry void processRecord(Record r);
```

The `Record` struct needs two things: a `pup()` method and a default constructor (used
during unpacking).

```cpp
struct Record {
    int id;
    double value;
    std::string label;

    Record() {}   // required for unpacking

    void pup(PUP::er& p) {
        p | id;
        p | value;
        p | label;
    }
};
```

The struct definition (and its `#include "pup_stl.h"`) must be visible to both the
`.C` file and the `.ci` file (put it in a shared header).

---

## Chare Migration and `pup()`

When Charm++ migrates a chare, it calls `pup()` on the chare object itself to serialize
its state, transfers the bytes, and reconstructs the chare on the destination PE. The
same rules apply: every member must appear in `pup()`, and STL containers work with
`pup_stl.h`.

Two additional requirements for a migratable chare:

**1. A migration constructor:**

```cpp
MyChare(CkMigrateMessage* m) {}
```

This constructor is called on the destination PE before `pup()` unpacks the state. It
should do nothing — just exist.

**2. Enable AtSync:**

If you want the chare to participate in load balancing (i.e., to be migrated at load
balance points), set this in the constructor:

```cpp
usesAtSync = true;
```

The load balancing chapter covers AtSync and migration in detail.

---

## Summary

PUP is a single mechanism that serves two purposes: serializing entry method parameters
and serializing chares for migration.

Write a `pup(PUP::er& p)` method that visits every member with `p | member`. Include
`pup_stl.h` for STL containers, use `PUParray` for C arrays, and use `p.isUnpacking()`
when you need conditional logic during reconstruction.

The key points:

1. `p | x` — pack/unpack `x`; handles primitives, enums, proxies, and PUP-able types.
2. `#include "pup_stl.h"` — enables `p | x` for `std::vector`, `std::string`,
   `std::map`, and other STL containers.
3. `PUParray(p, arr, n)` — for C arrays of length `n`.
4. `PUPbytes(p, x, sizeof(x))` — raw-byte copy for plain fixed-size structs with no
   pointers or containers. Do not use if the struct contains pointers or virtual methods.
5. `p.isUnpacking()` — use to guard allocation-on-reconstruct logic.
6. A migratable chare needs a `pup()` method, a `MyChare(CkMigrateMessage*)` constructor,
   and `usesAtSync = true` to participate in load balancing.
7. Any struct with `pup()` and a default constructor can be an entry method parameter.
