# PIM Vectorization Pass

This is an LLVM pass that converts vectorizable instructions into their PIM equivalent which can be used by the gem5 simulator.

Build:

    $ mkdir cmake-build-debug
    $ cd cmake-build-debug
    $ cmake ..
    $ make
    $ cd ..

Run:

    $ cd workloads
    $ make {WORKLOAD}

To compile a custom workload, look at workloads/Makefile to create a custom makefile. Note that applications have to be linked before the pass is run.
