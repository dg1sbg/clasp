# Clasp SC598 — M0 Findings

**Date:** 2026-05-30 · **Branch:** `feat/sc598-yocto-port`

## Decision: **GO**

aarch64-linux Clasp **builds, boots, and bytecode-compiles**, and there are **working
deployable-image paths**: the base image, a snapshot file (`iclasp --snapshot`), and — after
the fix below — a **standalone executable snapshot**. One genuine upstream aarch64-linux bug
was found, root-caused, **fixed, and verified end-to-end** (patch captured under `patches/`).

## Environment

- Host: Apple **M5**, 32 GiB, macOS 26.5; Lima 2.1.1 `vz` (native, no emulation).
- Guest: Ubuntu **24.04.4 LTS** aarch64, kernel 6.8, **PAGESIZE 4096**, glibc (no musl), 8 vCPU / 16 GiB.
- Toolchain: clang/llvm **18.1.3**, SBCL 2.2.9, ninja 1.11.1, GNU `objcopy`, `ld.gold`.
- Clasp: upstream `clasp-developers/clasp` @ **2d206cc9b** (clasp-2.7.0-834).
- koga line: `./koga --build-mode=bytecode --no-default-native` (rc=0, no aarch64-linux config errors).

## Build result

- **C++ runtime compiles clean on aarch64-linux** — 532 objects per variant, incl. arch-sensitive
  `boehmGarbageCollection.cc`, `gc_boot.cc`, `llvmo/*`. **No source changes needed.**
- Built variants (lean, not all 6): `boehm` (conservative) and `boehmprecise` (precise).
- `iclasp` links and runs. The vendored bdwgc/libatomic_ops cross-built fine (implied by green C++).

## Functional proof (smoke test)

`docs/superpowers/sc598/smoke.lisp` via the **base image** (`iclasp --base`) and via the
**snapshot file** (`iclasp --snapshot`), both green:
```
clasp-in-features: T · fib(25)=75025 · compiled-square-7=49 · smoke-ok · rc=0
```
`(compile …)`+`funcall` works → **bytecode compile-on-device confirmed**.

## Snapshot / deployable-image findings

1. **Conservative GC cannot snapshot.** `boehm` (conservative) `save-lisp-and-die` →
   `SIMPLE-PROGRAM-ERROR: save-lisp-and-die only works for precise GC`. **The deployable image
   must use a precise variant (`boehmprecise`).** Spec updated.

2. **Snapshot data serialization works on aarch64-linux.** `boehmprecise` writes a valid
   **165 MB** snapshot file (`save-lisp-and-die … :executable nil`, rc=0), and it **loads and
   runs** (`iclasp --snapshot <file>`). This is the SC598 deployment path.

3. **`:executable t` standalone-binary form is broken on aarch64-linux (upstream bug).**
   - **Root cause (primary):** `src/gctools/snapshotSaveLoad.cc:1857-1858` hardcodes the objcopy
     wrap as `--output-target elf64-x86-64 --binary-architecture i386`. On aarch64 this fails:
     `objcopy: architecture i386 unknown` (rc=1) → the `mkstemp` object stays **empty** →
     `ld.gold: file is empty` at the final link.
   - **Root cause (masking):** `snapshotSaveLoad.cc:1863` and `:1888` use `if (system(cmd) < 0)`,
     which only detects *fork* failure, not a non-zero *exit*. So both the objcopy failure and the
     link failure are ignored and control reaches `exit(0)` — **ninja reported success** despite no
     artifact (`NINJA_RC=0`, missing `snapshot-boehmprecise`).
   - Why only here: the Darwin path (`:1879`, `-sectcreate`) and x86-64-linux both work; **CI never
     exercises aarch64-linux ELF** (matrix is x86-64-ubuntu + aarch64-macOS).
   - **Fix (APPLIED + verified end-to-end):** parameterize the objcopy target/arch by build arch
     and correct the two `system(...)` exit checks — see
     `patches/0001-snapshot-aarch64-objcopy.patch`. After the patch, `ninja snapshot-boehmprecise`
     → `NINJA_RC=0` and produces a working **212 MB** `ELF 64-bit … ARM aarch64` PIE executable that
     boots from its embedded snapshot and passes the smoke test (`EXE_SMOKE_RC=0`).

## Investigations (unblock M1–M5 recipe)

- **koga cross knobs:** `cc/cxx/ar/nm/ld/llvm-config` are config slots (`configure.lisp:387-431`) →
  recipe points them at the Yocto SDK; **no koga patch needed** for toolchain selection.
- **OS/arch macros:** `_TARGET_OS_~A` from host `*features*` (`config-header.lisp:135`),
  `_ADDRESS_MODEL_64` set (`:52`) → correct because build host and target are both aarch64-linux.
  "Cross" = sysroot/toolchain only.
- **Scraper = host:** `scrape-pp` is `$cxx -E -DSCRAPING`; generation is `$lisp`=sbcl
  (`generate-sif`/`generate-headers`). No target execution.
- **Bytecode image = host SBCL:** `compile-bytecode-image → $lisp` (sbcl). Architecture-independent
  bytecode. Target `$clasp` runs only for `make-snapshot`, `generate-lisp-info`, `compile-systems`,
  tests — a small, native-on-the-aarch64-builder surface.
- **Bytecode-only image:** no koga knob excludes the native (Cleavir) compiler; `--no-default-native`
  only flips the runtime default. Ship full image (libLLVM linked regardless); size-only, revisit at M5.
- **LLVM majors accepted:** **15–20, 22** (`units.lisp:3-5`) — not 21. Confirms 24.04's clang-18 is fine;
  pins the Yocto-release LLVM question.
- **Default variants:** 6 (`boehm`/`boehmprecise`/`preciseprep` ± debug); only `boehm`+`boehmprecise`
  built here.

## Footprint data point

Precise snapshot file = **165 MB**. Plus `iclasp` + shared libs (incl. `libLLVM-18`). Relevant to the
512 MB–1 GB budget — comfortable at the upper end; tight at 512 MB (revisit footprint levers at M5).

## Tests (Task 7) — deferred

The `test`/`ansi-test` ninja targets invoke `$clasp`, which resolves to the (per-variant) snapshot
binary; with the executable-snapshot bug open, running the full suite would be confounded by that
dependency rather than testing conformance. The smoke tests (base image **and** snapshot file) already
prove core functionality. Run the full ANSI/regression suite once the snapshot-exec decision is made
(against the snapshot file or a fixed executable).

## Outstanding

- **User input:** Yocto **release name** → LLVM-major pin for the recipe (M1).
- **Test suite (Task 7):** now runnable against the fixed snapshot; not yet run (smoke is green on
  the base image, the snapshot file, and the standalone executable).
- **Upstream PR (optional):** `patches/0001-snapshot-aarch64-objcopy.patch` is ready; before a PR,
  check against upstream HEAD / issues (may already be reported).
