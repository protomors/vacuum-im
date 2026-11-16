#!/usr/bin/env bash
# Build the Arch Linux package from the current checkout. Output: build/arch/*.pkg.tar.zst
# Run as a non-root user inside the container from .github/containers/linux.Dockerfile.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
eval "$("$root/.github/scripts/package-version.sh")"

work="$root/build/arch"
rm -rf "$work"
mkdir -p "$work"
sed "s/^pkgver=.*/pkgver=$VACUUM_ARCH_PKGVER/" \
    "$root/src/packages/linux/arch/PKGBUILD" > "$work/PKGBUILD"
git -C "$root" archive --prefix="vacuum-im-$VACUUM_ARCH_PKGVER/" \
    -o "$work/vacuum-im-$VACUUM_ARCH_PKGVER.tar.gz" HEAD

cd "$work"
export VACUUM_GIT_HASH VACUUM_GIT_DATE
export MAKEFLAGS="-j${VACUUM_JOBS:-$(nproc)}"
# One package per build: skip the separate -debug package.
{ cat /etc/makepkg.conf; echo 'OPTIONS+=(!debug)'; } > "$work/makepkg.conf"
makepkg --config "$work/makepkg.conf" --cleanbuild --noconfirm
ls -l "$work"/*.pkg.tar.zst
