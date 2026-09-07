"""Shared setup for the GSIM examples -- `import _gsim_env` first in each script.

Points tinygrad at the in-repo clone and makes GSIM the default device, so the
examples run with a plain `python3 examples/<name>.py`.
"""
import os, sys, pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tinygrad"))                # use the in-repo tinygrad clone
os.environ["DEV"] = "GSIM"                      # make GSIM the default device
os.environ.setdefault("GSIM_LIB", str(REPO / "libgsim.so"))

if not pathlib.Path(os.environ["GSIM_LIB"]).exists():
  raise SystemExit(f"{os.environ['GSIM_LIB']} not found - build it with `make libgsim.so`")
