#!/usr/bin/env bash
# WORKS with patch 0002 (snapshot-relocation-basename-match) applied to libclasp — see
# deploy-findings.md. Without 0002, Clasp's snapshot load matches libraries by save-time path
# and aborts when the libs move into ./lib; with 0002 it matches by basename -> relocatable.
# Target runtime requirements: `nm` (binutils) on PATH + glibc >= the build's (Scarthgap 2.39).
#
# Build a self-contained Clasp deploy bundle for the ADSP-SC598 (aarch64 Scarthgap).
# Run INSIDE the clasp-arm64 VM. Produces ~/clasp-sc598-bundle.tar.gz:
#   clasp         - the boehmprecise standalone snapshot executable (embedded image)
#   lib/          - non-glibc .so deps (libLLVM-18, libstdc++, libfmt, ...) from Ubuntu 24.04 (~Scarthgap)
#   run-clasp.sh  - sets LD_LIBRARY_PATH=./lib and runs clasp (uses the BOARD's glibc)
#   smoke.lisp    - quick functional check
# Rationale: Ubuntu 24.04 == Scarthgap on glibc 2.39 / LLVM 18 / gcc-13 libstdc++, so the
# board's glibc satisfies the app; we carry only the non-glibc libraries.
set -euo pipefail

SNAP="${1:-$HOME/clasp/build/boehmprecise/snapshot-boehmprecise}"
OUT="$HOME/clasp-sc598-bundle"
[ -x "$SNAP" ] || { echo "ERROR: snapshot not found/executable: $SNAP" >&2; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT/lib"
cp "$SNAP" "$OUT/clasp"

# glibc/system libs the board provides (do NOT bundle - tied to its kernel/loader):
EXCL='^(ld-linux-aarch64|libc|libm|libpthread|libdl|librt|libresolv|libanl|libutil)\.so'

ldd "$SNAP" | while read -r line; do
  case "$line" in *"=>"*) : ;; *) continue ;; esac
  name=${line%% *}
  path=$(printf '%s\n' "$line" | sed -E 's/.*=> *([^ ]+).*/\1/')
  [ -e "$path" ] || continue
  printf '%s' "$name" | grep -qE "$EXCL" && continue
  cp -L "$path" "$OUT/lib/$name"
done

cat > "$OUT/run-clasp.sh" <<'RUN'
#!/bin/sh
# Run bundled Clasp on the SC598. Uses the board's glibc + bundled non-glibc libs.
HERE=$(cd "$(dirname "$0")" && pwd)
exec env LD_LIBRARY_PATH="$HERE/lib" "$HERE/clasp" "$@"
RUN
chmod +x "$OUT/run-clasp.sh" "$OUT/clasp"

cat > "$OUT/smoke.lisp" <<'LISP'
(format t "~&clasp-in-features: ~a~%" (and (member :clasp *features*) t))
(format t "impl: ~a ~a~%" (lisp-implementation-type) (lisp-implementation-version))
(defun fib (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(format t "fib(25)=~a~%" (fib 25))
(let ((sq (compile nil '(lambda (x) (* x x))))) (format t "compiled-square-7=~a~%" (funcall sq 7)))
(format t "smoke-ok~%")
LISP

cat > "$OUT/README.txt" <<'TXT'
Clasp bundle for ADSP-SC598 (aarch64, Yocto Scarthgap).
Built on Ubuntu 24.04 (glibc 2.39 / LLVM 18 / gcc-13 libstdc++ == Scarthgap ABI).

Deploy on the board:
  scp clasp-sc598-bundle.tar.gz <user>@<board>:/opt/
  ssh <user>@<board> 'cd /opt && tar xzf clasp-sc598-bundle.tar.gz'
Run:
  /opt/clasp-sc598-bundle/run-clasp.sh --norc --non-interactive \
      --load /opt/clasp-sc598-bundle/smoke.lisp
Expect: clasp-in-features: T / fib(25)=75025 / compiled-square-7=49 / smoke-ok

If you get a "GLIBC_2.39 not found" style error, the board's glibc is older than
Scarthgap's 2.39 -> the exact-SDK path is then required. Otherwise: this is Clasp
running on the SC598.
TXT

( cd "$OUT/lib" && ls -1 ) > "$OUT/BUNDLED-LIBS.txt"
tar czf "$HOME/clasp-sc598-bundle.tar.gz" -C "$HOME" clasp-sc598-bundle

echo "=== bundle size ==="; du -sh "$OUT"
echo "=== bundled libs ==="; cat "$OUT/BUNDLED-LIBS.txt"
echo "=== tarball ==="; ls -lh "$HOME/clasp-sc598-bundle.tar.gz"
echo "=== sanity: run via the bundle's own launcher (in the VM) ==="
"$OUT/run-clasp.sh" --norc --non-interactive --load "$OUT/smoke.lisp" || echo "LOCAL-RUN-FAILED"
