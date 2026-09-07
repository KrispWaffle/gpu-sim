#!/usr/bin/env python3
"""Tests for the tinygrad GSIM backend (tinygrad/runtime/ops_gsim.py).

Runs tinygrad with DEV=GSIM against the headless libgsim.so and checks results
against pure-Python references. No external deps; run directly:

    make libgsim.so
    python3 test/test_gsim.py

It is also pytest-compatible (functions named test_*) if pytest is installed.
"""
import os, sys, pathlib, math

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tinygrad"))                       # use the in-repo tinygrad clone
os.environ["DEV"] = "GSIM"
os.environ.setdefault("GSIM_LIB", str(REPO / "libgsim.so"))

if not pathlib.Path(os.environ["GSIM_LIB"]).exists():
  raise RuntimeError(f"{os.environ['GSIM_LIB']} not found - build it first with `make libgsim.so`")

from tinygrad import Tensor, Device, dtypes  # noqa: E402

# --------------------------------------------------------------------------- helpers
def flat(x):
  if isinstance(x, list):
    out = []
    for e in x: out.extend(flat(e))
    return out
  return [x]

def assert_close(got, exp, tol=1e-4, ctx=""):
  g, e = flat(got), flat(exp)
  assert len(g) == len(e), f"{ctx}: length {len(g)} != {len(e)}"
  for i, (a, b) in enumerate(zip(g, e)):
    assert math.isclose(a, b, rel_tol=1e-6, abs_tol=tol), f"{ctx}: index {i}: {a} != {b}\n got={g[:8]}\n exp={e[:8]}"

SIZES = [1, 2, 3, 4, 7, 16, 31, 64, 255, 256, 1023, 1024, 4096]

# --------------------------------------------------------------------------- tests
def test_device_registered():
  assert Device.DEFAULT == "GSIM"
  assert Tensor([1.0], device="GSIM").device == "GSIM"
  assert "GSIM" in Device._devices
  dev = Device["GSIM"]
  assert type(dev).__name__ == "GSIMDevice"
  assert type(dev.renderer).__name__ == "GSIMRenderer"

def test_elementwise_binary():
  for n in SIZES:
    a = [float(i + 1) for i in range(n)]
    b = [float((i % 5) + 1) for i in range(n)]    # nonzero divisor
    ta, tb = Tensor(a), Tensor(b)
    assert_close((ta + tb).tolist(), [a[i] + b[i] for i in range(n)], ctx=f"add n={n}")
    assert_close((ta - tb).tolist(), [a[i] - b[i] for i in range(n)], ctx=f"sub n={n}")
    assert_close((ta * tb).tolist(), [a[i] * b[i] for i in range(n)], ctx=f"mul n={n}")
    assert_close((ta / tb).tolist(), [a[i] / b[i] for i in range(n)], ctx=f"div n={n}")

def test_unary_neg():
  for n in [1, 5, 64, 1024]:
    a = [float(i - 3) for i in range(n)]
    assert_close((-Tensor(a)).tolist(), [-x for x in a], ctx=f"neg n={n}")

def test_scalar_and_chains():
  for n in [3, 16, 257, 1024]:
    a = [float(i + 1) for i in range(n)]
    ta = Tensor(a)
    assert_close((ta * 3.0 - 1.0).tolist(), [x * 3.0 - 1.0 for x in a], ctx=f"scal n={n}")
    assert_close(((ta + 1.0) * (ta - 1.0)).tolist(), [(x + 1) * (x - 1) for x in a], ctx=f"poly n={n}")
    tb = Tensor([2.0] * n)
    assert_close((ta * tb + ta).tolist(), [x * 2.0 + x for x in a], ctx=f"fma n={n}")

def test_multidim():
  a2 = [[1., 2., 3.], [4., 5., 6.]]
  b2 = [[10., 20., 30.], [40., 50., 60.]]
  assert_close((Tensor(a2) + Tensor(b2)).tolist(), [[11, 22, 33], [44, 55, 66]], ctx="2d add")
  a3 = [[[float(i + 2 * j + 4 * k) for i in range(2)] for j in range(2)] for k in range(2)]
  exp = [[[v * 2 for v in row] for row in plane] for plane in a3]
  assert_close((Tensor(a3) * 2.0).tolist(), exp, ctx="3d mul")

def test_buffer_reuse():
  a = [float(i) for i in range(50)]
  ta = Tensor(a)
  assert_close((ta + ta).tolist(), [2 * x for x in a], ctx="a+a")
  assert_close((ta * ta).tolist(), [x * x for x in a], ctx="a*a")

def test_large_multi_axis():
  # large sizes force tinygrad to use both global+local axes -> exercises tidx decomposition
  for n in [1024, 4096]:
    a = [float(i) for i in range(n)]
    assert_close((Tensor(a) * -2.0).tolist(), [x * -2.0 for x in a], ctx=f"large n={n}")

def test_rerun_caches():
  a = Tensor([float(i) for i in range(50)])
  r1 = (a + a).tolist()
  r2 = (a + a).tolist()    # second realize hits the program cache
  assert r1 == r2

def test_compare_select():
  a = [1., 2., 3., 4., 5., 6., 7., 8.]
  b = [8., 7., 6., 5., 4., 3., 2., 1.]
  ta, tb = Tensor(a), Tensor(b)
  assert_close((ta < tb).float().tolist(), [float(a[i] < b[i]) for i in range(8)], ctx="cmplt")
  assert_close((ta == tb).float().tolist(), [float(a[i] == b[i]) for i in range(8)], ctx="cmpeq")
  assert_close((ta != tb).float().tolist(), [float(a[i] != b[i]) for i in range(8)], ctx="cmpne")
  assert_close((ta < tb).where(ta, tb).tolist(), [min(a[i], b[i]) for i in range(8)], ctx="where")
  assert_close(ta.maximum(tb).tolist(), [max(a[i], b[i]) for i in range(8)], ctx="maximum")
  c = [float(i - 32) for i in range(64)]                  # exercises multi-axis + branch divergence
  assert_close(Tensor(c).relu().tolist(), [max(0., x) for x in c], ctx="relu")

def test_reductions():
  for n in [8, 64, 100, 512]:
    x = [float(i % 7) for i in range(n)]
    assert math.isclose(Tensor(x).sum().item(), sum(x), abs_tol=1e-2), f"sum n={n}"
  a = [float(i % 6) for i in range(12)]
  assert math.isclose(Tensor(a).max().item(), max(a), abs_tol=1e-4), "max"
  assert math.isclose(Tensor(a).mean().item(), sum(a) / len(a), abs_tol=1e-3), "mean"
  big = [float((i * 7) % 101) for i in range(200)]   # large -> looped max (acc inits to -inf)
  assert math.isclose(Tensor(big).max().item(), max(big), abs_tol=1e-4), "looped max"

def test_matmul():
  def mm(A, B):
    n, k, m = len(A), len(B), len(B[0])
    return [[sum(A[i][p] * B[p][j] for p in range(k)) for j in range(m)] for i in range(n)]
  for N in [2, 4, 8, 16, 32]:
    A = [[float((i + j) % 5) for j in range(N)] for i in range(N)]
    B = [[float((i * j) % 4) for j in range(N)] for i in range(N)]
    assert_close((Tensor(A) @ Tensor(B)).tolist(), mm(A, B), ctx=f"matmul {N}x{N}")
  A = [[1., 2., 3.], [4., 5., 6.]]; B = [[1., 4.], [2., 5.], [3., 6.]]   # non-square
  assert_close((Tensor(A) @ Tensor(B)).tolist(), mm(A, B), ctx="matmul 2x3@3x2")

def test_unsupported_ops_raise():
  for name, fn in [("sqrt", lambda: Tensor([1., 4., 9.]).sqrt().tolist()),
                   ("exp", lambda: Tensor([1., 2.]).exp().tolist()),
                   ("randint", lambda: Tensor.randint(8, low=0, high=5).tolist())]:
    try:
      fn()
    except NotImplementedError:
      continue
    raise AssertionError(f"expected {name} to raise (unsupported by GSIM backend)")

def raises(exc, text, fn):
  try: fn()
  except exc as e:
    assert text in str(e), str(e)
  else: raise AssertionError(f"expected {exc.__name__}: {text}")

def test_small_constants():
  for c in [1e-5, -1e-7, 1e-30, 2**-149]:
    x = Tensor([1., 2., 4.], device="GSIM") * c
    assert x.device == "GSIM"
    assert_close(x.tolist(), [c, 2*c, 4*c], tol=0, ctx=f"constant {c}")

def test_special_selection():
  gate = Tensor([0., 1., 0., 1.], device="GSIM") > 0.5
  a = Tensor([math.nan, 3., math.inf, -math.inf], device="GSIM")
  b = Tensor([2., math.nan, 4., math.inf], device="GSIM")
  assert gate.where(a, b).tolist() == [2., 3., 4., -math.inf]
  x = Tensor([-math.inf, math.inf, -3., 5.], device="GSIM")
  y = Tensor([2., 4., -math.inf, math.inf], device="GSIM")
  assert x.maximum(y).tolist() == [2., math.inf, -3., math.inf]

def test_safe_padding():
  x = Tensor([2., 3., 5.], device="GSIM")
  assert_close((x.pad(((2, 3),)) + 1.).tolist(), [1., 1., 3., 4., 6., 1., 1., 1.])
  assert_close((x.pad(((1, 2),), value=7.) * 2.).tolist(), [14., 4., 6., 10., 14., 14.])

def test_changed_input_reuse():
  from tinygrad.runtime.ops_gsim import GSIMProgram
  p = GSIMProgram("GSIM", "reuse", b"; gsim regs=1 bufs=2\nld r0, gm[__BUF1__]\nadd r0, r0, 1.0\nst gm[__BUF0__], r0\nhalt\n")
  out, inp = memoryview(bytearray(4)), memoryview(bytearray(4))
  for v in [3., -8., 19.]:
    inp.cast('f')[0] = v
    p(out, inp, local_size=None)
    assert out.cast('f')[0] == v + 1

def test_dtype_contract():
  for dt in (dtypes.int32, dtypes.float64, dtypes.float16):
    raises(NotImplementedError, "GSIM", lambda: (Tensor([1, 2, 3, 4], device="GSIM", dtype=dt) + 1).tolist())
  x = Tensor([1.5, -2.5, 0., 4.], device="GSIM")
  raises(NotImplementedError, "GSIM", lambda: x.cast(dtypes.int32).tolist())
  raises(NotImplementedError, "GSIM", lambda: x.bitcast(dtypes.int32).tolist())
  x = Tensor([1.5, -2.5, 0., 4.], device="GSIM")
  assert_close(x.cast(dtypes.bool).float().tolist(), [1., 1., 0., 1.])
  assert_close((Tensor.arange(4).to("GSIM").float() + x).tolist(), [1.5, -1.5, 2., 7.])

def test_gated_load_valid_address():
  from tinygrad.uop.ops import UOp, Ops
  from tinygrad.runtime.ops_gsim import GSIMProgram
  p = UOp.param(0, dtypes.float32, (1,))
  zero = UOp.const(dtypes.int32, 0)
  idx = p.index(zero)
  fallback, gate = UOp.const(dtypes.float32, 7.), UOp.const(dtypes.bool, False)
  load = UOp(Ops.LOAD, dtypes.float32, (idx, fallback, gate))
  store = idx.store(load)
  src = Device["GSIM"].renderer.render([p, zero, idx, fallback, gate, load, store])
  buf = memoryview(bytearray(4)); buf.cast('f')[0] = 3.
  GSIMProgram("GSIM", "gate", src.encode())(buf)
  assert buf.cast('f')[0] == 7.

def test_runtime_contract():
  from tinygrad.runtime.ops_gsim import GSIMProgram
  from tinygrad.device import BufferSpec
  alloc = Device["GSIM"].allocator
  raises(ValueError, "multiple of four", lambda: alloc._alloc(3, None))
  raises(ValueError, "allocation options", lambda: alloc._alloc(4, BufferSpec(external_ptr=1)))
  raises(NotImplementedError, "views", lambda: alloc._offset(memoryview(bytearray(8)), 4, 0))
  raises(NotImplementedError, "mapping", lambda: alloc._map(memoryview(bytearray(4))))
  p = GSIMProgram("GSIM", "contract", b"; gsim regs=1 bufs=2\nhalt\n")
  b = memoryview(bytearray(4))
  raises(NotImplementedError, "aliases", lambda: p(b, b))
  raises(NotImplementedError, "scalar", lambda: p(vals=(1,)))
  raises(ValueError, "expects", lambda: p(b))

def test_renderer_contract():
  from tinygrad.uop.ops import UOp, Ops
  r = Device["GSIM"].renderer
  for op in (Ops.CAST, Ops.BITCAST):
    x = UOp.const(dtypes.float32, 1.5)
    u = UOp(op, dtypes.int32, (x,))
    raises(NotImplementedError, "GSIM", lambda: r.render([x, u]))
  p = UOp.param(0, dtypes.float32, ())
  raises(NotImplementedError, "scalar", lambda: r.render([p]))

def test_launch_modes():
  import subprocess
  for mode in (0, 1):
    env = dict(os.environ, NOLOCALS=str(mode), PYTHONDONTWRITEBYTECODE="1", CACHELEVEL="0")
    code = "from tinygrad import Tensor; x=Tensor([float(i) for i in range(257)], device='GSIM'); y=x*2.; assert y.device=='GSIM'; assert y.tolist()==[float(i*2) for i in range(257)]"
    subprocess.run([sys.executable, "-c", code], cwd=REPO, env=dict(env, PYTHONPATH=str(REPO / "tinygrad")), check=True)

# --------------------------------------------------------------------------- runner
if __name__ == "__main__":
  tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_") and callable(v)]
  failed = 0
  for name, fn in tests:
    try:
      fn()
      print(f"PASS {name}")
    except Exception as e:
      failed += 1
      print(f"FAIL {name}: {e}")
  print(f"\n{len(tests) - failed}/{len(tests)} passed")
  sys.exit(1 if failed else 0)
