# Shared helpers for omarchium scripts. Source, don't execute.
set -euo pipefail
shopt -s extglob

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=../omarchium.conf
source "$ROOT/omarchium.conf"

SRC=$ROOT/chromium/src        # the Chromium tree (its own git repo, gitignored here)
CACHE=$ROOT/.cache             # tarballs, crubit, Arch packaging clone
OUT=${OUT:-$SRC/out/Release}
CORE=$ROOT/core                # our code; appears in the tree as //omarchium

# Build with the distro toolchain, never mise/asdf shims that may shadow it.
export PATH=/usr/bin:$PATH
export RUSTUP_TOOLCHAIN=$RUST_TOOLCHAIN
export RUSTC_BOOTSTRAP=1       # Chromium uses a few nightly rustc features

log()  { printf '\e[1;34m==>\e[0m %s\n' "$*"; }
warn() { printf '\e[1;33m==>\e[0m %s\n' "$*" >&2; }
die()  { printf '\e[1;31m!!\e[0m %s\n' "$*" >&2; exit 1; }
cgit() { git -C "$SRC" -c core.quotepath=off "$@"; }

tarball_path() { echo "$CACHE/chromium-${1:-$CHROMIUM_VERSION}-lite.tar.xz"; }

# Patch file name for a path inside the Chromium tree (Brave's convention:
# one patch per upstream file, slashes become dashes).
patch_name_for() { echo "${1//\//-}.patch"; }
