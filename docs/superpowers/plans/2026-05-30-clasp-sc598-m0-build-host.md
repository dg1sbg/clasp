# Clasp SC598 — M0: Build Host & aarch64-linux Proof Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up an aarch64 Linux build host on the M-series Mac and prove that Clasp builds, runs, bytecode-compiles, and passes its test suites natively on aarch64-linux — the go/no-go gate before any Yocto-recipe work.

**Architecture:** A Lima-managed aarch64 Ubuntu 24.04 VM (Apple Virtualization.framework / `vz`, 4 KB pages) builds Clasp from a clean upstream clone using the VM's *native* LLVM 18 toolchain in `--build-mode=bytecode --no-default-native` (the device's intended mode). The build runs in VM-local ext4 storage; small text artifacts (VM script, provisioning, smoke test, findings) are authored and committed in this repo. Four source-level investigations resolve the facts the recipe plan (M1–M5) needs.

**Tech Stack:** Lima + Apple Virtualization.framework, Ubuntu arm64, LLVM/Clang 18, SBCL, Ninja, Clasp `koga`.

**Working model (read first):**
- This repo (`/Users/frgo/gbt Dropbox/gbt/projects/clasp`) holds the committed artifacts under `docs/superpowers/sc598/`. Lima mounts the Mac home read-only, so inside the VM these files appear at the **same absolute path** — no copying needed.
- The actual Clasp build happens in **VM-local** storage (`~/clasp` on ext4), NOT on the Dropbox-mounted path (that path is slow and corrupts build timing — a known issue here).
- M0 does **not** modify Clasp source. koga cross changes come in the M1–M5 plan.

**Prerequisites:** Apple Silicon Mac, ideally ≥32 GB RAM (we give the VM 16 GB) and ≥120 GB free disk; `brew install lima` done; network access for clones.

---

### Task 1: Create the work area, branch, and Lima VM template

**Files:**
- Create: `docs/superpowers/sc598/start-vm.sh`

- [ ] **Step 1: Create a dedicated branch (keep SC598 work off `perf/runtime-hotpaths`)**

```bash
cd "/Users/frgo/gbt Dropbox/gbt/projects/clasp"
git switch -c feat/sc598-yocto-port
mkdir -p docs/superpowers/sc598
```

- [ ] **Step 2: Write the VM-start script**

Create `docs/superpowers/sc598/start-vm.sh`:

```bash
#!/usr/bin/env bash
# Bring up an aarch64 Linux build host for the Clasp SC598 port.
# On Apple Silicon, Lima defaults to arch=aarch64 + vmType=vz (native, no emulation).
set -euo pipefail

NAME="${1:-clasp-arm64}"

# template://ubuntu tracks the current Ubuntu LTS (24.04), which carries clang/llvm-18.
limactl start \
  --name="${NAME}" \
  --cpus=8 \
  --memory=16 \
  --disk=100 \
  --tty=false \
  template://ubuntu

echo "VM '${NAME}' started. Open a shell with:  limactl shell ${NAME}"
```

- [ ] **Step 3: Start the VM**

Run:
```bash
chmod +x docs/superpowers/sc598/start-vm.sh
docs/superpowers/sc598/start-vm.sh clasp-arm64
```
Expected: Lima downloads the Ubuntu arm64 cloud image (first run only) and prints `VM 'clasp-arm64' started.`

- [ ] **Step 4: Verify the VM is aarch64, 4 KB pages, and adequately sized**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'uname -m; getconf PAGESIZE; nproc; free -g | awk "/Mem/{print \$2\" GiB\"}"; lsb_release -ds'
```
Expected (exact for the first two lines):
```
aarch64
4096
8
15 GiB        # ~16
Ubuntu 24.04...
```
If `getconf PAGESIZE` is not `4096`, STOP — pick a 4 KB-page guest (do not proceed on a 16 KB guest; it would taint the snapshot for the 4 KB SC598).

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/sc598/start-vm.sh
git commit -m "sc598(m0): add aarch64 Linux build-host (Lima) start script"
```

---

### Task 2: Provision build dependencies in the VM

**Files:**
- Create: `docs/superpowers/sc598/provision.sh`

- [ ] **Step 1: Write the provisioning script (deps are the CI-authoritative set)**

Create `docs/superpowers/sc598/provision.sh`:

```bash
#!/usr/bin/env bash
# Install Clasp's Linux build dependencies (mirrors .github/workflows/test.yml).
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  git build-essential pkg-config curl \
  binutils-gold \
  clang-18 libclang-18-dev libclang-cpp18-dev llvm-18 llvm-18-dev \
  libelf-dev libgmp-dev libunwind-dev \
  ninja-build sbcl \
  libnetcdf-dev libexpat1-dev libfmt-dev libboost-all-dev

echo "--- versions ---"
clang-18 --version | head -1
llvm-config-18 --version
sbcl --version
ninja --version
```

- [ ] **Step 2: Run it in the VM**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc '/Users/frgo/gbt\ Dropbox/gbt/projects/clasp/docs/superpowers/sc598/provision.sh'
```
(The repo path is visible inside the VM via the Lima home mount. If the space in the path is awkward, `cp` the script to `~` first and run it there.)

Expected: ends with four version lines, including `llvm-config-18` printing `18.x.x`.

- [ ] **Step 3: Verify koga will find LLVM 18**

koga accepts LLVM majors **15–20 or 22** (`src/koga/units.lisp:3-5`) and searches for `llvm-config-<major>` (`units.lisp:7-13`). Confirm the binary it will pick exists:
```bash
limactl shell clasp-arm64 -- bash -lc 'which llvm-config-18 && llvm-config-18 --bindir'
```
Expected: a path like `/usr/bin/llvm-config-18` and a bindir like `/usr/lib/llvm-18/bin` (where `clang`/`clang++` live — koga locates them via `llvm-config --bindir`).

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/sc598/provision.sh
git commit -m "sc598(m0): add build-dependency provisioning script"
```

---

### Task 3: Clone Clasp into VM-local storage

**Files:** (none committed — throwaway build tree in the VM)

- [ ] **Step 1: Clone a clean upstream baseline into ext4 (not the Dropbox mount)**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~ && rm -rf clasp && git clone https://github.com/clasp-developers/clasp.git ~/clasp && cd ~/clasp && git rev-parse --short HEAD'
```
Expected: a clone completes and prints a short commit hash. (Upstream main gives a clean aarch64-linux baseline, independent of the local fork's branches.)

- [ ] **Step 2: Verify koga runs under the host SBCL**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && ./koga --help | head -20'
```
Expected: koga help text (confirms `sbcl --script ./koga` works — `koga:1`).

---

### Task 4: Configure the bytecode build

**Files:** (none committed; the exact invocation is recorded in findings in Task 9)

- [ ] **Step 1: Configure with the device's intended mode**

`--build-mode=bytecode` builds the system as bytecode; `--no-default-native` makes runtime `compile`/`compile-file` default to **bytecode** (`test.yml:63`, `src/koga/scripts.lisp:258-262`) — i.e. "bytecode compile on device."

Run (this also git-clones koga's external Lisp deps from `repos*.sexp`, so it needs network):
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && ./koga --build-mode=bytecode --no-default-native'
```
Expected: a `Configuring LLVM` line, no "could not find required LLVM" error, and completion that writes `~/clasp/build/build.ninja`.

- [ ] **Step 2: Verify the generated build and the LLVM version koga locked onto**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && test -f build/build.ninja && echo BUILD_NINJA_OK; grep -RIn "LLVM" build/*config* 2>/dev/null | head'
```
Expected: `BUILD_NINJA_OK`. Note the LLVM version reported — must be 18.

- [ ] **Step 3: Verify the default GC variant is boehm**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && ls build'
```
Expected: a `boehm` subdirectory (the default variant; `src/koga/configure.lisp:650`). Record what other variant dirs appear (the default config builds several boehm variants — relevant to build time and to image size in M1).

---

### Task 5: Build Clasp (long-running — the real aarch64-linux proof)

**Files:** (none committed)

- [ ] **Step 1: Build, inside a detachable session (this can take 1–3 h)**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && script -q -c "ninja -C build" ~/clasp-build.log; tail -5 ~/clasp-build.log'
```
Expected: ninja reaches 100% and exits 0. (`script` captures the full log to `~/clasp-build.log` for triage.)

- [ ] **Step 2: Locate the built interpreter/compiler binary**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'find ~/clasp/build -maxdepth 3 -name iclasp -type f'
```
Expected: a path, most likely `~/clasp/build/boehm/iclasp`. Save it — later steps call it `$ICLASP`.

- [ ] **Step 3: If the build FAILED — capture it as a finding (do not silently retry)**

A build failure here is the single most valuable M0 output (aarch64-linux is not in Clasp CI for *this* host combo). Record the first error verbatim:
```bash
limactl shell clasp-arm64 -- bash -lc 'grep -nE "error:|FAILED:" ~/clasp-build.log | head -20'
```
Append the output, plus your triage (missing dep vs. real aarch64-linux source gap vs. RAM/OOM — check `dmesg | grep -i oom`), to `docs/superpowers/sc598/m0-findings.md` (created in Task 9). If it is OOM, set `:parallel-build nil` via `./koga --build-mode=bytecode --no-default-native --no-parallel-build` and rebuild. Only continue past this task once the build is green.

---

### Task 6: Smoke-test the built Clasp

**Files:**
- Create: `docs/superpowers/sc598/smoke.lisp`

- [ ] **Step 1: Write the smoke test**

Create `docs/superpowers/sc598/smoke.lisp`:

```lisp
;;;; M0 smoke test: the aarch64-linux Clasp runs, is the bytecode build, and can compile+run.
(format t "~&clasp-in-features: ~a~%" (and (member :clasp *features*) t))
(format t "impl: ~a ~a~%" (lisp-implementation-type) (lisp-implementation-version))
(defun fib (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(format t "fib(25)=~a~%" (fib 25))
(let ((sq (compile nil '(lambda (x) (* x x)))))
  (format t "compiled-square-7=~a~%" (funcall sq 7)))
(format t "smoke-ok~%")
```

- [ ] **Step 2: Run it through the built Clasp**

Run (uses the `$ICLASP` path from Task 5 Step 2):
```bash
limactl shell clasp-arm64 -- bash -lc 'ICLASP=$(find ~/clasp/build -maxdepth 3 -name iclasp -type f | head -1); "$ICLASP" --non-interactive --load "/Users/frgo/gbt Dropbox/gbt/projects/clasp/docs/superpowers/sc598/smoke.lisp"'
```
Expected stdout to contain, in order:
```
clasp-in-features: T
impl: Clasp ...
fib(25)=75025
compiled-square-7=49
smoke-ok
```
If `--non-interactive` is rejected, use instead: `"$ICLASP" --load <path> --eval '(core:exit 0)'`.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/sc598/smoke.lisp
git commit -m "sc598(m0): add aarch64-linux Clasp smoke test"
```

---

### Task 7: Run the regression and ANSI test suites

**Files:** (results recorded into findings in Task 9)

- [ ] **Step 1: Regression tests**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && script -q -c "ninja -C build test" ~/clasp-test.log; tail -15 ~/clasp-test.log'
```
Expected: a test summary. Record total/failed counts (search the log: `grep -iE "fail|pass|error" ~/clasp-test.log | tail -30`).

- [ ] **Step 2: ANSI conformance tests**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && script -q -c "ninja -C build ansi-test" ~/clasp-ansi.log; tail -20 ~/clasp-ansi.log'
```
Expected: an ANSI test summary (some known failures are normal). Record the failure count and compare it to Clasp's documented baseline (note any *new* aarch64-linux-specific failures vs. the expected set).

- [ ] **Step 3: There is no separate commit here** — results are committed with the findings doc in Task 9.

---

### Task 8: Source investigations that unblock the M1–M5 recipe plan

Each step inspects the checkout (do it in the VM clone or the Mac repo — same source) and writes a finding. These remove the guesswork that currently blocks writing bite-sized recipe steps.

- [ ] **Step 1: Confirm koga's cross-toolchain override slots (old risk R4)**

Inspect `src/koga/configure.lisp:389-435` and confirm these initargs exist and are settable from `config.sexp`/CLI: `:cc`, `:cxx`, `:ar`, `:nm`, `:ld`, `:llvm-config`, plus `--cflags/--cppflags/--cxxflags` (used in `test.yml:71`). Record: "the recipe points these at the Yocto SDK cross-tools + target `llvm-config`; no koga patch needed for toolchain selection."

- [ ] **Step 2: Confirm OS/arch macros come from host `*features*` (and why that's fine here)**

Open `src/koga/config-header.lisp` (around the `_TARGET_OS_*` / address-model emission). Confirm `_TARGET_OS_LINUX` and the 64-bit address model are derived from the build host's `*features*`. Record the exact lines, and the conclusion: because the build host is aarch64 **Linux** and the target is aarch64 **Linux**, host-derived macros are already correct — the only "cross" concern is sysroot/toolchain/libraries, not arch/OS detection.

- [ ] **Step 3: Confirm the scraper is host-side only**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'grep -nE "scrap|sbcl|iclasp" ~/clasp/build/build.ninja | grep -i scrap | head'
```
Record whether the scraper rule invokes `sbcl` (host) or a target `iclasp`. Expected: host `sbcl` — meaning the *only* target-execution step in the recipe is the image bootstrap.

- [ ] **Step 4: Determine whether a bytecode-only image can exclude the native compiler**

Search for a knob that drops Cleavir/native-compiler inclusion from the image (size lever at 512 MB):
```bash
limactl shell clasp-arm64 -- bash -lc 'grep -RInE "cleavir|default-native|no-native|aclasp|bclasp|cclasp|image" ~/clasp/src/koga | grep -iE "native|image|stage" | head -30'
```
Record the finding: either "a knob exists (name it)" or "no exclusion knob — ship the full image; libLLVM is linked regardless, so this is size-only." Note: this is independent of `--no-default-native` (which sets the *default*, not *inclusion*).

- [ ] **Step 5: Record the LLVM-major decision input**

State the accepted set from `src/koga/units.lisp:3-5` — majors **15–20, 22** (note: **not 21**) — and the mapping to Yocto/meta-clang: Scarthgap≈LLVM 18, Styhead≈19, Walnascar≈20 are in-range; **Kirkstone≈LLVM 14 is too old** and would need a newer meta-clang or a custom LLVM recipe. Flag that the recipe's LLVM pin is decided by the user's **Yocto release name** (still outstanding).

---

### Task 9: Write the M0 findings + go/no-go, and unblock M1

**Files:**
- Create: `docs/superpowers/sc598/m0-findings.md`

- [ ] **Step 1: Write the findings document**

Create `docs/superpowers/sc598/m0-findings.md` with these sections, filled from Tasks 4–8:

```markdown
# Clasp SC598 — M0 Findings

## Environment
- Host: <Mac model / RAM>, Lima vz, Ubuntu <ver>, pages=<4096>, vCPU=<8>, RAM=<16 GiB>
- Toolchain: clang/llvm <18.x>, sbcl <ver>, ninja <ver>
- Clasp upstream commit: <hash>
- koga line: ./koga --build-mode=bytecode --no-default-native

## Build result
- Status: <green | failed: first error + triage>
- iclasp path: <build/boehm/iclasp>
- Variants built: <list>  | wall-clock: <time>  | parallel-build: <t|nil>

## Smoke test
- <pasted smoke-ok output>

## Test suites
- Regression (`ninja -C build test`): <pass/fail counts>
- ANSI (`ninja -C build ansi-test`): <fail count vs. baseline; any NEW aarch64-linux failures>

## Investigations (unblock M1–M5)
- koga toolchain slots (cc/cxx/ar/nm/ld/llvm-config): <confirmed/not>
- OS/arch macro source (config-header.lisp): <lines; host==target so OK>
- scraper host-side only: <yes/no>
- bytecode-only image (exclude native compiler): <knob name | not supported>
- accepted LLVM majors: 15–20, 22 (not 21)

## GO / NO-GO
- Decision: <GO | NO-GO + why>
- Outstanding before M1: Yocto release name -> LLVM-major pin
- New risks surfaced: <...>
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/sc598/m0-findings.md
git commit -m "sc598(m0): record build/test results, investigations, and go/no-go"
```

- [ ] **Step 3: Hand off to the M1–M5 recipe plan**

Confirm the findings doc now answers: (a) does aarch64-linux Clasp build & pass tests? (b) the koga cross knobs, (c) scraper host-side, (d) bytecode-only-image feasibility, (e) accepted LLVM majors. With these + the Yocto release name, the M1–M5 `meta-clasp` recipe plan can be written without placeholders.

---

## Self-Review

**Spec coverage (against the design doc M0):**
- "Stand up aarch64 Linux VM (4 KB pages, verify)" → Task 1. ✓
- "Build Clasp natively, bytecode mode, Boehm; run test suite" → Tasks 4–7. ✓
- "Exercises the native-execution path the bootstrap will use" → native VM build (Task 5). ✓
- "Provide Yocto release name for M1 LLVM pin" → Task 8 Step 5 + Task 9 (flagged outstanding). ✓
- Investigations I1–I3, I8 from the spec (koga cross, scraper, bytecode-only image, vendored GC) → Task 8 (vendored bdwgc/libatomic_ops is implicitly proven by a green Task 5 build; called out in Task 9). ✓
- Device facts deferred → not in M0 scope; only the Yocto release name is requested. ✓

**Placeholder scan:** Angle-bracket fields appear only inside the `m0-findings.md` *template* (values filled at execution) and as the documented binary-path discovery (`find … -name iclasp`) — not as missing plan content. No "TBD/TODO/handle appropriately." Code/commands are complete.

**Consistency:** `$ICLASP` is defined in Task 5 Step 2 and reused identically in Task 6. The koga line `--build-mode=bytecode --no-default-native` is identical in Tasks 4, 9. Dep list matches `test.yml:41`. LLVM majors (15–20, 22) cited identically in Task 3, Task 8, Task 9.

**Scope:** M0 only (build host + proof + investigations). The Yocto recipe + on-device validation (M1–M5) is a separate plan, deliberately deferred until M0 surfaces the koga/image/LLVM facts.

## Out of scope for M0 (handled in the M1–M5 plan)
- The `meta-clasp` bitbake recipe, cross-toolchain/sysroot linkage, the image bootstrap via the target sysroot loader, rootfs integration, and on-device validation.
- Any Clasp source changes (koga cross config, optional FPE/footprint work).
