# Clasp SC598 — M1–M5: Yocto Recipe & On-Device Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Clasp from source for the ADSP-SC598 as a reproducible Yocto/bitbake recipe (`meta-clasp`) producing a precise-GC bytecode snapshot image, and validate it on the board.

**Architecture:** Build **natively on the aarch64 Linux VM** (from M0) but **link against the SC598's Yocto target sysroot** (via the Yocto SDK), so the snapshot is target-correct by construction. M0 proved the mechanics: the heavy Lisp→bytecode compile runs on **host SBCL** (`compile-bytecode-image → $lisp`), and only `make-snapshot`/`generate-lisp-info`/`compile-systems`/tests run the **target** `$clasp` — which executes natively on the aarch64 builder via the sysroot loader (no qemu-JIT). The deployable variant is **`boehmprecise`**; the build applies the M0 objcopy fix (`patches/0001`).

**Tech Stack:** Yocto/bitbake, meta-clang, Yocto eSDK/SDK, koga, clang/llvm (in-range major 15–20/22), `boehmprecise` GC, SBCL (host), ninja.

**Granularity note:** M1 and M2 are execution-ready. M3–M5 contain real bitbake content but a few values (the LLVM major, whether `sbcl-native` exists in your layers, the exact SDK sysroot path) are **outputs of M1** — they are determined by concrete M1 commands, not placeholders. Providing your **Yocto release name** up front collapses Task M1.1's branch to a single path.

**Prerequisites:** M0 complete (the `clasp-arm64` VM builds/runs aarch64-linux Clasp; `patches/0001-snapshot-aarch64-objcopy.patch` exists). Access to the SC598 Yocto build tree (layers + `bitbake`).

---

## File Structure

A new **`meta-clasp`** layer in your Yocto layers directory (outside this repo):
```
meta-clasp/
  conf/layer.conf                                   # layer metadata + priority
  recipes-devtools/clasp/clasp_git.bb               # the main recipe
  recipes-devtools/clasp/files/
    0001-snapshot-aarch64-objcopy.patch             # copied from docs/superpowers/sc598/patches/
    config.sexp                                      # koga cross configuration
  recipes-devtools/common-lisp/sbcl-native_2.4.bb   # ONLY IF sbcl-native absent in your layers (M1.2)
```
In **this** repo, `docs/superpowers/sc598/` keeps the plan, the patch, and a reference copy of the recipe files for version control.

---

## M1 — Characterize the SC598 Yocto environment

Run these in your **Yocto build directory** (where you `source oe-init-build-env`).

### Task M1.1: Determine the Yocto release and the LLVM major it provides

**Files:** none (records outputs into `docs/superpowers/sc598/m1-env.md`, created in Step 4)

- [ ] **Step 1: Identify the release and confirm meta-clang is present**

Run:
```bash
ls ../layers 2>/dev/null; grep -RinE "meta-clang" conf/bblayers.conf
git -C ../layers/poky describe --tags 2>/dev/null || cat ../layers/poky/meta/conf/distro/include/*-version.inc 2>/dev/null | head
```
Expected: a release codename (e.g. `kirkstone`, `scarthgap`, `styhead`, `walnascar`) and a `meta-clang` line in `bblayers.conf`. If `meta-clang` is absent, you will add it (it provides `clang`/`llvm`).

- [ ] **Step 2: Read the LLVM major version meta-clang will build**

Run:
```bash
bitbake -e clang 2>/dev/null | grep -E "^PV=" | head -1
```
Expected: e.g. `PV="18.1.8"` (scarthgap≈18, styhead≈19, walnascar≈20) or `PV="14...."` (kirkstone).

- [ ] **Step 3: Branch on the major version**

koga accepts majors **15–20 or 22** (`src/koga/units.lisp:3-5`), **not 21**.
- **If the major is in {15,16,17,18,19,20,22}:** record `LLVM_OK=yes` and the value; you will use meta-clang's LLVM directly. No further action in M1.1.
- **If the major is 14 (kirkstone) or 21:** record `LLVM_OK=no`. You must provide an in-range LLVM. Pick **the smallest disruptive option**:
  - Preferred: add a newer `meta-clang` branch matching an in-range LLVM (e.g. the `scarthgap`/LLVM-18 branch) as an extra layer, pinned via `PREFERRED_VERSION_clang`/`PREFERRED_VERSION_llvm`. Verify with:
    ```bash
    PREFERRED_VERSION_clang="18%" bitbake -e clang 2>/dev/null | grep -E "^PV="
    ```
    Expected `PV="18...."`.
  - If that conflicts, vendor a custom `llvm_18.bb` recipe in `meta-clasp` (copy meta-clang's scarthgap `llvm`/`clang` recipes). Validate it parses: `bitbake -e llvm | grep ^PV=`.

- [ ] **Step 4: Record findings**

Create `docs/superpowers/sc598/m1-env.md`:
```markdown
# M1 — SC598 Yocto environment
- Release: <kirkstone|scarthgap|styhead|walnascar|...>
- meta-clang: <present|added>
- LLVM major: <NN>  (LLVM_OK=<yes|no>; in-range path: <direct|newer-meta-clang|custom-recipe>)
```
Commit:
```bash
git add docs/superpowers/sc598/m1-env.md && git commit -m "sc598(m1): record Yocto release + LLVM major"
```

### Task M1.2: Confirm a host Common Lisp (`sbcl-native`) is available

**Files:** none

- [ ] **Step 1: Search the layers for an sbcl recipe**

Run:
```bash
bitbake-layers show-recipes 'sbcl*' 2>/dev/null; bitbake-layers show-recipes 'ecl*' 2>/dev/null
```
Expected: either a hit (e.g. `sbcl` in `meta-openembedded/meta-oe`) or "no recipes".

- [ ] **Step 2: Branch and record**

- **If `sbcl` (or `ecl`/`ccl`) exists:** record `HOST_LISP=sbcl-native` (you'll add `-native` via `BBCLASSEXTEND` or a `DEPENDS` on the native variant). Verify it builds native:
  ```bash
  bitbake sbcl-native
  ```
  Expected: builds; `sbcl` appears under `tmp/sysroots-components/*/sbcl-native/usr/bin/`.
- **If none exists:** record `HOST_LISP=provide`, you will add `recipes-devtools/common-lisp/sbcl-native_*.bb` in M3 (Task M3.2b). Append to `m1-env.md`.

Commit:
```bash
git add docs/superpowers/sc598/m1-env.md && git commit -m "sc598(m1): record host-lisp (sbcl-native) availability"
```

### Task M1.3: Generate the Yocto SDK (cross toolchain + target sysroot)

**Files:** none

- [ ] **Step 1: Populate the SDK for your SC598 image**

Run (replace `<sc598-image>` with your image target, e.g. `core-image-minimal` or your ADI image):
```bash
bitbake <sc598-image> -c populate_sdk
```
Expected: an installer at `tmp/deploy/sdk/*-toolchain-*.sh`.

- [ ] **Step 2: Install the SDK and capture the env-setup script + sysroot**

Run:
```bash
tmp/deploy/sdk/*-toolchain-*.sh -y -d /opt/sc598-sdk
ls /opt/sc598-sdk/environment-setup-* ; ls -d /opt/sc598-sdk/sysroots/*linux*
```
Expected: an `environment-setup-aarch64-...-linux` script and a target sysroot dir (e.g. `/opt/sc598-sdk/sysroots/cortexa55-...-linux`).

- [ ] **Step 3: Record SDK coordinates into `m1-env.md` and commit**

```markdown
- SDK env: /opt/sc598-sdk/environment-setup-aarch64-<...>-linux
- Target sysroot: /opt/sc598-sdk/sysroots/<machine>-<...>-linux
```
```bash
git add docs/superpowers/sc598/m1-env.md && git commit -m "sc598(m1): record Yocto SDK env + target sysroot"
```

---

## M2 — koga cross-config + sysroot-correct build (in the aarch64 VM)

Goal: prove that Clasp built **against the SC598 sysroot** produces a working `boehmprecise`
snapshot, *before* wrapping it in a recipe. Do this inside the `clasp-arm64` VM (M0). Copy the
SDK into the VM (e.g. `limactl copy` the installer, or re-run `populate_sdk` reachable from the VM).

### Task M2.1: Install the SDK in the VM and identify the cross tools

**Files:** none

- [ ] **Step 1: Install the SDK in the VM and source its env**

Run (in the VM):
```bash
limactl shell clasp-arm64 -- bash -lc '/path/to/*-toolchain-*.sh -y -d ~/sc598-sdk; . ~/sc598-sdk/environment-setup-*-linux; echo "CC=$CC"; echo "CXX=$CXX"; echo "SYSROOT=$SDKTARGETSYSROOT"; $CC --version | head -1; ls $SDKTARGETSYSROOT/usr/lib/libLLVM* 2>/dev/null'
```
Expected: `CC`/`CXX` point at the SDK's cross clang, `SDKTARGETSYSROOT` set, and a target `libLLVM` in the sysroot.

- [ ] **Step 2: Locate the SDK's target `llvm-config`**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc '. ~/sc598-sdk/environment-setup-*-linux; find $SDKTARGETSYSROOT -name "llvm-config*" 2>/dev/null; find ~/sc598-sdk -name "llvm-config*" 2>/dev/null | head'
```
Expected: a `llvm-config` that reports target paths. Record its path as `$TLLVMCONFIG`. (If the SDK ships only a native `llvm-config`, you will pass explicit `--cflags/--ldflags` instead — see Step 4 fallback.)

### Task M2.2: Apply the M0 snapshot fix to the build clone

**Files:** Modify: `~/clasp/src/gctools/snapshotSaveLoad.cc` (in the VM, via the patch)

- [ ] **Step 1: Apply the patch (idempotent check first)**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cd ~/clasp && (git apply --check "/Users/frgo/gbt Dropbox/gbt/projects/clasp/docs/superpowers/sc598/patches/0001-snapshot-aarch64-objcopy.patch" 2>/dev/null && git apply "/Users/frgo/gbt Dropbox/gbt/projects/clasp/docs/superpowers/sc598/patches/0001-snapshot-aarch64-objcopy.patch" && echo APPLIED || echo "already-applied-or-skip"); grep -c littleaarch64 src/gctools/snapshotSaveLoad.cc'
```
Expected: `APPLIED` (or `already-applied-or-skip`) and grep count `1`.

### Task M2.3: Write the cross `config.sexp` and configure koga

**Files:** Create: `docs/superpowers/sc598/config.sexp`

- [ ] **Step 1: Write the cross config**

Create `docs/superpowers/sc598/config.sexp` (koga reads a plist of initargs; slots confirmed in M0 at `configure.lisp:387-431`). Replace the three `/opt/...` paths with the SDK values from M2.1:

```lisp
(:build-mode :bytecode
 :default-native nil
 :cc "/opt/sc598-sdk/.../aarch64-...-clang"
 :cxx "/opt/sc598-sdk/.../aarch64-...-clang++"
 :ar "/opt/sc598-sdk/.../llvm-ar"
 :nm "/opt/sc598-sdk/.../llvm-nm"
 :ld :gold
 :llvm-config "/opt/sc598-sdk/.../llvm-config"
 :cflags "--sysroot=/opt/sc598-sdk/sysroots/<machine>-...-linux"
 :cppflags "--sysroot=/opt/sc598-sdk/sysroots/<machine>-...-linux"
 :cxxflags "--sysroot=/opt/sc598-sdk/sysroots/<machine>-...-linux")
```

- [ ] **Step 2: Configure koga with it**

Run (copies the config into the build clone and configures; `lisp`=host sbcl stays the VM's sbcl):
```bash
limactl shell clasp-arm64 -- bash -lc '. ~/sc598-sdk/environment-setup-*-linux; cp "/Users/frgo/gbt Dropbox/gbt/projects/clasp/docs/superpowers/sc598/config.sexp" ~/clasp/config.sexp; cd ~/clasp && ./koga 2>&1 | tail -8; test -f build/build.ninja && echo CONFIGURE-OK'
```
Expected: koga reports the SDK `llvm-config` version (in-range major) and `CONFIGURE-OK`.

- [ ] **Step 3: Commit the config**

```bash
git add docs/superpowers/sc598/config.sexp && git commit -m "sc598(m2): koga cross config against SC598 SDK sysroot"
```

### Task M2.4: Build the boehmprecise snapshot and verify it is sysroot-linked

**Files:** none

- [ ] **Step 1: Build only the precise snapshot**

(`variants` has no koga initarg — M0 — so configure builds all variants but we build just the target.)
Run:
```bash
limactl shell clasp-arm64 -- bash -lc '. ~/sc598-sdk/environment-setup-*-linux; cd ~/clasp && ninja -C build snapshot-boehmprecise 2>&1 | tail -8; echo "RC=${PIPESTATUS[0]}"'
```
Expected: `RC=0`.

- [ ] **Step 2: Verify the produced binary links the TARGET sysroot, not the VM's libs**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'SNAP=~/clasp/build/boehmprecise/snapshot-boehmprecise; file "$SNAP"; readelf -d "$SNAP" | grep -E "NEEDED|RUNPATH"; echo "--- interpreter ---"; readelf -l "$SNAP" | grep interpreter'
```
Expected: `ELF 64-bit … ARM aarch64`; `NEEDED` entries for the target `libLLVM`/`libstdc++`; the interpreter is the standard `/lib/ld-linux-aarch64.so.1` (matches the SC598). This is the proof the image is target-correct.

- [ ] **Step 3: Smoke-run it (still on the VM — same ISA)**

Run:
```bash
limactl shell clasp-arm64 -- bash -lc 'cat > /tmp/smoke.lisp; ~/clasp/build/boehmprecise/snapshot-boehmprecise --norc --non-interactive --load /tmp/smoke.lisp; echo RC=$?' < "/Users/frgo/gbt Dropbox/gbt/projects/clasp/docs/superpowers/sc598/smoke.lisp"
```
Expected: `smoke-ok`, `RC=0`. (Runs on the VM because VM and SC598 are the same aarch64 ISA; the sysroot libs differ but are ABI-compatible for execution here — the real on-device run is M4.)

- [ ] **Step 4: Record M2 result**

Append to `m1-env.md` the confirmed cross-build command + `readelf` NEEDED list; commit:
```bash
git add docs/superpowers/sc598/m1-env.md && git commit -m "sc598(m2): sysroot-correct boehmprecise snapshot builds + verified"
```

---

## M3 — `meta-clasp` bitbake recipe

Author the recipe that performs M2 under bitbake and packages the result. Create the layer in your Yocto layers dir; keep a reference copy in `docs/superpowers/sc598/meta-clasp/`.

### Task M3.1: Create the layer skeleton

**Files:** Create: `meta-clasp/conf/layer.conf`

- [ ] **Step 1: Write `layer.conf`**

```
BBPATH .= ":${LAYERDIR}"
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb ${LAYERDIR}/recipes-*/*/*.bbappend"
BBFILE_COLLECTIONS += "clasp"
BBFILE_PATTERN_clasp = "^${LAYERDIR}/"
BBFILE_PRIORITY_clasp = "10"
LAYERSERIES_COMPAT_clasp = "kirkstone scarthgap styhead walnascar"
LAYERDEPENDS_clasp = "clang-layer"
```

- [ ] **Step 2: Add the layer and confirm bitbake sees it**

Run:
```bash
bitbake-layers add-layer ../layers/meta-clasp
bitbake-layers show-layers | grep clasp
```
Expected: `clasp` listed.

### Task M3.2: Stage the patch and cross config into the recipe

**Files:**
- Create: `meta-clasp/recipes-devtools/clasp/files/0001-snapshot-aarch64-objcopy.patch` (copy of `docs/.../patches/0001`)
- Create: `meta-clasp/recipes-devtools/clasp/files/config.sexp` (the M2 config, but with paths as recipe vars)

- [ ] **Step 1: Copy the patch in**

```bash
cp "docs/superpowers/sc598/patches/0001-snapshot-aarch64-objcopy.patch" ../layers/meta-clasp/recipes-devtools/clasp/files/
```

- [ ] **Step 2: Write the recipe-templated `config.sexp`**

Create `meta-clasp/recipes-devtools/clasp/files/config.sexp` using bitbake-substituted tokens
(the recipe will `sed` `@CC@` etc. at `do_configure`):
```lisp
(:build-mode :bytecode
 :default-native nil
 :cc "@CC@" :cxx "@CXX@" :ar "@AR@" :nm "@NM@" :ld :gold
 :llvm-config "@LLVM_CONFIG@"
 :cflags "@SYSROOT_FLAG@" :cppflags "@SYSROOT_FLAG@" :cxxflags "@SYSROOT_FLAG@")
```

- [ ] **Step 2b (CONDITIONAL — only if M1.2 recorded `HOST_LISP=provide`):** add `meta-clasp/recipes-devtools/common-lisp/sbcl-native_2.4.bb` building SBCL for the build host (`BBCLASSEXTEND = "native"`), sourced from `https://prdownloads.sourceforge.net/sbcl/sbcl-2.4.0-source.tar.bz2`. Validate: `bitbake sbcl-native` builds.

### Task M3.3: Write the recipe `clasp_git.bb`

**Files:** Create: `meta-clasp/recipes-devtools/clasp/clasp_git.bb`

- [ ] **Step 1: Write the recipe**

Replace `@LLVM_MAJOR@` with the M1.1 value (e.g. `18`).

```bitbake
SUMMARY = "Clasp Common Lisp (bytecode, boehmprecise) for ADSP-SC598"
LICENSE = "LGPL-2.1-or-later & Apache-2.0-with-LLVM-exception"
LIC_FILES_CHKSUM = "file://LICENSE;md5=<fill from repo: md5sum LICENSE>"

SRC_URI = "git://github.com/clasp-developers/clasp.git;branch=main;protocol=https \
           file://0001-snapshot-aarch64-objcopy.patch \
           file://config.sexp"
SRCREV = "${AUTOREV}"
S = "${WORKDIR}/git"

# Target libLLVM/clang (M1.1 major) + a host Common Lisp to run koga/scraper.
DEPENDS = "clang sbcl-native"
RDEPENDS:${PN} = "clang"          # libLLVM at runtime (bytecode mode still links it)

inherit pkgconfig

# Run target iclasp during the bootstrap via the sysroot loader (native on an aarch64 builder).
# On an x86-64 builder, add `qemu` to DEPENDS and prefix with qemuwrapper instead.
do_configure() {
    sed -e "s|@CC@|${WORKDIR}/recipe-sysroot-native/usr/bin/clang|" \
        -e "s|@CXX@|${WORKDIR}/recipe-sysroot-native/usr/bin/clang++|" \
        -e "s|@AR@|llvm-ar|" -e "s|@NM@|llvm-nm|" \
        -e "s|@LLVM_CONFIG@|${STAGING_BINDIR_CROSS}/llvm-config|" \
        -e "s|@SYSROOT_FLAG@|--sysroot=${STAGING_DIR_TARGET}|" \
        "${WORKDIR}/config.sexp" > "${S}/config.sexp"
    cd ${S} && ./koga
}

do_compile() {
    cd ${S} && ninja -C build snapshot-boehmprecise
}

do_install() {
    install -d ${D}${bindir} ${D}${libdir}/clasp
    install -m 0755 ${S}/build/boehmprecise/snapshot-boehmprecise ${D}${bindir}/clasp
    cp -a ${S}/build/boehmprecise/lib/. ${D}${libdir}/clasp/
}

FILES:${PN} = "${bindir}/clasp ${libdir}/clasp"
INSANE_SKIP:${PN} += "already-stripped dev-so"
```

- [ ] **Step 2: Parse-check the recipe**

```bash
bitbake -e clasp | grep -E "^PN=|^DEPENDS=" | head
```
Expected: `PN="clasp"`, DEPENDS includes `clang sbcl-native`. (No fetch yet.)

- [ ] **Step 3: Build the recipe**

```bash
bitbake clasp
```
Expected: completes; `tmp/work/*/clasp/*/image${bindir}/clasp` exists. If `do_compile`'s bootstrap (`make-snapshot`) cannot run the target binary, see Step 4.

- [ ] **Step 4 (if the bootstrap can't exec the target binary): enable the loader**

On an **aarch64 build host**, run the target binary natively by prefixing the snapshot rule's
`$clasp` with the sysroot loader; add to the recipe:
```bitbake
export CLASP_RUN_PREFIX = "${STAGING_DIR_TARGET}${base_libdir}/ld-linux-aarch64.so.1 --library-path ${STAGING_DIR_TARGET}${libdir}:${STAGING_DIR_TARGET}${base_libdir}"
```
and patch koga's `make-snapshot`/`generate-lisp-info`/`compile-systems` rule commands to prefix
`$clasp` with `${CLASP_RUN_PREFIX}` (a one-line koga change to `src/koga/ninja.lisp` rule
emission — record it as `patches/0002-koga-run-prefix.patch`). On an **x86-64** host, instead add
`qemu` to `DEPENDS` and set the prefix to `qemu-aarch64 -L ${STAGING_DIR_TARGET}`.
Re-run `bitbake clasp`; expected: completes.

- [ ] **Step 5: Commit the recipe (reference copy in this repo)**

```bash
mkdir -p docs/superpowers/sc598/meta-clasp
cp -a ../layers/meta-clasp/. docs/superpowers/sc598/meta-clasp/
git add docs/superpowers/sc598/meta-clasp && git commit -m "sc598(m3): meta-clasp recipe builds Clasp from source"
```

---

## M4 — Image integration & boot

### Task M4.1: Add Clasp to the SC598 image

**Files:** Modify: your image recipe or `conf/local.conf`

- [ ] **Step 1: Install clasp into the image**

Append to `conf/local.conf`:
```
IMAGE_INSTALL:append = " clasp"
```

- [ ] **Step 2: Build the image**

```bash
bitbake <sc598-image>
```
Expected: completes; the rootfs contains `${bindir}/clasp` and `${libdir}/clasp`.

### Task M4.2: Boot and verify on the SC598

**Files:** none

- [ ] **Step 1: Flash/boot the image on the board** (per your ADI boot flow — SD/eMMC/TFTP).

- [ ] **Step 2: Confirm clasp runs on the board**

On the SC598 console:
```bash
clasp --norc --non-interactive --eval '(progn (format t "fib=~a board=~a~%" (let ((f (lambda (n) (if (< n 2) n 0)))) (funcall f 5)) (machine-type)) (core:exit 0))'
```
Expected: prints `fib=...` and the aarch64 machine type, exits 0. (If it fails to find its image, set `CLASP_HOME`/pass `--snapshot ${libdir}/clasp/...`.)

---

## M5 — On-device validation

### Task M5.1: Functional + bytecode-on-device

- [ ] **Step 1: REPL + bytecode compile/load on the board**

Copy `docs/superpowers/sc598/smoke.lisp` to the board and run:
```bash
clasp --norc --non-interactive --load smoke.lisp
```
Expected: `clasp-in-features: T`, `fib(25)=75025`, `compiled-square-7=49`, `smoke-ok`.
Then test `compile-file` to bytecode on-device:
```bash
echo '(defun sq (x) (* x x))' > /tmp/m.lisp
clasp --norc --non-interactive --eval '(progn (compile-file "/tmp/m.lisp" :output-file "/tmp/m.fasl") (load "/tmp/m.fasl") (format t "sq8=~a~%" (sq 8)) (core:exit 0))'
```
Expected: `sq8=64` (proves bytecode compile-on-device).

### Task M5.2: Footprint + resource behavior (the 512 MB–1 GB budget)

- [ ] **Step 1: Measure resident footprint and image size**

On the board:
```bash
ls -la $(command -v clasp); ls -la /usr/lib/clasp | head
( clasp --norc --non-interactive --eval '(progn (sleep 2)(core:exit 0))' & p=$!; sleep 1; grep VmRSS /proc/$p/status; wait )
free -m
```
Record: binary size, `/usr/lib/clasp` size, idle `VmRSS`, free RAM. Compare to the budget (M0 saw a ~212 MB executable / ~165 MB snapshot — expect RSS in the low hundreds of MB).

- [ ] **Step 2: GC + stress under constrained RAM**

```bash
clasp --norc --non-interactive --eval '(progn (dotimes (i 2000000) (cons i nil)) (gctools:garbage-collect) (format t "gc-ok rss-after~%") (core:exit 0))'
grep VmHWM /proc/self/status 2>/dev/null || true
```
Expected: completes without OOM; note peak. If it OOMs at 512 MB, apply footprint levers (smaller image / trim libLLVM — spec §4.2).

### Task M5.3: Platform-specific checks

- [ ] **Step 1: FP exceptions, backtrace, signals**

```bash
clasp --norc --non-interactive --eval '(handler-case (/ 1.0 0.0) (error (e) (format t "fpe-trapped: ~a~%" e)) (:no-error (v) (format t "no-trap: ~a~%" v)))'
clasp --norc --non-interactive --eval '(handler-case (error "boom") (error (e) (clasp-debug:print-backtrace) (core:exit 0)))' 2>&1 | head -5
```
Expected: FP behavior is consistent (glibc path); a backtrace prints without crashing (validates the Linux-aarch64 libunwind path).

- [ ] **Step 2: Record M5 results + final go/no-go**

Create `docs/superpowers/sc598/m5-device.md` with: board boot status, smoke/bytecode results, footprint
numbers vs budget, GC/FPE/backtrace status, and a deploy recommendation (executable vs snapshot-file).
Commit.

---

## Self-Review

**Spec coverage (against the design doc):**
- "Full Yocto from-source recipe" → M3. ✓
- "Build natively on aarch64, link against target sysroot" → M2 (SDK sysroot) + M3 (`STAGING_DIR_TARGET`). ✓
- "LLVM provisioning, majors 15–20/22, Yocto-release-dependent" → M1.1 (branch incl. kirkstone/21 fallback). ✓
- "Precise GC variant for the deployable image" → M2.4/M3 (`snapshot-boehmprecise`). ✓
- "Bytecode-compile-on-device" → `:default-native nil` (M2.3) + M5.1 verify. ✓
- "Bootstrap runs target binary via sysroot loader; host-SBCL bytecode" → M3.3 Step 4 (`CLASP_RUN_PREFIX`). ✓
- "Snapshot objcopy fix" → M2.2 + M3.2 (patch in recipe). ✓
- "Footprint within 512 MB–1 GB" → M5.2. ✓
- "FPE/backtrace/glibc" → M5.3. ✓
- "sbcl-native dependency" → M1.2 + M3.2b. ✓

**Placeholder scan:** Angle-bracket/`@TOKEN@` items are recipe substitutions resolved by named M1 tasks or `sed` in `do_configure`, and `<sc598-image>`/paths are environment values the engineer supplies — not unfinished plan content. `LIC_FILES_CHKSUM` md5 has an explicit "fill from `md5sum LICENSE`" command. No "TBD/handle appropriately".

**Consistency:** the build target is `snapshot-boehmprecise` in M2.4, M3.3, and self-review; `:default-native nil` and `:build-mode :bytecode` identical in M2.3 and M3.2; LLVM-major handling identical in M1.1 and M3.3. The patch name `0001-snapshot-aarch64-objcopy.patch` matches the M0 artifact.

## Out of scope
- Removing the runtime libLLVM dependency (the ~10k-line decoupling) — only if M5.2 shows 512 MB can't fit.
- Native JIT / running Lisp on the SHARC+ DSP cores.
- Upstreaming the koga `CLASP_RUN_PREFIX` change and the snapshot fix (separate PRs).
