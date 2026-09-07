#!/usr/bin/env python3
"""Matmul on the GSIM simulator.

Use explicit (non-constant) data. eye/arange/ones/randint build compile-time
CONSTANTS that tinygrad folds on the host -- eye @ eye even simplifies to eye --
so NO GSIM kernel is generated and it looks like it "runs on CPU". Concrete data
forces a real matmul kernel onto GSIM (DEV=GSIM, set by _gsim_env).
"""
import _gsim_env  # noqa: F401
from tinygrad import Tensor
import random

random.seed(0)
N = 128
A = [[float(random.randint(0, 5)) for _ in range(N)] for _ in range(N)]
B = [[float(random.randint(0, 5)) for _ in range(N)] for _ in range(N)]

out = (Tensor(A) @ Tensor(B)).tolist()
ref = [[sum(A[i][p] * B[p][j] for p in range(N)) for j in range(N)] for i in range(N)]
print("result:", out)
print("matches reference:", out == ref)
