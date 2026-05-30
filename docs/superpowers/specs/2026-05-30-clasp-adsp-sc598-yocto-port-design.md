# Design: Clasp on ADSP-SC598 (aarch64 Linux / Yocto)

- **Date:** 2026-05-30
- **Status:** Draft for review
- **Target:** Analog Devices ADSP-SC598, ARM Cortex-A55 running Yocto-built Linux
- **Author context:** dg1sbg (Clasp contributor via dg1sbg/clasp fork; no upstream push)

---

## 1. Summary and reframe

This is **not a CPU/architecture port**. `aarch64` + `Linux` is already a supported
Clasp configuration:

- aarch64 calling-convention glue is explicitly present: `include/clasp/core/lispCallingConvention.h:79`
  gates on `__x86_64__ || __aarch64__`.
- Linux is a first-class OS path: `include/clasp/core/configure_clasp.h:68` accepts
  `_TARGET_OS_LINUX`.
- aarch64 atomics backend exists (`src/libatomic_ops/.../sysdeps/gcc/aarch64.h`).

The Cortex-A55 is ARMv8.2 AArch64 — the same ISA family Clasp already runs on (Apple
Silicon). The core VM is expected to need **zero architecture work**.

The actual project is a **build-and-packaging problem**: produce a *target-correct* Clasp
image and runtime, from source, as a **Yocto bitbake recipe** in a custom meta-layer, that
drops into the SC598 rootfs.

### Locked decisions (from scoping Q&A)

| Decision | Value |
|---|---|
| Target userland | **64-bit aarch64** (confirmed) |
| Deployment model | **Run a prebuilt image/app** on the board |
| Runtime codegen | **Bytecode compile on device** (eval/load to bytecode); **no native JIT on device** |
| RAM budget | **512 MB – 1 GB** |
| Build/packaging | **Full Yocto from-source recipe** (reproducible meta-layer) |
| Build host | **M-series Mac hosting a 4 KB-page aarch64 Linux VM** (Lima/UTM/Multipass); whole bitbake build runs inside the VM |
| libc | **glibc** (confirmed) |
| GC | **Boehm *precise*** (`boehmprecise` variant), vendored in-tree — `save-lisp-and-die`/snapshot requires precise GC, confirmed M0 (the conservative `boehm` variant errors: "save-lisp-and-die only works for precise GC") |
| DSP cores | **Out of scope for Lisp.** Lisp runs on the A55; SHARC+ cores orchestrated via RPMsg/shared memory by the application, not as Lisp execution targets |

---

## 2. The central engineering problem and its solution

### 2.1 Why a naive cross-build fails

Clasp's build is **self-hosting**. After the C++ runtime (`libclasp`, the `iclasp`
executable) is compiled, the build must **run `iclasp` on the target ISA** to load and
compile the Common Lisp system (CL + CLOS + the compiler) and write the heap **snapshot**
image (`docs/bootstrap.rst`; stages `aclasp → bclasp → cclasp`). koga has no host/target
split, and the snapshot is an architecture-specific binary heap image. So "just point a
cross-toolchain at it" does not work — something must execute aarch64 code during the build.

### 2.2 Three facts that make it tractable

1. **Target execution is confined to a few finalization steps (refined by M0).** koga
   (configure → `build.ninja` + headers), the scraper (`$cxx -E -DSCRAPING` + host-SBCL
   `generate-sif`/`generate-headers`), and — in bytecode mode — **the compilation of the
   whole Lisp system to bytecode** (`compile-bytecode-image` runs host `$lisp`=sbcl, emitting
   architecture-independent bytecode) all run on the **host**. The C++ compile is ordinary
   cross-compilation. The target `$clasp` is executed only for `make-snapshot`,
   `generate-lisp-info`, `compile-systems`, and the test rules — a *smaller* surface than a
   self-hosted native bootstrap, and on the aarch64 VM these run natively.

2. **Bytecode build-mode removes the JIT from that one step.** With `build-mode :bytecode`
   (`src/koga/configure.lisp` `build-mode` slot; `src/koga/scripts.lisp:258-262` passes
   `:native nil` to `compile-file`), the bootstrap compiles Lisp to **bytecode**, executed
   by the LLVM-free VM in `src/core/bytecode.cc`. The emulated step therefore never invokes
   the LLVM JIT — the part that is genuinely hostile to emulation.

3. **The bootstrap yields target-correctness by construction.** The snapshot is produced
   *by the target `iclasp` linked against the target sysroot*, so when it is loaded on the
   board it relocates against the *same* libraries (same Yocto build). The "snapshot vs
   target-library ABI skew" risk is eliminated, not merely mitigated.

### 2.3 How the target-execution step is run

**Locked: aarch64-native build host (M-series Mac + aarch64 Linux VM).** The whole bitbake
build runs inside an **aarch64 Linux VM** on the Apple Silicon Mac (Lima/UTM/Multipass;
ext4/case-sensitive root; allocate ≥12–16 GB RAM and ≥80 GB disk for Yocto + the Clasp
bootstrap). Because the VM is aarch64 Linux, the bootstrap runs the freshly built **target**
`iclasp` **natively** — invoked through the target sysroot's own loader
(`${sysroot}/lib/ld-linux-aarch64.so.1 --library-path ${sysroot}/usr/lib ./iclasp …`), so
there is **no emulation at all**. (OE's default qemu-user wrapper also works on matching ISA
and is a fine alternative; either way the qemu bootstrap risk is retired.) The M-series vCPU
is ARMv8.5+, a superset of the A55's ARMv8.2, so `-mcpu=cortex-a55` codegen and LSE atomics
run natively in the VM.

**Build-host page size (keep live):** the SC598 kernel is almost certainly 4 KB pages, and
the snapshot is generated *in the VM*, so the **VM guest kernel must also be 4 KB pages**.
Stock Ubuntu/Debian arm64 kernels are 4 KB (good); avoid a 16 KB-page guest. Verify in-VM:
`getconf PAGESIZE` must print `4096`.

> **Decision D1 (locked):** aarch64-native builder on the M-series Mac via a 4 KB-page
> aarch64 Linux VM. The x86-64 + qemu-user route remains documented as a fallback for a
> future CI/build-farm scenario but is not the chosen path.

---

## 3. Build pipeline (recipe task mapping)

```
do_configure:  SBCL(host) runs koga  ->  build.ninja + config headers   [target-parameterized]
do_compile #1: SBCL(host) runs scraper -> generated C++ bindings        [host; no target exec]
do_compile #2: cross-CXX compiles libclasp + iclasp                     [target objects, target sysroot]
do_compile #3: target iclasp bootstraps Lisp -> bytecode snapshot       [ONLY target-exec step; native-aarch64 or qemu]
do_install:    stage libs + snapshot/image + fasl tree into ${D}
```

ninja orchestrates #1–#3 as one graph (koga emits the rules). Under bitbake, `do_compile`
just runs `ninja`; the target-exec rules resolve via native execution or binfmt+qemu.

---

## 4. Component design

### 4.1 koga cross-parameterization (the main code change)

koga currently auto-detects the **host** environment. For cross it must describe the
**target**:

- **`*features*` / target descriptors:** force target features (`:unix :linux :arm64
  :64-bit`, bytecode build-mode) regardless of the host SBCL's own `*features*`.
  Investigate whether koga already exposes a knob (`src/koga/configure.lisp`,
  `src/koga/config-header.lisp`); if not, add one (or a thin wrapper that rebinds
  `*features*`).
- **Toolchain:** use OE's cross `${CXX}/${CC}/${LD}` and target sysroot instead of
  host detection.
- **LLVM:** feed koga the **target** `llvm-config` (meta-clang) so `--cxxflags/--ldflags/
  --libs/--system-libs/--includedir` (`src/koga/units.lisp:18-51`) report sysroot-relative
  target paths. llvm-config-for-cross is a known OE pain point — meta-clang distinguishes
  `llvm-config` (target) from `llvm-config-native`; the recipe must select correctly.
- **Install layout:** target `prefix`, `CLASP_HOME`, image/fasl install paths.

This is the largest single change. Fallback if koga resists clean parameterization: patch
the detection points directly in the recipe (`SRC_URI` patch).

### 4.2 LLVM provisioning on target

- The C++ runtime links **all of libLLVM unconditionally**, even in bytecode mode
  (`src/koga/units.lisp` appends `llvm-config --libs`; `src/core/function.cc:50-51,105-113`
  weaves `llvmo::` types into dispatch). So target libLLVM is a hard build- and run-time
  dependency. **Bytecode-compile-on-device works** (the bytecode compiler/VM never touch
  it), but libLLVM must be *present*.
- **Use the LLVM version your meta-clang / Yocto release provides** (recent releases ship
  LLVM ~18–20). **Do not target LLVM 22 on the device** — that is the local dev pin. Verify
  Clasp's accepted LLVM version range covers the chosen version (Clasp LLVM version checks
  in CMake/koga).
- **Shared `libLLVM.so`** (demand-paged) is preferred over static for RSS at 512 MB–1 GB;
  the cost is rootfs/flash size. (Static-into-libclasp with `--gc-sections` is an
  alternative that trims dead LLVM and removes one ABI to match, at the cost of a large
  binary — keep as a footprint lever, decide after M1 measurement.)

### 4.3 GC, libc, FP exceptions

- **GC:** Clasp vendors BDW-GC + libatomic_ops in-tree and builds them; the aarch64 backend
  exists. Prefer building the vendored copies (cross-compiles as plain C) over an external
  `bdwgc` recipe — fewer moving parts. Confirm in M1.
- **libc = glibc (confirmed).** No FPE shim needed — the glibc `feenableexcept` path serves
  `include/clasp/core/numbers.h:60-125` (which otherwise has only a Darwin-aarch64 FPCR
  path). Relevant only if the image ever moved to musl (which lacks `feenableexcept` and
  alters libunwind/TLS) — not applicable.
- **Backtrace/unwind:** recent work hardened the macOS libunwind path. The Linux-aarch64
  unwinder is a different implementation — validate backtraces on target in M5.

### 4.4 Image content / footprint

- `build-mode :bytecode` keeps fasls as bytecode (`fasl`, not `nfasl` —
  `src/koga/config-header.lisp:25-27`), yielding a smaller image than native.
- **Investigate** whether Clasp can build an image that **excludes the native Cleavir
  compiler** (the device only needs the *bytecode* compiler). If supported, this shrinks
  heap/snapshot and shortens the emulated bootstrap. If not, ship the full image — libLLVM
  is linked regardless, so this is a size optimization, not a dependency change. Fallback:
  full image.

### 4.5 meta-layer & recipe skeleton

```
meta-clasp/
  conf/layer.conf
  recipes-devtools/clasp/clasp_<ver>.bb        # the main recipe
  recipes-devtools/clasp/files/                # koga-cross patch(es)
  recipes-devtools/common-lisp/sbcl-native_*.bb # IF sbcl-native not available in layers
```

Recipe essentials:

- `DEPENDS = "clang sbcl-native qemu-native ..."` — `clang`/`llvm` for target libLLVM;
  `sbcl-native` to run koga + scraper; `qemu-native` for the bootstrap (fallback host).
- `RDEPENDS:${PN}` — runtime libs (`libLLVM`, libstdc++, libgcc), the image, fasl tree.
- `SRC_URI` — Clasp source (upstream or dg1sbg fork) + cross patches.
- `do_configure/do_compile/do_install` per §3; large `do_compile` timeout.
- `LICENSE` / `LIC_FILES_CHKSUM` (Clasp LGPL-2.1+; LLVM Apache-2.0-WITH-LLVM-exception).
- `INSANE_SKIP` as needed (large binaries, prebuilt-ish image artifacts).

> **Open dependency:** is `sbcl-native` available in the user's layers
> (meta-openembedded)? If not, a `sbcl-native` recipe is required (or use ECL/CCL). Resolve
> in M0.

---

## 5. Milestones

- **M0 — Build host + de-risk (go/no-go).**
  1. Stand up the **aarch64 Linux VM** on the M-series Mac (4 KB pages — verify); this *is*
     the build host. Inside it, build Clasp **natively** (bytecode mode, Boehm) and run the
     test suite. Proves aarch64-linux Clasp works at all (it is *not* in Clasp CI) and
     exercises the native-execution path the recipe's bootstrap will use. The qemu-user
     spike is **retired** by the native VM.
  2. Provide the **Yocto release name** (kirkstone/scarthgap/…) — needed for M1's LLVM
     pin. This is build-config metadata, *not* board access, so it need not wait on the
     device.
  3. Remaining **device facts are deferred** (no board access now): exact page size,
     `free -m`, on-image libLLVM, CPU features. Needed by M4/M5, not M0–M2.
- **M1 — Toolchain & deps.** meta-clang LLVM version pinned and matched to Clasp;
  `sbcl-native` resolved; vendored bdwgc cross-build confirmed.
- **M2 — koga cross.** Target-parameterize koga (features, toolchain, llvm-config, paths);
  produce `build.ninja` for the cross target on the host.
- **M3 — Recipe end-to-end.** `meta-clasp` recipe builds libclasp/iclasp (cross) and runs
  the bootstrap (native-aarch64 or qemu) to produce a target snapshot; `do_install` stages
  artifacts.
- **M4 — Image into rootfs.** Add to an image recipe; boot on the SC598.
- **M5 — On-device validation.** REPL; on-device bytecode `compile`/`load`/`eval`; measure
  RSS + dormant libLLVM footprint; GC under constrained RAM; FP exceptions; signals/GC
  safepoints; backtraces; startup time. Decide if 512 MB needs the footprint levers (§4.2).

---

## 6. Risk register

| ID | Risk | Impact | Mitigation |
|---|---|---|---|
| R1 | Bootstrap emulation issues (threads/Boehm under qemu) | **Retired** | aarch64 Linux VM runs the target binary **natively**; no qemu in the chosen path |
| R2 | libc musl (FPE/unwind/TLS) | **Retired** | glibc confirmed |
| R3 | LLVM version: meta-clang ≠ Clasp pin | High (schedule) | Align Clasp to meta-clang's LLVM (not 22); verify Clasp version range |
| R4 | koga not cleanly cross-parameterizable | Moderate | Wrapper to rebind `*features*`; else patch detection points |
| R5 | 512 MB headroom (image + libLLVM + heap + GC) | Moderate | Bytecode image; exclude native compiler if possible; shared libLLVM; tune heap; LLVM-shed only as last resort |
| R6 | VM guest vs target **page-size** mismatch | Moderate (sneaky) | 4 KB-page arm64 guest kernel; verify `getconf PAGESIZE`=4096 in-VM |
| R7 | `sbcl-native` unavailable in layers | Low | Write `sbcl-native` recipe or use ECL/CCL |
| R8 | Cortex-A55 codegen / LSE atomics | Low | `-mcpu=cortex-a55`; confirm LSE enabled |
| R9 | aarch64-linux not in Clasp CI → latent gaps | Moderate | M0.1 native aarch64 build shakes these out cheaply |

---

## 7. Open investigations (resolve during planning/M0)

1. koga: existing cross knob, or can `*features*` be overridden cleanly?
   (`src/koga/configure.lisp`, `config-header.lisp`.)
2. Confirm the scraper runs host-side only (SBCL), no target execution. (Strongly implied.)
3. Can Clasp build a bytecode image **without** the native Cleavir compiler?
4. ~~qemu bootstrap viability~~ — **retired** by the native aarch64 VM (revisit only if an
   x86-64 CI/build-farm path is later added).
5. meta-clang LLVM version for the user's Yocto release; Clasp's accepted range.
6. `sbcl-native` availability.
7. Device facts — **deferred** (no board access now); gather before M4/M5:
   `uname -m -r` · `getconf PAGESIZE` · `free -m` ·
   `find / \( -name 'libLLVM*' -o -name 'llvm-config*' \) 2>/dev/null` ·
   `grep -i features /proc/cpuinfo`. The **Yocto release name** is needed earlier (M1) and
   is independent of board access — provide it when convenient.
8. Vendored bdwgc/libatomic_ops cross-build vs external `bdwgc` recipe.
9. Shared vs static libLLVM footprint at 512 MB–1 GB (decide after M1 measurement).

---

## 8. Out of scope

- Removing the libLLVM runtime dependency (the ~10k-line `function.cc`/snapshot decoupling).
  Only revisit if M5 proves 512 MB cannot fit a shared, demand-paged libLLVM.
- Native JIT on device (user chose bytecode-compile-on-device).
- Running Lisp on the SHARC+ DSP cores.
- 32-bit / aarch32 support (target confirmed 64-bit).
- Upstreaming koga cross support (separate effort; PRs go via the dg1sbg fork if pursued).

---

## 9. Success criteria

1. A `meta-clasp` bitbake recipe builds Clasp **from source** for aarch64 SC598 reproducibly.
2. The produced image **boots and runs** on the SC598 under the Yocto Linux.
3. A Clasp REPL runs; **bytecode `compile`/`load`/`eval` works on-device**; the standard
   library is functional.
4. Resident footprint fits the **512 MB–1 GB** budget with GC headroom.
5. A representative subset of the Clasp/ANSI test suite passes on-device.

---

## 10. Confidence

- aarch64/Linux core VM needs no arch work — **high**.
- Bytecode-mode bootstrap is the right lever to enable cross/emulation — **high**.
- Snapshot target-correctness-by-construction via the bootstrap — **high**.
- aarch64-native builder makes the bootstrap low-risk — **high**.
- Bootstrap runs natively in the aarch64 VM (qemu risk retired) — **high**.
- meta-clang LLVM version alignment effort — **moderate** (likely the schedule driver).
- koga cross-parameterization is bounded, not a rewrite — **moderate**.
