#!/usr/bin/env python3
"""Elementwise ops on a 2D tensor (shapes are flattened for the simulator)."""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

m = Tensor([[1.0, 2.0], [3.0, 4.0]])
print("m + m =", (m + m).tolist())   # [[2, 4], [6, 8]]
print("m * 2 =", (m * 2.0).tolist()) # [[2, 4], [6, 8]]
