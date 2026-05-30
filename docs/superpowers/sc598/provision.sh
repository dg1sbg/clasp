#!/usr/bin/env bash
# Install Clasp's Linux build dependencies inside the aarch64 VM.
# Mirrors .github/workflows/test.yml (Ubuntu, clang/llvm-18 -- accepted by koga).
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

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
