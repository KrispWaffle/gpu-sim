#!/usr/bin/env python3
"""Matrix multiply on the simulator.

Matmul works via reduction loops: each output element is one thread that loops
over the K dimension accumulating into its output slot (small K is unrolled).

Use explicit float data. `eye`/`arange`/`ones` build device-agnostic constants
that tinygrad realizes on CPU, so a bare `Tensor.eye(...) @ Tensor.eye(...)`
won't touch GSIM -- add `.to("GSIM").realize()` to force it onto the simulator.
"""
import _gsim_env  # noqa: F401
from tinygrad import Tensor

a = Tensor([[1.0, 2.0, 3.0],
            [4.0, 5.0, 6.0]])
b = Tensor([[1.0, 4.0],
            [2.0, 5.0],
            [3.0, 6.0]])
print("a @ b =", (a @ b).tolist())   # [[14, 32], [32, 77]]

# larger square matmul (uses a K-loop reduction under the hood)
def ref(A, B):
  n, k, m = len(A), len(B), len(B[0])
  return [[sum(A[i][p] * B[p][j] for p in range(k)) for j in range(m)] for i in range(n)]

N = 16
x = [[float((i + j) % 5) for j in range(N)] for i in range(N)]
y = [[float((i * j) % 4) for j in range(N)] for i in range(N)]
print(f"{N}x{N} matmul matches reference:", (Tensor(x) @ Tensor(y)).tolist() == ref(x, y))

# forcing a const-built matrix onto GSIM
eye = Tensor.eye(4).to("GSIM").realize()
m = Tensor([[1., 2., 3., 4.]] * 4).to("GSIM").realize()
print("eye @ m == m:", (eye @ m).tolist() == m.tolist())
