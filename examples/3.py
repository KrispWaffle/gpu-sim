#!/usr/bin/env python3
"""Scalar arithmetic and reciprocal broadcast over a tensor."""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

x = Tensor([float(i) for i in range(8)])
print("x*3 - 1   =", (x * 3.0 - 1.0).tolist())
print("1 / (x+1) =", (1.0 / (x + 1.0)).tolist())
