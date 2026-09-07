#!/usr/bin/env python3
"""A 1024-element kernel; tinygrad parallelizes it across simulator threads."""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

x = Tensor([float(i) for i in range(1024)])
out = (x * 2.0 + 1.0).tolist()
print("first 4:", out[:4])
print("last 4 :", out[-4:])
