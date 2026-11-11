#!/usr/bin/env bash
# Run the Kirin 990 compilation in the *published* reusable Rust/Mesa
# image. The exact legacy NDK r28c is mounted read-only from the host.
set -euo pipefail
: "${GITHUB_WORKSPACE:?Run this only from GitHub Actions}"
: "${ANDROID_HOME:?Android SDK not configured}"
IMAGE="ghcr.io/dissidius/panvk-h42-kirin990-builder:llvm18-rust199-v1"
NDK="$ANDROID_HOME/ndk/28.2.13676358"
test -x "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang" || {
  echo "H42: r28c NDK not present; refusing to switch the known-working toolchain" >&2
  exit 2
}
mkdir -p "$GITHUB_WORKSPACE/.h42-cargo" "$GITHUB_WORKSPACE/.ci-ccache" "$GITHUB_WORKSPACE/.ci-home"
if [ -n "${H42_GHCR_TOKEN:-}" ]; then
  printf '%s' "$H42_GHCR_TOKEN" |
    docker login ghcr.io -u "${GITHUB_ACTOR:-dissidius}" --password-stdin >/dev/null
fi
echo "H42: pulling verified Rust/Mesa image; no runner apt or pip setup"
docker pull "$IMAGE"
exec docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$GITHUB_WORKSPACE:$GITHUB_WORKSPACE" \
  -v "$NDK:$NDK:ro" \
  -w "$GITHUB_WORKSPACE" \
  -e HOME="$GITHUB_WORKSPACE/.ci-home" \
  -e GITHUB_WORKSPACE="$GITHUB_WORKSPACE" \
  -e ANDROID_NDK_ROOT="$NDK" \
  -e H42_NDK_ROOT="$NDK" \
  -e CARGO_HOME="$GITHUB_WORKSPACE/.h42-cargo" \
  -e RUSTUP_HOME=/usr/local/rustup \
  -e CCACHE_DIR="$GITHUB_WORKSPACE/.ci-ccache" \
  -e CCACHE_BASEDIR="$GITHUB_WORKSPACE" \
  -e CCACHE_COMPILERCHECK=content \
  -e CCACHE_NOHASHDIR=true \
  -e LIBCLANG_PATH=/usr/lib/llvm-18/lib \
  -e CLANG_PATH=/usr/bin/clang-18 \
  -e PANVK_JOBS=4 \
  "$IMAGE" bash ci/h42-build-rotation-universal.sh
