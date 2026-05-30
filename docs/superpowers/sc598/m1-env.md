# M1 — SC598 Yocto environment

## M1.1 — release + LLVM  (RESOLVED)

- **Release:** scarthgap (Yocto 5.0 LTS)
- **meta-clang:** confirm present in `conf/bblayers.conf` (scarthgap branch)
- **LLVM major:** **18** (scarthgap meta-clang default; confirm exactly with `bitbake -e clang | grep '^PV='`)
- **LLVM_OK: YES** — 18 is in koga's accepted range (15–20, 22). Use meta-clang's LLVM directly;
  **no custom LLVM recipe needed** (no kirkstone/LLVM-14 or LLVM-21 workaround).
- **Bonus:** matches the M0 build host (Ubuntu 24.04, clang/llvm **18.1.3**) → minimal `libLLVM`
  ABI skew between builder and target, best-case for snapshot relocation.

## M1.2 — host Common Lisp  (PENDING — run in your Yocto tree)

- Check: `bitbake-layers show-recipes 'sbcl*'` and `'ecl*'`
- **Expectation:** scarthgap's standard layers have **no** sbcl/ecl recipe. **Recommended approach:**
  run koga + the scraper with the **build host's** sbcl via `HOSTTOOLS += "sbcl"` (the `clasp-arm64`
  builder already has sbcl from M0). Avoid authoring `sbcl-native`: SBCL needs an existing Common
  Lisp to bootstrap its build, which is awkward under bitbake. Record the actual `show-recipes` result.

## M1.3 — Yocto SDK  (PENDING — run in your Yocto tree)

- `bitbake <sc598-image> -c populate_sdk`  → installer under `tmp/deploy/sdk/`
- Install: `tmp/deploy/sdk/*-toolchain-*.sh -y -d /opt/sc598-sdk`
- Record:
  - SDK env-setup: `/opt/sc598-sdk/environment-setup-aarch64-...-linux`
  - Target sysroot: `/opt/sc598-sdk/sysroots/<machine>-...-linux`
  - Cross `llvm-config`: `find /opt/sc598-sdk -name 'llvm-config*'`
