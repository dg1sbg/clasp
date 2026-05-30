# Build the SC598 Yocto SDK (aarch64-hosted) on an x86_64 host

**Goal:** produce a Yocto **SDK** for the ADSP-SC598 Scarthgap image whose **target sysroot
includes `libLLVM`-18** (for Clasp), and whose **host side is aarch64** (`SDKMACHINE = "aarch64"`)
so it installs and runs inside the `clasp-arm64` VM for M2.

**Why x86_64:** ADI's BSP is x86_64-host only. `SDKMACHINE = "aarch64"` lets an x86_64 *build*
emit an aarch64-*hosted* SDK — so the heavy build runs where ADI supports it, but the SDK is
usable on your Apple-Silicon VM with **no qemu** in the Clasp build.

**Two entry points:**
- **A. You already have a Scarthgap SC598 build tree** (the common case — you said you're on
  Scarthgap). Skip to **§3** (add clang + SDKMACHINE, then `populate_sdk`).
- **B. From scratch.** Do §1 → §3.

> ⚠ **Branch:** use ADI manifest **`main`** (Scarthgap). The `yocto_5`/`dev`/`update-yocto-5`
> branches are **Kirkstone** (glibc 2.35, LLVM 14 — rejected by koga). Don't use them.

**Host:** x86_64 Ubuntu 22.04 or 24.04, **≥150 GB** free disk, **≥16 GB** RAM, 8+ vCPU.
Cloud sizing: e.g. AWS `m7i.4xlarge` / GCP `c3-standard-16`, Ubuntu 22.04, 200 GB disk —
a few hours, a few dollars. The `meta-clang` LLVM-18 build is the long pole.

---

## §1 — Host prerequisites + `repo` (from-scratch only)

```bash
sudo apt-get update
sudo apt-get install -y gawk wget git diffstat unzip texinfo gcc build-essential chrpath \
  socat cpio python3 python3-pip python3-pexpect xz-utils debianutils iputils-ping \
  python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev xterm python3-subunit mesa-common-dev \
  zstd liblz4-tool file locales libacl1
sudo locale-gen en_US.UTF-8

mkdir -p ~/bin
curl -o ~/bin/repo https://storage.googleapis.com/git-repo-downloads/repo
chmod a+x ~/bin/repo
export PATH=~/bin:$PATH
```

## §2 — Fetch the Scarthgap BSP (from-scratch only)

```bash
mkdir -p ~/sc598-yocto && cd ~/sc598-yocto
repo init -u https://github.com/analogdevicesinc/lnxdsp-repo-manifest.git -b main -m main.xml
repo sync -j"$(nproc)"
```
Verify Scarthgap (sanity):
```bash
grep -i scarthgap sources/lnxdsp-repo-manifest/main.xml   # expect the scarthgap-revision comments
```

## §3 — Add meta-clang, configure for Clasp, build the SDK

**3a. Clone meta-clang (Scarthgap) into the sources tree:**
```bash
cd ~/sc598-yocto    # or your existing build tree's root
git clone -b scarthgap https://github.com/kraj/meta-clang.git sources/meta-clang
```

**3b. Initialise the build env for the SC598 machine** (creates `build/conf/`):
```bash
source sources/setup-environment -m adsp-sc598-som-ezkit
# (now in the build/ dir)
```

**3c. Add the meta-clang layer:**
```bash
bitbake-layers add-layer ../sources/meta-clang
bitbake-layers show-layers | grep clang   # confirm it's active
```

**3d. Configure `conf/local.conf`** — append:
```
# --- Clasp SC598 SDK additions ---
# Emit an aarch64-HOSTED SDK from this x86_64 build, so it runs in the clasp-arm64 VM:
SDKMACHINE = "aarch64"

# Put clang + libLLVM (and dev headers / llvm-config) in BOTH the image and the SDK sysroot,
# so koga can find a target llvm-config and Clasp can link libLLVM-18:
IMAGE_INSTALL:append = " clang"
TOOLCHAIN_TARGET_TASK:append = " clang clang-dev"

# meta-clang on scarthgap defaults to LLVM 18; pin explicitly to be safe (koga needs 15-20/22):
PREFERRED_VERSION_clang = "18.%"

# Faster/idempotent rebuilds:
BB_NUMBER_THREADS = "${@oe.utils.cpu_count()}"
PARALLEL_MAKE = "-j ${@oe.utils.cpu_count()}"
```

**3e. (Optional) confirm the LLVM version that will be built:**
```bash
bitbake -e clang | grep '^PV='     # expect PV="18.x.x"
```

**3f. Build the image, then the SDK** (the long step — hours; `meta-clang` builds LLVM from source):
```bash
bitbake adsp-sc5xx-minimal
bitbake adsp-sc5xx-minimal -c populate_sdk
```

**3g. Collect the SDK installer** (it is **aarch64-hosted** because `SDKMACHINE=aarch64`):
```bash
ls -la tmp/deploy/sdk/*toolchain*.sh
file tmp/deploy/sdk/*toolchain*.sh   # the embedded payload targets aarch64 host
```

---

## §4 — Hand off to M2 (in the `clasp-arm64` VM)

Copy the installer to the Mac, then into the VM, install it, and read back the paths M2 needs:
```bash
# on the Mac (scp/limactl copy the .sh into the VM), then in the VM:
limactl shell clasp-arm64 -- bash -lc '~/sc598-toolchain.sh -y -d ~/sc598-sdk
. ~/sc598-sdk/environment-setup-*-linux
echo "CC=$CC"; echo "CXX=$CXX"; echo "SYSROOT=$SDKTARGETSYSROOT"
find ~/sc598-sdk -name "llvm-config*"
ls $SDKTARGETSYSROOT/usr/lib/libLLVM* 2>/dev/null'
```
Paste those (`CC`, `CXX`, `SDKTARGETSYSROOT`, the cross `llvm-config`, and the target `libLLVM`)
back — they fill M2's `config.sexp` (see the M1–M5 plan, Task M2.3).

---

## Notes & caveats

- **Match your board image.** For an exactly-correct sysroot, run `populate_sdk` from the *same*
  build state that produced the rootfs on your board (entry point A), not a drifted fresh `main`.
- **If `clang-dev`/`llvm-config` packaging differs**, adjust `TOOLCHAIN_TARGET_TASK` (meta-clang
  package names occasionally shift across branches). Confirm the SDK sysroot has `usr/bin/llvm-config`
  and `usr/lib/libLLVM*` after §3f.
- **Footprint on the board:** `libLLVM-18` is large; ensure the SC598 image (not just the SDK) ships
  it plus `libfmt`, `libgmp`, `libunwind`, `libelf` (Clasp runtime deps) — handled by the
  `IMAGE_INSTALL` / the M3 recipe's `RDEPENDS`.
- **Shortcut sanity check:** Scarthgap ≈ Ubuntu 24.04 (glibc 2.39 / LLVM 18). The M0 snapshot may
  already load on the board; if you get board access before this SDK finishes, that's the cheapest
  validation.
