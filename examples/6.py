#!/usr/bin/env python3
"""Print the .gsim assembly tinygrad generates for a kernel.

`__BUFk__` is the base offset of buffer k in the simulator's flat global memory
(filled in at launch time). Tiny kernels are fully unrolled into one thread;
larger ones parallelize, with `tidx` as the per-thread id.
"""
import _gsim_env  # noqa: F401
from tinygrad import Tensor
import tinygrad.runtime.ops_gsim as gsim


def show(label, build):
  captured = {}
  orig = gsim.GSIMProgram.__call__
  def hook(self, *a, **kw):
    captured["src"] = self.src
    return orig(self, *a, **kw)
  gsim.GSIMProgram.__call__ = hook
  try:
    result = build().tolist()
  finally:
    gsim.GSIMProgram.__call__ = orig
  print(f"\n=== {label} ===")
  print(captured["src"].rstrip())
  print("result:", result)


show("4-elem add (unrolled into 1 thread)",
     lambda: Tensor([1., 2., 3., 4.]) + Tensor([5., 6., 7., 8.]))
show("32-elem add (parallel: tidx is the per-thread id)",
     lambda: Tensor([float(i) for i in range(32)]) + Tensor([float(i) for i in range(32)]))
