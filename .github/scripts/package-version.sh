#!/usr/bin/env bash
# Print package version variables (KEY=value lines) for distribution packages.
#   VACUUM_RELEASE=1  -> release version, e.g. 1.3.0~alpha (Debian) / 1.3.0alpha (Arch)
#   otherwise         -> snapshot version with commit date and hash appended
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
header="$root/src/definitions/version.h"

version=$(sed -nE 's/^#define[[:space:]]+CLIENT_VERSION[[:space:]]+"([0-9.]+)".*/\1/p' "$header")
suffix=$(sed -nE 's/^#define[[:space:]]+CLIENT_VERSION_SUFFIX[[:space:]]+"([^"]*)".*/\1/p' "$header" \
    | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9')
[[ -n $version ]] || { echo "Cannot read CLIENT_VERSION from $header" >&2; exit 1; }

hash=$(git -C "$root" rev-parse HEAD)
date=$(git -C "$root" log -1 --format=%ct)

deb="$version${suffix:+~$suffix}"
arch="$version$suffix"
if [[ ${VACUUM_RELEASE:-0} != 1 ]]; then
    stamp=$(date -u -d "@$date" +%Y%m%d)
    deb+="+git$stamp.${hash:0:8}"
    arch+=".r$stamp.g${hash:0:8}"
fi

echo "VACUUM_VERSION=$version"
echo "VACUUM_GIT_HASH=$hash"
echo "VACUUM_GIT_DATE=$date"
echo "VACUUM_DEB_VERSION=$deb-1"
echo "VACUUM_ARCH_PKGVER=$arch"
