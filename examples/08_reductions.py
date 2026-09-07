#!/usr/bin/env python3
"""Reductions on the simulator (sum/max/mean).

Each reduction lowers to a per-thread loop: init an accumulator in global
memory, loop over the reduce axis with cmp_lt + jmp, accumulate, store. Dump it
with GSIM_DUMP=ex to see the loop (it looks like ex/reduction.gsim).
"""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

x = Tensor([float(i) for i in range(100)])
print("sum :", x.sum().item())          # 4950
print("max :", x.max().item())          # 99
print("mean:", x.mean().item())         # 49.5

# reduce one axis of a 2D tensor
m = Tensor([[1.0, 2.0, 3.0],
            [4.0, 5.0, 6.0]])
print("col sums:", m.sum(axis=0).tolist())   # [5, 7, 9]
print("row sums:", m.sum(axis=1).tolist())   # [6, 15]
