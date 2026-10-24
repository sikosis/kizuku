#!/bin/sh

set -eu

test_directory="${TMPDIR:-/tmp}/kizuku-package-test-$$"
fake_bin="$test_directory/bin"
output_directory="$test_directory/output"
mkdir -p "$fake_bin" "$output_directory"
trap 'rm -rf "$test_directory"' EXIT

cat > "$fake_bin/package" <<'EOF'
#!/bin/sh
set -eu
test "$1" = create
test -x bin/kizuku
test -f documentation/packages/kizuku/README.md
test -f documentation/packages/kizuku/LICENSE
cp .PackageInfo "$2.PackageInfo"
: > "$2"
EOF
chmod +x "$fake_bin/package"

cat > "$fake_bin/getarch" <<'EOF'
#!/bin/sh
test "$1" = -p
printf '%s\n' x86_64
EOF
chmod +x "$fake_bin/getarch"

PATH="$fake_bin:$PATH" OUTPUT_DIRECTORY="$output_directory" KIZUKU_SKIP_TESTS=1 \
    ./scripts/build_hpkg.sh >/dev/null

version=$(tr -d '\r\n' < VERSION)
package_path="$output_directory/kizuku-$version-1-x86_64.hpkg"
test -f "$package_path"
test -f "$package_path.sha256"
test -f "$package_path.PackageInfo"
(cd "$output_directory" && sha256sum -c "$(basename "$package_path").sha256" >/dev/null)
grep -Fq 'name          kizuku' "$package_path.PackageInfo"
grep -Fq "version       $version-1" "$package_path.PackageInfo"
grep -Fq 'architecture  x86_64' "$package_path.PackageInfo"
grep -Fq 'cmd:kizuku' "$package_path.PackageInfo"
grep -Fq 'lib:libstdc++' "$package_path.PackageInfo"

echo "package script test passed"
