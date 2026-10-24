#!/bin/sh

set -eu

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$script_directory"

for required_command in make package sha256sum; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "error: $required_command is required to build the HPKG" >&2
        exit 1
    fi
done

version=$(tr -d '\r\n' < VERSION)
revision=${PACKAGE_REVISION:-1}
case "$revision" in
    ''|*[!0-9]*|0)
        echo "error: PACKAGE_REVISION must be a positive integer" >&2
        exit 1
        ;;
esac

architecture=${PACKAGE_ARCHITECTURE:-}
if [ -z "$architecture" ]; then
    if ! command -v getarch >/dev/null 2>&1; then
        echo "error: getarch is required; run this script on Haiku or set PACKAGE_ARCHITECTURE" >&2
        exit 1
    fi
    architecture=$(getarch -p)
fi
case "$architecture" in
    x86|x86_gcc2|x86_64|arm|arm64|m68k|ppc|riscv64|sparc)
        ;;
    *)
        echo "error: unsupported Haiku package architecture: $architecture" >&2
        exit 1
        ;;
esac

output_directory=${OUTPUT_DIRECTORY:-"$script_directory/dist"}
mkdir -p "$output_directory"
output_directory=$(CDPATH= cd -- "$output_directory" && pwd)
package_name="kizuku-$version-$revision-$architecture.hpkg"
package_path="$output_directory/$package_name"

echo "==> Building Kizuku v$version"
make clean
make
if [ "${KIZUKU_SKIP_TESTS:-0}" != "1" ]; then
    make test
fi

staging_directory=$(mktemp -d "${TMPDIR:-/tmp}/kizuku-hpkg.XXXXXX")
trap 'rm -rf "$staging_directory"' EXIT HUP INT TERM

mkdir -p "$staging_directory/bin"
mkdir -p "$staging_directory/documentation/packages/kizuku"
install -m 755 kizuku "$staging_directory/bin/kizuku"
install -m 644 README.md "$staging_directory/documentation/packages/kizuku/README.md"
install -m 644 LICENSE "$staging_directory/documentation/packages/kizuku/LICENSE"

cat > "$staging_directory/.PackageInfo" <<EOF
name          kizuku
version       $version-$revision
architecture  $architecture
summary       "Interactive script builder and editor for Haiku"
description   "Kizuku creates, edits, and restyles interactive shell build scripts, using Hum when it is available."
vendor        "Sikosis"
packager      "${PACKAGER:-phil@sikosis.com}"
copyrights    { "2026 Sikosis" }
licenses      { "MIT" }
provides {
    kizuku = $version
    cmd:kizuku = $version
}
requires {
    haiku
    lib:libstdc++
}
EOF

echo "==> Creating $package_name"
(
    cd "$staging_directory"
    package create "$package_path"
)

(
    cd "$output_directory"
    sha256sum "$package_name" > "$package_name.sha256"
)

echo "Created: $package_path"
echo "Checksum: $package_path.sha256"
