# `.gsim` Language Reference

Programs are plain text files loaded at runtime. Lines beginning with `;` are comments. Opcodes and keywords are case-insensitive.

---

## Memory spaces

| Space | Keyword | Backed by | Size |
|-------|---------|-----------|------|
| Global | `global` | `GPU::global_memory` | `GLOBAL_MEM_SIZE` (default 10) |
| Shared | `shared` | `Warp::memory` | `WARP_SIZE` (default 10) |
| Local | `local` | `Thread::_registers` | `NUM_REGISTERS` (default 4) |

Global memory is shared across all threads. Shared memory is shared within a warp. Local memory (registers) is private to each thread.

---

## Operands

| Form | Example | Meaning |
|------|---------|---------|
| Register | `r0`, `r3` | Local register N |
| Register (thread-indexed) | `rTIDX` | Register at index = this thread's ID |
| Global memory | `gm0`, `gm3` | Global memory slot N |
| Global (thread-indexed) | `gmTIDX` | Global slot at index = thread ID |
| Shared memory | `sm0` | Shared memory slot N |
| Numeric literal | `3.0`, `-1`, `0.5` | Constant float value |
| Variable name | `x`, `i` | Named variable (resolved via `VarTable`) |

---

## `def` — declare a variable

```
def <loc> <name> = <value> [tidx] [@rN]
```

Declares a named variable and writes its initial value into memory.

- `<loc>` — `shared`, `global`, or `local`
- `<name>` — identifier used to reference the variable in instructions
- `<value>` — numeric literal (initial value)
- `tidx` (optional) — use the thread's ID as the memory slot index (gives each thread its own slot)
- `@rN` (optional) — use register N as the slot index

```
def global i  = 0 tidx   ; each thread gets its own global slot
def local  z  = 10 @ r2  ; store in register 2 (not thread-indexed)
def shared x  = 0 tidx   ; each thread gets its own shared slot
```

Only the first thread to execute a `def` registers the variable name. All threads still write their own slot, so thread-indexed vars work correctly across all threads.

---

## Arithmetic & bitwise opcodes

All take the form `op dst, src1, src2` — result stored in `dst`.

| Opcode | Operation |
|--------|-----------|
| `add dst, a, b` | `dst = a + b` |
| `sub dst, a, b` | `dst = a - b` |
| `mul dst, a, b` | `dst = a * b` |
| `div dst, a, b` | `dst = a / b` (errors on divide-by-zero) |
| `and dst, a, b` | `dst = (int)a & (int)b` |
| `or  dst, a, b` | `dst = (int)a \| (int)b` |
| `xor dst, a, b` | `dst = (int)a ^ (int)b` |

Bitwise ops truncate to `int` before operating, result stored as `float`.

---

## Other opcodes

| Opcode | Syntax | Description |
|--------|--------|-------------|
| `neg` | `neg dst, src` | `dst = -src` |
| `mov` | `mov dst, src` | `dst = src` |
| `ld`  | `ld  dst, src` | Copy `src` into `dst` (alias for `mov`) |
| `st`  | `st  dst, src` | Copy `src` into `dst` (alias for `mov`) |
| `halt` | `halt` | Marks this thread as inactive (stops it) |

---

## Control flow

Control flow is a two-instruction sequence: `cmp_lt` sets a predicate, then `jmp` acts on it.

### `cmp_lt a, b`
Compares `a < b`. Sets the thread's predicate register to `1` (true) or `0` (false). Does **not** jump.

### `jmp label`
Jumps to `label` if the predicate register is `1`. Falls through if `0`.

```
loop:
    add  i, i, 1.0
    cmp_lt i, z     ; predicate = (i < z)
    jmp  loop       ; jump back if true
    halt            ; reached when i >= z
```

Labels are declared as `name:` on their own line. They are resolved before execution — forward jumps work. Labels do not cost a cycle.

---

Constants in `src/include/config.hpp`:

| Constant | Default | Meaning |
|----------|---------|---------|
| `NUM_THREADS` | 10 | Threads per warp |
| `NUM_REGISTERS` | 4 | Local registers per thread (`r0`–`r3`) |
| `GLOBAL_MEM_SIZE` | 10 | Global memory slots |
| `WARP_SIZE` | 10 | Shared memory slots |
| `DELAY_TIME` | 50 ms | Pause between cycles |
