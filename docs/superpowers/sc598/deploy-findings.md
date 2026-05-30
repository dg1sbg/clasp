# SC598 deployment findings — snapshot relocation + the bundle shortcut

**Date:** 2026-05-30.

## ✅ RESOLVED (patch 0002): the bundle shortcut WORKS after a relocation fix

The original problem (below) was that Clasp's snapshot load matched libraries by requiring the
recorded save-time path to be a *suffix* of the loaded path (`library_with_name`, `debugger.cc`),
so moving libs into `./lib` aborted (`SIGABRT` at `snapshotSaveLoad.cc:2120`, "Unable to find
library"). **Fix — `patches/0002-snapshot-relocation-basename-match.patch`: match libraries by
*basename* (SONAME) instead.** Verified: with the fixed `libclasp.so`, the self-contained bundle
extracted to a *fresh* path boots from its embedded snapshot and runs (`smoke-ok`, rc 0). The
snapshot is now **position-independent / relocatable**.

**Board deployment is therefore viable WITHOUT the full sysroot SDK**, with two honest caveats:
- the load still shells **`nm`** → the board image must include **binutils** (or bundle a static `nm`);
- board glibc must be ≥ the build's (Scarthgap 2.39 == the Ubuntu 24.04 build ✓).
The bundle carries its own `libLLVM`/`libstdc++`/etc., so those match by construction. **Still
pending: validation on the actual board.** Optional follow-up: also drop the runtime `nm` by reading
the ELF symtab in-process — would make the bundle fully self-contained (no binutils on target).

---

## Original analysis (why the naive bundle failed — kept for the record)

The "Ubuntu 24.04 ≈ Scarthgap → the M0 snapshot will just run on the board" claim was wrong **as
stated**: the snapshot was bound to save-time library *paths*. Patch 0002 removes that binding.

## What was tried

`make-bundle.sh`: ship the `boehmprecise` standalone-snapshot executable + its non-glibc `.so`
deps in `./lib`, run with `LD_LIBRARY_PATH=./lib` (board provides glibc).

**Result:** raw `SIGABRT` (rc 134) **before** the Lisp banner, no message. Bisected:
- Not a missing lib (all resolve), not libstdc++/libgcc, not libclasp — removing them didn't help.
- Only **one** `libLLVM` loads (not a double-load).
- The **base-image** mode (`iclasp --base`) boots fine under the *same* bundled libs.
- Only the **heap-snapshot** mode aborts.

## Root cause (snapshotSaveLoad.cc)

Snapshot relocation is **symbol+path based**, not position-independent:
- **Save:** `dladdr` each C++ pointer → record `(library path, symbol)` (`_ISLLibraries[]._Name`).
- **Load:** for each recorded library, run **`nm` on that path** + `dlsym` to recompute the load
  base (`loadLibrarySymbolLookup`, `:111`/`:116`/`:346`).

Consequences:
1. The snapshot is bound to the **library paths present at save time**. Move the libs (→ `./lib`)
   and the `nm`/dlsym base computation mismatches → abort.
2. It needs **`nm` (binutils) at runtime** on whatever loads the snapshot.
3. **Ubuntu paths ≠ Yocto paths:** Ubuntu is multiarch (`/usr/lib/aarch64-linux-gnu/libLLVM.so.18.1`);
   a Yocto target uses `/usr/lib/libLLVM.so.18`. The Ubuntu-built snapshot records multiarch paths
   that won't exist on the board → relocation fails **even though the ABI matches**.

**So there is no copy-the-snapshot shortcut.** The spec's "build against the target sysroot so the
snapshot relocates against the same libraries (at the same paths)" was right — it's necessary, not
just cleaner.

## Viable deployment options

- **(A) Sysroot-correct build (M2/M3).** Build Clasp against the Yocto SDK sysroot so the snapshot
  records the board's real `/usr/lib` paths; install libs there on the board; ensure `nm` (binutils)
  is in the image. This is the intended path. *Blocked on a build host* (goedews01 can't; needs a
  cloud x86_64 box).
- **(B) Base-image install tree.** `ninja install` produces a CLASP_HOME-relocatable base-image tree
  (path-tolerant — re-loads fasls at boot, no heap relocation). Ships the `SYS:` tree (~300 MB of
  cfasl/source). Testable at board-bring-up. NOTE: a raw relocated `iclasp --base` failed
  (`foundation.cc:328`) because `SYS:` resolved to a missing source tree — must use the **install**
  layout, not raw build artifacts. (Avoid bare `ninja install`: it builds all 6 variants.)
- **(C) Upstream fix (PR-worthy, deeper).** Make snapshot relocation position-independent: discover
  loaded libraries via `dl_iterate_phdr`/`dladdr` by SONAME at load time instead of `nm`-ing the
  recorded save-time paths. Removes the runtime `nm` dependency and the path-binding, making the
  snapshot a truly relocatable single-file deployable — ideal for embedded. Non-trivial.

## Status of `make-bundle.sh`

**Non-functional for the snapshot** (kept for the record + the bisection it enabled). A working
bundle needs option A (sysroot build) or B (install tree) — both currently gated on a build host
or board access.

## Bottom line

Both blockers stand: no x86_64 build host for the sysroot SDK (option A), and no board access to
test the install-tree (option B). The bundle shortcut is dead. Next real progress needs a cloud
x86_64 box (A), board access (B), or the upstream relocation fix (C).
