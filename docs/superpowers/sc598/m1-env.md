# M1 — SC598 Yocto environment

## M1.1 — release + LLVM  (RESOLVED, verified against the ADI manifest)

- **Release:** scarthgap — **confirmed**: ADI's `lnxdsp-repo-manifest` **`main`** branch (the repo
  default) pins poky + meta-openembedded at *scarthgap* revisions.
- **⚠ Manifest-branch trap:** ADI's `yocto_5` / `dev` / `update-yocto-5` branches are **Kirkstone**
  (Yocto 4.0 → glibc 2.35, meta-clang LLVM 14 = **below koga's min of 15**). Despite the name,
  "yocto_5" = ADI *product* 5.0.x on Kirkstone. **Use `main` (Scarthgap), not `yocto_5`.**
- **meta-clang:** **NOT in ADI's manifest** — LLVM is not in the base BSP; we add `meta-clang`
  (scarthgap branch) ourselves to get a target `libLLVM`.
- **LLVM major:** **18** on Scarthgap meta-clang (in koga's 15–20/22 range; no custom recipe).
- **ABI match:** Scarthgap = glibc 2.39 / gcc-13 libstdc++ / LLVM 18, identical to the M0 host
  (Ubuntu 24.04) — best-case for snapshot relocation.
- **Host-arch reality:** ADI's BSP is **x86_64-host**. Build the SDK on an x86_64 box but set
  **`SDKMACHINE = "aarch64"`** so the produced SDK is aarch64-hosted and usable in the
  `clasp-arm64` VM for M2. Full runbook: `build-sdk-x86_64.md`.

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
