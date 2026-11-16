#!/usr/bin/env bash
# Build the Debian package from the current checkout. Output: build/deb/*.deb
# Run inside a container prepared by .github/containers/debian.Dockerfile.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
eval "$("$root/.github/scripts/package-version.sh")"

work="$root/build/deb"
src="$work/vacuum-im-$VACUUM_VERSION"
rm -rf "$work"
mkdir -p "$src"
git -C "$root" archive HEAD | tar -x -C "$src"
cp -r "$root/src/packages/linux/debian" "$src/debian"

codename=$(. /etc/os-release && echo "${VERSION_CODENAME:-unstable}")
cat > "$src/debian/changelog" <<CHANGELOG
vacuum-im ($VACUUM_DEB_VERSION) $codename; urgency=medium

  * Automated build of commit $VACUUM_GIT_HASH.

 -- protomors <protomors@users.noreply.github.com>  $(date -R -u -d "@$VACUUM_GIT_DATE")
CHANGELOG

cd "$src"
dpkg-checkbuilddeps
# One package per build: skip the separate -dbgsym package.
export DEB_BUILD_OPTIONS="noautodbgsym ${DEB_BUILD_OPTIONS:-}"
VACUUM_CMAKE_ARGS="-DGIT_HASH=$VACUUM_GIT_HASH -DGIT_DATE=$VACUUM_GIT_DATE" \
    dpkg-buildpackage -b -us -uc -j"${VACUUM_JOBS:-$(nproc)}"
ls -l "$work"/*.deb
