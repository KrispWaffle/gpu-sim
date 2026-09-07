#!/usr/bin/env python3
"""a*b + c evaluated as a single fused kernel."""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

a = Tensor([1.0, 2.0, 3.0, 4.0])
b = Tensor([10.0, 20.0, 30.0, 40.0])
c = Tensor([100.0, 100.0, 100.0, 100.0])
print("a*b + c =", (a * b + c).tolist())   # [110, 140, 190, 260]
