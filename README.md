# Clairvoyance

Clairvoyance provides passes for instruction scheduling targeting long latency loads.
It is currently being developed at Uppsala University.

## Benchmarks
The NPB benchmark suite is available under
http://aces.snu.ac.kr/software/snu-npb/.  In order to reproduce the
experiments from the paper, you will need to 1) download the sources and
add them to their respective directories (see
experiments/swoop/sources/CG/src, for example) and 2) mark the loops to
target. We specify the functions to target in our paper [1]. 

[1] Clairvoyance: Look-ahead Compile-Time Scheduling

