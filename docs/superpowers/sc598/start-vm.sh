#!/usr/bin/env bash
# Bring up an aarch64 Linux build host for the Clasp SC598 port.
# On Apple Silicon, Lima defaults to arch=aarch64 + vmType=vz (native virtualization,
# no emulation), so the build (and later the recipe's image bootstrap) runs natively.
#
# template://ubuntu-lts == Ubuntu 24.04 LTS (aarch64), which carries clang/llvm-18.
# koga accepts LLVM majors 15-20 or 22 (NOT 21); the default `ubuntu` template now
# tracks 25.10 (newer LLVM, 21-risk), so we pin the LTS here.
set -euo pipefail

NAME="${1:-clasp-arm64}"

limactl start \
  --name="${NAME}" \
  --cpus=8 \
  --memory=16 \
  --disk=100 \
  --tty=false \
  template://ubuntu-lts

echo "VM '${NAME}' started. Shell in with:  limactl shell ${NAME}"
echo "Verify:  limactl shell ${NAME} -- bash -lc 'uname -m; getconf PAGESIZE'"
